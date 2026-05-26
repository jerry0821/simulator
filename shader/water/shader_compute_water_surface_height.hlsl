cbuffer WATER_SURFACE_HEIGHT_CONSTANT_BUFFER : register(b0)
{
    float water_surface_height;
    float minimum_depth_for_surface;
    float flow_rate;
    float max_outflow_fraction;
    float evaporation_rate;
    float accumulation_rate;
    float seepage_rate;
    float basin_fade;
    float field_width;
    float field_depth;
    float injection_center_x;
    float injection_center_z;
    float injection_radius;
    float injection_amount;
    float delta_time_seconds;
    float time_seconds;
    float rock_slope_start;
    float rock_slope_end;
    float shoreline_offset_start;
    float shoreline_offset_end;
    float wetness_gain;
    float padding5;
    float padding6;
    float padding7;
    uint width;
    uint height;
    uint initialize_from_water_level;
    uint injection_enabled;
    uint padding1;
    uint padding2;
    uint padding3;
    uint padding4;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D<float2> g_PreviousTerrainWaterHeight : register(t1);
Texture2D<float> g_RainMap : register(t2);
Texture2D<float4> g_PreviousWaterFlow : register(t3);
Texture2D<float4> g_PreviousWaterSediment : register(t4);
Texture2D<float4> g_Meteorograph : register(t5);
SamplerState g_TerrainSampler : register(s0);
RWTexture2D<float2> g_WaterSurfaceHeight : register(u0);
RWTexture2D<float4> g_WaterFlowOut : register(u1);
RWTexture2D<float4> g_WaterVelocityOut : register(u2);
RWTexture2D<float4> g_WaterSedimentOut : register(u3);
RWTexture2D<float4> g_TerrainSurfaceDataOut : register(u4);

static const float kWaterDeltaTime = 1.0f / 60.0f;
static const float kWaterFlowDamping = 0.020f;
static const float kHydrologicalCycleRate = 0.5f;
static const float kWaterBaseHeight = 0.0f;
static const float kDryDepthEpsilon = 0.008f;
static const float kDryWaterSurfaceDrop = 0.25f;
static const float kHydraulicErosionRate = 0.020f;
static const float kHydraulicDepositionRate = 0.016f;
static const float kThermalErosionRate = 0.030f;
static const float kThermalSlopeThreshold = 0.42f;
static const float kMaxHydraulicErosion = 0.020f;
static const float kMaxHydraulicDeposition = 0.014f;
static const float kPolarTemperature = -10.0f;
static const float kEquatorialTemperature = 30.0f;

bool InBounds(int2 coord)
{
    return coord.x >= 0 && coord.y >= 0 && coord.x < int(width) && coord.y < int(height);
}

int2 ClampCoord(int2 coord)
{
    return clamp(coord, int2(0, 0), int2(int(width) - 1, int(height) - 1));
}

bool IsEdgeCell(int2 coord)
{
    return coord.x <= 0 || coord.y <= 0 || coord.x >= int(width) - 1 || coord.y >= int(height) - 1;
}

float2 SafeNormalize(float2 value)
{
    const float len_sq = dot(value, value);
    return len_sq > 1.0e-6f ? value * rsqrt(len_sq) : float2(1.0f, 0.0f);
}

float Smoothstep01(float v)
{
    const float t = saturate(v);
    return t * t * (3.0f - 2.0f * t);
}

float NormalizeClimateTemperature(float temperature_celsius)
{
    return saturate((temperature_celsius - kPolarTemperature) / max(kEquatorialTemperature - kPolarTemperature, 1.0e-4f));
}

float4 ComputeVisibleWaterSample(
    float terrain_center,
    float terrain_left,
    float terrain_right,
    float terrain_up,
    float terrain_down,
    float water_depth,
    float4 flow)
{
    static const float runoff_depth_min = 0.02f;
    static const float visible_depth_min = 0.10f;
    static const float standing_water_min = 0.22f;
    static const float slope_suppress_start = 0.08f;
    static const float slope_suppress_end = 0.34f;
    static const float flow_runoff_scale = 0.85f;
    static const float flow_visibility_suppress = 0.30f;

    const float slope =
        max(abs(terrain_right - terrain_left), abs(terrain_down - terrain_up));
    const float slope_keep =
        1.0f - Smoothstep01(saturate((slope - slope_suppress_start) / max(slope_suppress_end - slope_suppress_start, 1.0e-4f)));

    const float standing_water =
        Smoothstep01(saturate((water_depth - visible_depth_min) / max(standing_water_min - visible_depth_min, 1.0e-4f)));
    const float flow_strength = saturate(dot(flow, 1.0f.xxxx) * 5.0f);

    const float pooled_core =
        Smoothstep01(saturate((standing_water - standing_water_min) / 0.42f));
    const float depth_core =
        Smoothstep01(saturate((water_depth - visible_depth_min) / 0.28f));
    float visible_water =
        max(
            pooled_core * lerp(0.45f, 1.0f, slope_keep),
            depth_core * slope_keep * (1.0f - flow_strength * flow_visibility_suppress * 0.35f));
    visible_water = saturate(visible_water);

    float runoff_hint =
        Smoothstep01(saturate((water_depth - runoff_depth_min) / max(visible_depth_min - runoff_depth_min, 1.0e-4f))) *
        (1.0f - visible_water) *
        max(flow_strength, 0.15f) *
        flow_runoff_scale;
    runoff_hint *= lerp(0.45f, 1.0f, 1.0f - slope_keep);

    const float pooled_preview = saturate(pooled_core);
    const float preview_alpha = saturate(max(visible_water, runoff_hint * 0.75f));
    return float4(visible_water, runoff_hint, pooled_preview, preview_alpha);
}

float4 ComputeWaterMaskSample(
    float4 visible_water,
    float4 surface_left,
    float4 surface_right,
    float4 surface_up,
    float4 surface_down)
{
    static const float shore_band = 0.24f;

    float water_alpha = visible_water.r;
    const float runoff_hint = visible_water.g;
    const float pooled_water = visible_water.b;

    const float visible_left = surface_left.r;
    const float visible_right = surface_right.r;
    const float visible_up = surface_up.r;
    const float visible_down = surface_down.r;

    const float neighbor_average = 0.25f * (visible_left + visible_right + visible_up + visible_down);
    const float edge_soften =
        saturate(lerp(neighbor_average * 0.10f, neighbor_average * 0.32f, water_alpha));
    water_alpha = max(water_alpha, edge_soften * smoothstep(0.10f, 0.42f, water_alpha));

    const float runoff_edge =
        max(max(surface_left.g, surface_right.g), max(surface_up.g, surface_down.g));
    water_alpha = max(water_alpha, runoff_edge * 0.02f);

    const float pooled_neighbor =
        max(pooled_water, max(surface_left.b, max(surface_right.b, max(surface_up.b, surface_down.b))));

    float water_gradient =
        abs(surface_left.r - surface_right.r) +
        abs(surface_up.r - surface_down.r) +
        0.45f * (abs(surface_left.b - surface_right.b) + abs(surface_up.b - surface_down.b));
    float shore_mask = Smoothstep01(saturate(water_gradient / max(shore_band, 1.0e-4f)));
    shore_mask *= smoothstep(0.12f, 0.55f, max(water_alpha, neighbor_average));
    shore_mask *= smoothstep(0.08f, 0.45f, pooled_neighbor);

    const float water_contact = saturate(max(water_alpha, runoff_hint * 0.22f));
    const float shoreline_contact = saturate(shore_mask);
    const float pooled_contact = saturate(pooled_neighbor);
    const float composite_alpha = saturate(max(water_contact, shoreline_contact * 0.04f));

    return float4(
        water_contact,
        shoreline_contact,
        pooled_contact,
        composite_alpha);
}

float SampleBaseTerrainHeight(int2 coord)
{
    uint terrain_width = 0;
    uint terrain_height = 0;
    g_TerrainHeight.GetDimensions(terrain_width, terrain_height);

    const float2 terrain_resolution = max(float2(terrain_width, terrain_height), 1.0f.xx);
    const float2 uv = (float2(ClampCoord(coord)) + 0.5f) / float2(width, height);
    const int2 terrain_coord = int2(saturate(uv) * (terrain_resolution - 1.0f) + 0.5f);
    return g_TerrainHeight.Load(int3(terrain_coord, 0)).r;
}

float2 SamplePreviousStateRaw(int2 coord)
{
    return g_PreviousTerrainWaterHeight.Load(int3(ClampCoord(coord), 0));
}

float SampleWorkingTerrainHeight(int2 coord)
{
    const float base_terrain_height = SampleBaseTerrainHeight(coord);
    const float previous_terrain_height = SamplePreviousStateRaw(coord).x;
    return initialize_from_water_level != 0u ? base_terrain_height : previous_terrain_height;
}

float SamplePreviousSurfaceHeight(int2 coord)
{
    const float terrain_height = SampleWorkingTerrainHeight(coord);
    const float previous_surface = SamplePreviousStateRaw(coord).y;
    return max(previous_surface, terrain_height);
}

float SamplePreviousDepth(int2 coord)
{
    const float terrain_height = SampleWorkingTerrainHeight(coord);
    return max(SamplePreviousSurfaceHeight(coord) - terrain_height, 0.0f);
}

float SampleRainAmount(int2 coord)
{
    const float2 uv = (float2(ClampCoord(coord)) + 0.5f) / float2(width, height);
    return max(g_RainMap.SampleLevel(g_TerrainSampler, uv, 0.0f), 0.0f);
}

float2 ComputeWorldPosition(int2 coord)
{
    float2 uv = (float2(coord) + 0.5f) / float2(width, height);
    return float2(
        (uv.x - 0.5f) * field_width,
        (uv.y - 0.5f) * field_depth);
}

float ComputeBasinFactor(float terrain_height)
{
    const float basin_depth = water_surface_height - terrain_height;
    return saturate((basin_depth + 2.0f) / max(basin_fade, 1.0e-4f));
}

float ComputeInjectedWater(int2 coord)
{
    const float injection_active =
        injection_enabled != 0u && injection_radius > 1.0e-4f && injection_amount > 1.0e-4f
            ? 1.0f
            : 0.0f;
    const float safe_radius = max(injection_radius, 1.0e-4f);
    const float2 world_pos = ComputeWorldPosition(coord);
    const float distance_to_injection =
        length(world_pos - float2(injection_center_x, injection_center_z));
    const float injection_falloff =
        1.0f - smoothstep(safe_radius * 0.30f, safe_radius, distance_to_injection);
    return injection_amount * saturate(injection_falloff) * injection_active;
}

float4 ComputeOutflowFromPreviousState(int2 coord)
{
    const float coord_active = InBounds(coord) ? 1.0f : 0.0f;
    const float water_dt = max(min(delta_time_seconds, kWaterDeltaTime), 1.0e-4f);
    const float4 previous_flow = max(g_PreviousWaterFlow.Load(int3(ClampCoord(coord), 0)), 0.0f.xxxx);
    const float previous_flow_retention = 0.0f;
    const float4 retained_previous_flow = previous_flow * previous_flow_retention;
    const float available_water = SamplePreviousDepth(coord) * coord_active;

    const float center_surface = SamplePreviousSurfaceHeight(coord);
    float4 surface_delta_heights = float4(
        max(center_surface - SamplePreviousSurfaceHeight(coord + int2(1, 0)), 0.0f),
        max(center_surface - SamplePreviousSurfaceHeight(coord + int2(-1, 0)), 0.0f),
        max(center_surface - SamplePreviousSurfaceHeight(coord + int2(0, 1)), 0.0f),
        max(center_surface - SamplePreviousSurfaceHeight(coord + int2(0, -1)), 0.0f));

    const float2 uv = (float2(ClampCoord(coord)) + 0.5f) / float2(width, height);
    const float4 meteorograph_sample = g_Meteorograph.SampleLevel(g_TerrainSampler, uv, 0.0f);

    float4 candidate = max(retained_previous_flow + water_dt * flow_rate * surface_delta_heights, 0.0f.xxxx);
    const float total_candidate = dot(candidate, 1.0f.xxxx);
    const float movable_water = available_water * max_outflow_fraction;
    const float candidate_active = total_candidate > 1.0e-5f ? 1.0f : 0.0f;
    const float scale =
        min((movable_water / max(total_candidate, 1.0e-5f)) * (1.0f - kWaterFlowDamping), 1.0f);
    const float4 no_candidate_outflow =
        max(retained_previous_flow * saturate(1.0f - water_dt * 5.0f), 0.0f.xxxx);
    const float4 wet_outflow = lerp(no_candidate_outflow, candidate * scale, candidate_active);
    const float4 dry_outflow =
        max(retained_previous_flow * saturate(1.0f - water_dt * 6.0f), 0.0f.xxxx);
    const float water_active = available_water > 1.0e-5f ? 1.0f : 0.0f;
    return lerp(dry_outflow, wet_outflow, water_active) * coord_active;
}

float4 ComputeApproxVisibleWaterAtCoord(int2 coord)
{
    const int2 clamped = ClampCoord(coord);
    const int2 left_coord = ClampCoord(clamped + int2(-1, 0));
    const int2 right_coord = ClampCoord(clamped + int2(1, 0));
    const int2 up_coord = ClampCoord(clamped + int2(0, -1));
    const int2 down_coord = ClampCoord(clamped + int2(0, 1));

    return ComputeVisibleWaterSample(
        SampleWorkingTerrainHeight(clamped),
        SampleWorkingTerrainHeight(left_coord),
        SampleWorkingTerrainHeight(right_coord),
        SampleWorkingTerrainHeight(up_coord),
        SampleWorkingTerrainHeight(down_coord),
        SamplePreviousDepth(clamped),
        ComputeOutflowFromPreviousState(clamped));
}

int2 ComputeUpstreamCoord(int2 coord, float2 flow_vector)
{
    const float2 upstream = clamp(flow_vector * 4.0f, -1.0f, 1.0f);
    return ClampCoord(coord - int2((int)round(upstream.x), (int)round(upstream.y)));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const int2 coord = int2(dispatch_thread_id.xy);
    float terrain_height = SampleWorkingTerrainHeight(coord);
    const float water_dt = max(min(delta_time_seconds, kWaterDeltaTime), 1.0e-4f);
    const float basin_factor = ComputeBasinFactor(terrain_height);
    const float rain_amount = SampleRainAmount(coord);

    const float previous_depth = SamplePreviousDepth(coord);
    const float rain_input =
        rain_amount *
        accumulation_rate *
        kHydrologicalCycleRate *
        water_dt *
        lerp(0.55f, 1.0f, basin_factor);
    const float injected_water = ComputeInjectedWater(coord);
    const float seeded_min_depth =
        minimum_depth_for_surface *
        smoothstep(-minimum_depth_for_surface, basin_fade * 0.35f, water_surface_height - terrain_height);

    float source_depth =
        initialize_from_water_level != 0u
            ? max(water_surface_height - terrain_height, 0.0f)
            : max(previous_depth + rain_input + injected_water, 0.0f);

    if (initialize_from_water_level != 0u && source_depth > 1.0e-5f)
    {
        source_depth = max(source_depth, seeded_min_depth);
    }

    const float4 outflow = ComputeOutflowFromPreviousState(coord);
    const float incoming =
        ComputeOutflowFromPreviousState(coord + int2(-1, 0)).x +
        ComputeOutflowFromPreviousState(coord + int2(1, 0)).y +
        ComputeOutflowFromPreviousState(coord + int2(0, -1)).z +
        ComputeOutflowFromPreviousState(coord + int2(0, 1)).w;

    float next_depth = max(source_depth - dot(outflow, 1.0f.xxxx) + incoming, 0.0f);

    const float total_flux = dot(outflow, 1.0f.xxxx);
    const float2 flow_vector = float2(outflow.x - outflow.y, outflow.w - outflow.z);
    const float average_depth = max((source_depth + next_depth) * 0.5f, 1.0e-3f);
    const float2 uv = (float2(coord) + 0.5f) / float2(width, height);
    const float4 meteorograph_sample = g_Meteorograph.SampleLevel(g_TerrainSampler, uv, 0.0f);
    const float2 atmospheric_wind = meteorograph_sample.xy;
    const float atmospheric_humidity = saturate(meteorograph_sample.z);
    const float atmospheric_temperature = NormalizeClimateTemperature(meteorograph_sample.w);
    const float wind_strength = saturate(length(atmospheric_wind)) * 0.35f;
    const float2 wind_dir = SafeNormalize(atmospheric_wind);
    const float wind_surface_factor =
        smoothstep(0.02f, 0.14f, next_depth) *
        wind_strength *
        (0.70f + basin_factor * 0.45f);
    float2 water_velocity_xy = flow_vector / max(average_depth * 4.0f + 0.04f, 0.08f);
    water_velocity_xy += wind_dir * ((0.0045f + next_depth * 0.0180f) * wind_surface_factor);
    float water_speed = saturate(length(water_velocity_xy) * 2.8f);
    float transport_energy =
        saturate(water_speed * (0.55f + saturate(total_flux * 5.0f) * 0.45f + wind_surface_factor * 0.05f));

    const float east_terrain = SampleWorkingTerrainHeight(coord + int2(1, 0));
    const float west_terrain = SampleWorkingTerrainHeight(coord + int2(-1, 0));
    const float north_terrain = SampleWorkingTerrainHeight(coord + int2(0, 1));
    const float south_terrain = SampleWorkingTerrainHeight(coord + int2(0, -1));
    const float2 terrain_gradient = float2(
        east_terrain - west_terrain,
        north_terrain - south_terrain) * 0.5f;
    const float terrain_slope = saturate(length(terrain_gradient) * 0.55f);

    const float standing_water = smoothstep(0.03f, 0.18f, next_depth);
    const float runoff = smoothstep(0.004f, 0.045f, total_flux);
    const float sediment_capacity =
        saturate(water_speed * 0.62f + terrain_slope * 0.38f) *
        saturate(next_depth * 2.8f + 0.12f);
    const float erosion_tendency =
        saturate(terrain_slope * water_speed * 1.2f + runoff * 0.22f);
    const float deposition_tendency =
        saturate((1.0f - water_speed) * (standing_water * 0.70f + 0.18f));

    float suspended_sediment = 0.0f;
    float erosion_amount = 0.0f;
    float deposition_amount = 0.0f;

    if (initialize_from_water_level == 0u && next_depth > 1.0e-5f)
    {
        const int2 upstream_coord = ComputeUpstreamCoord(coord, flow_vector);
        const float previous_suspended_sediment =
            saturate(g_PreviousWaterSediment.Load(int3(upstream_coord, 0)).x);
        const float sediment_gap = sediment_capacity - previous_suspended_sediment;
        const float hydraulic_support =
            saturate((0.25f + next_depth * 0.75f) *
                     (0.20f + terrain_slope * 0.80f) *
                     (0.35f + transport_energy * 0.65f));

        float hydraulic_erosion =
            max(sediment_gap, 0.0f) *
            kHydraulicErosionRate *
            hydraulic_support *
            (0.55f + erosion_tendency * 0.45f) *
            water_dt * 12.0f;
        float hydraulic_deposition =
            max(-sediment_gap, 0.0f) *
            kHydraulicDepositionRate *
            (0.45f + standing_water * 0.55f) *
            (0.35f + deposition_tendency * 0.65f) *
            (1.0f - terrain_slope * 0.35f) *
            water_dt * 12.0f;

        const float neighbor_height =
            (east_terrain + west_terrain + north_terrain + south_terrain) * 0.25f;
        const float thermal_drive = max(terrain_height - neighbor_height, 0.0f);
        const float underwater_factor = lerp(1.0f, 0.42f, standing_water);
        const float thermal_excess =
            max(terrain_slope - kThermalSlopeThreshold * underwater_factor, 0.0f);
        const float thermal_erosion =
            thermal_drive *
            kThermalErosionRate *
            thermal_excess *
            (0.30f + next_depth * 0.20f + transport_energy * 0.20f) *
            water_dt * 12.0f;

        hydraulic_erosion = min(hydraulic_erosion + thermal_erosion, kMaxHydraulicErosion);
        hydraulic_deposition = min(hydraulic_deposition, kMaxHydraulicDeposition);

        erosion_amount = hydraulic_erosion;
        deposition_amount = hydraulic_deposition;

        const float terrain_delta = deposition_amount - erosion_amount;
        terrain_height += terrain_delta;
        next_depth = max(next_depth - terrain_delta, 0.0f);
        suspended_sediment =
            saturate(previous_suspended_sediment + erosion_amount - deposition_amount * 0.85f);
    }

    const float evaporation_loss =
        min(
            next_depth,
            evaporation_rate *
                water_dt *
                60.0f *
                lerp(1.08f, 0.58f, basin_factor) *
                lerp(0.88f, 1.18f, wind_strength) *
                lerp(0.84f, 1.16f, atmospheric_temperature) *
                lerp(1.08f, 0.82f, atmospheric_humidity));
    next_depth = max(next_depth - evaporation_loss, 0.0f);

    const float seepage_loss =
        min(
            next_depth,
            seepage_rate * water_dt * 60.0f * (1.0f - basin_factor));
    next_depth = max(next_depth - seepage_loss, 0.0f);

    const float shallow_sheet_factor = 1.0f - smoothstep(0.018f, 0.060f, next_depth);
    const float slope_sheet_factor = smoothstep(0.08f, 0.24f, terrain_slope);
    const float basin_sheet_factor = 1.0f - smoothstep(0.12f, 0.52f, basin_factor);
    const float sheet_prune_factor =
        shallow_sheet_factor *
        slope_sheet_factor *
        basin_sheet_factor *
        (1.0f - standing_water) *
        (0.42f + runoff * 0.58f);
    const float sheet_absorption = next_depth * saturate(sheet_prune_factor * 0.92f);
    next_depth = max(next_depth - sheet_absorption, 0.0f);

    float resolved_surface_height = terrain_height + next_depth;
    if (!IsEdgeCell(coord))
    {
        const float neighbor_surface_average =
            (SamplePreviousSurfaceHeight(coord + int2(1, 0)) +
             SamplePreviousSurfaceHeight(coord + int2(-1, 0)) +
             SamplePreviousSurfaceHeight(coord + int2(0, 1)) +
             SamplePreviousSurfaceHeight(coord + int2(0, -1)) +
             resolved_surface_height) * 0.2f;
        const float local_surface_slope =
            max(
                abs(SamplePreviousSurfaceHeight(coord + int2(1, 0)) - SamplePreviousSurfaceHeight(coord + int2(-1, 0))),
                abs(SamplePreviousSurfaceHeight(coord + int2(0, 1)) - SamplePreviousSurfaceHeight(coord + int2(0, -1))));
        const float standing_settle =
            standing_water *
            (1.0f - runoff) *
            (1.0f - saturate(local_surface_slope * 6.0f)) *
            saturate(1.0f - terrain_slope * 1.8f) *
            lerp(0.55f, 1.0f, basin_factor);
        const float settled_surface_height =
            max(terrain_height, lerp(resolved_surface_height, neighbor_surface_average, standing_settle * 0.55f));
        resolved_surface_height = settled_surface_height;
        next_depth = max(resolved_surface_height - terrain_height, 0.0f);
    }
    else
    {
        resolved_surface_height = max(resolved_surface_height, water_surface_height);
        next_depth = max(resolved_surface_height - terrain_height, 0.0f);
    }

    float4 resolved_outflow = outflow;
    float resolved_runoff = runoff;
    float resolved_sediment_capacity = sediment_capacity;
    float resolved_deposition_tendency = deposition_tendency;
    float resolved_erosion_tendency = erosion_tendency;
    if (next_depth < kDryDepthEpsilon)
    {
        next_depth = 0.0f;
        resolved_surface_height = water_surface_height;
        water_velocity_xy = 0.0f.xx;
        water_speed = 0.0f;
        transport_energy = 0.0f;
        suspended_sediment = 0.0f;
        resolved_outflow = 0.0f.xxxx;
        resolved_runoff = 0.0f;
        resolved_sediment_capacity = 0.0f;
        resolved_deposition_tendency = 0.0f;
        resolved_erosion_tendency = 0.0f;
    }

    const float4 visible_water = ComputeVisibleWaterSample(
        terrain_height,
        west_terrain,
        east_terrain,
        south_terrain,
        north_terrain,
        next_depth,
        resolved_outflow);
    const float4 visible_left = ComputeApproxVisibleWaterAtCoord(coord + int2(-1, 0));
    const float4 visible_right = ComputeApproxVisibleWaterAtCoord(coord + int2(1, 0));
    const float4 visible_up = ComputeApproxVisibleWaterAtCoord(coord + int2(0, -1));
    const float4 visible_down = ComputeApproxVisibleWaterAtCoord(coord + int2(0, 1));
    const float4 water_mask = ComputeWaterMaskSample(
        visible_water,
        visible_left,
        visible_right,
        visible_up,
        visible_down);
    const float normal_y = rsqrt(1.0f + dot(terrain_gradient, terrain_gradient));
    const float slope_amount = saturate(1.0f - normal_y);
    const float surface_slope =
        smoothstep(1.0f - rock_slope_end, 1.0f - rock_slope_start, slope_amount);
    const float shoreline =
        smoothstep(
            water_surface_height + shoreline_offset_start,
            water_surface_height + shoreline_offset_end,
            terrain_height);
    const float surface_wetness = saturate(max(water_mask.r, visible_water.r * 0.88f + visible_water.g * 0.22f));
    const float shoreline_wetness = saturate(max(water_mask.g, water_mask.b * 0.28f));
    const float pooled_wetness = saturate(max(water_mask.b, visible_water.b * 0.92f));
    const float retained_wetness = saturate(max(surface_wetness * 0.55f, pooled_wetness * 0.38f));
    const float erosion_mask =
        saturate(erosion_amount * 1.85f + max(erosion_amount - deposition_amount, 0.0f) * 0.65f);
    const float wetness =
        saturate(
            max(shoreline * (0.68f + shoreline_wetness * 0.32f), retained_wetness * 0.55f) * wetness_gain +
            surface_wetness * 0.18f +
            pooled_wetness * 0.12f);
    const float beach_mask =
        saturate(max(shoreline_wetness, shoreline * (1.0f - pooled_wetness * 0.25f)));
    const float humidity =
        saturate(retained_wetness * 0.48f + wetness * 0.20f);
    const float roughness =
        saturate(lerp(0.24f, 0.92f, max(erosion_mask, surface_slope * 0.65f)));

    g_WaterFlowOut[dispatch_thread_id.xy] = resolved_outflow;
    g_TerrainSurfaceDataOut[dispatch_thread_id.xy] =
        float4(surface_slope, beach_mask, humidity, roughness);
    g_WaterVelocityOut[dispatch_thread_id.xy] =
        float4(water_velocity_xy, water_speed, transport_energy);
    g_WaterSedimentOut[dispatch_thread_id.xy] =
        float4(suspended_sediment, resolved_deposition_tendency, resolved_erosion_tendency, resolved_sediment_capacity);
    g_WaterSurfaceHeight[dispatch_thread_id.xy] =
        float2(terrain_height, resolved_surface_height);
}
