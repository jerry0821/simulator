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
Texture2D<float4> g_PreviousTerrainWaterHeight : register(t1);
Texture2D g_RainMap : register(t2);
Texture2D<float4> g_PreviousWaterFlow : register(t3);
Texture2D<float4> g_PreviousWaterSediment : register(t4);
Texture2D<float4> g_Meteorograph : register(t5);
Texture2D<float4> g_PreviousSoilMoisture : register(t6);
SamplerState g_TerrainSampler : register(s0);
RWTexture2D<float4> g_WaterSurfaceHeight : register(u0);
RWTexture2D<float4> g_WaterFlowOut : register(u1);
RWTexture2D<float4> g_WaterVelocityOut : register(u2);
RWTexture2D<float4> g_WaterSedimentOut : register(u3);
RWTexture2D<float4> g_ErosionDeltaOut : register(u4);
RWTexture2D<float4> g_SoilMoistureOut : register(u5);
RWTexture2D<float4> g_WaterInteractionOut : register(u6);
RWTexture2D<float4> g_TerrainSurfaceDataOut : register(u7);

static const float kWaterDeltaTime = 1.0f / 60.0f;
static const float kWaterFlowDamping = 0.005f;
static const float kHydrologicalCycleRate = 0.5f;
static const float kWaterBaseHeight = 0.0f;
static const float kHydraulicErosionRate = 0.020f;
static const float kHydraulicDepositionRate = 0.016f;
static const float kThermalErosionRate = 0.030f;
static const float kThermalSlopeThreshold = 0.42f;
static const float kMaxHydraulicErosion = 0.020f;
static const float kMaxHydraulicDeposition = 0.014f;

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

float4 SamplePreviousStateRaw(int2 coord)
{
    return g_PreviousTerrainWaterHeight.Load(int3(ClampCoord(coord), 0));
}

float SampleWorkingTerrainHeight(int2 coord)
{
    float terrain_height = SampleBaseTerrainHeight(coord);
    if (initialize_from_water_level != 0u)
    {
        return terrain_height;
    }

    terrain_height = SamplePreviousStateRaw(coord).x;
    return terrain_height;
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
    return max(g_RainMap.Load(int3(ClampCoord(coord), 0)).r, 0.0f);
}

float4 SampleRainState(int2 coord)
{
    return g_RainMap.Load(int3(ClampCoord(coord), 0));
}

float4 SamplePreviousSoilMoistureState(int2 coord)
{
    return g_PreviousSoilMoisture.Load(int3(ClampCoord(coord), 0));
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
    float injected_water = 0.0f;
    if (injection_enabled == 0u || injection_radius <= 1.0e-4f || injection_amount <= 1.0e-4f)
    {
        return 0.0f;
    }

    const float2 world_pos = ComputeWorldPosition(coord);
    const float distance_to_injection =
        length(world_pos - float2(injection_center_x, injection_center_z));
    const float injection_falloff =
        1.0f - smoothstep(injection_radius * 0.30f, injection_radius, distance_to_injection);
    injected_water = injection_amount * saturate(injection_falloff);
    return injected_water;
}

float4 ComputeOutflowFromPreviousState(int2 coord)
{
    float4 outflow = 0.0f.xxxx;
    if (!InBounds(coord))
    {
        return 0.0f.xxxx;
    }

    const float water_dt = max(min(delta_time_seconds, kWaterDeltaTime), 1.0e-4f);
    const float4 previous_flow = max(g_PreviousWaterFlow.Load(int3(ClampCoord(coord), 0)), 0.0f.xxxx);
    const float available_water = SamplePreviousDepth(coord);
    if (available_water <= 1.0e-5f)
    {
        outflow = max(previous_flow * saturate(1.0f - water_dt * 4.0f), 0.0f.xxxx);
        return outflow;
    }

    const float center_surface = SamplePreviousSurfaceHeight(coord);
    float4 surface_delta_heights = float4(
        max(center_surface - SamplePreviousSurfaceHeight(coord + int2(1, 0)), 0.0f),
        max(center_surface - SamplePreviousSurfaceHeight(coord + int2(-1, 0)), 0.0f),
        max(center_surface - SamplePreviousSurfaceHeight(coord + int2(0, 1)), 0.0f),
        max(center_surface - SamplePreviousSurfaceHeight(coord + int2(0, -1)), 0.0f));

    const float2 uv = (float2(ClampCoord(coord)) + 0.5f) / float2(width, height);
    const float4 meteorograph_sample = g_Meteorograph.SampleLevel(g_TerrainSampler, uv, 0.0f);
    const float2 wind_velocity = meteorograph_sample.xy;
    const float wind_strength = saturate(length(wind_velocity));
    const float2 wind_dir = SafeNormalize(wind_velocity);
    if (wind_strength > 1.0e-4f)
    {
        const float2 world_pos = ComputeWorldPosition(coord);
        const float along_wind = dot(world_pos, wind_dir);
        const float cross_wind = dot(world_pos, float2(-wind_dir.y, wind_dir.x));
        const float wind_phase =
            along_wind * 0.030f -
            time_seconds * (0.10f + wind_strength * 0.40f) +
            cross_wind * 0.006f;
        const float gust = 0.5f + 0.5f * sin(wind_phase * 6.2831853f);
        const float4 neighbor_depth = float4(
            SamplePreviousDepth(coord + int2(1, 0)),
            SamplePreviousDepth(coord + int2(-1, 0)),
            SamplePreviousDepth(coord + int2(0, 1)),
            SamplePreviousDepth(coord + int2(0, -1)));
        const float disturbance_scale =
            wind_strength *
            (0.10f + gust * 0.16f) *
            max(length(surface_delta_heights), 0.001f);
        const float4 wind_effect =
            float4(wind_dir.x, -wind_dir.x, wind_dir.y, -wind_dir.y) * disturbance_scale;
        const float4 max_disturbance =
            max(min(neighbor_depth, available_water.xxxx) * 0.22f, 0.001f.xxxx);
        surface_delta_heights =
            max(surface_delta_heights + clamp(wind_effect, -max_disturbance, max_disturbance), 0.0f.xxxx);
    }

    float4 candidate = max(previous_flow + water_dt * flow_rate * surface_delta_heights, 0.0f.xxxx);
    const float total_candidate = dot(candidate, 1.0f.xxxx);
    if (total_candidate <= 1.0e-5f)
    {
        outflow = max(previous_flow * saturate(1.0f - water_dt * 4.0f), 0.0f.xxxx);
        return outflow;
    }

    const float movable_water = available_water * max_outflow_fraction;
    const float scale = min((movable_water / total_candidate) * (1.0f - kWaterFlowDamping), 1.0f);
    outflow = candidate * scale;
    return outflow;
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

float4 ComputeSoilMoistureSample(
    int2 coord,
    float water_dt,
    float4 rain_state,
    float next_depth,
    float4 water_mask)
{
    static const float rain_absorption = 0.16f;
    static const float wetness_absorption = 0.14f;
    static const float shoreline_absorption = 0.10f;
    static const float evaporation_rate = 0.045f;
    static const float diffusion_rate = 0.16f;
    static const float saturation_decay = 0.12f;
    static const float moisture_step_scale = 4.0f;

    const float moisture_dt = saturate(water_dt * moisture_step_scale);
    const float4 previous_state = SamplePreviousSoilMoistureState(coord);
    const float previous = previous_state.r;
    const float neighbor_average =
        (SamplePreviousSoilMoistureState(coord + int2(1, 0)).r +
         SamplePreviousSoilMoistureState(coord + int2(-1, 0)).r +
         SamplePreviousSoilMoistureState(coord + int2(0, 1)).r +
         SamplePreviousSoilMoistureState(coord + int2(0, -1)).r) * 0.25f;

    const float rain_amount = max(rain_state.r, 0.0f);
    const float rain_wet_hint = saturate(rain_state.g);
    const float water_coverage = saturate(water_mask.r);
    const float shoreline_contact = saturate(water_mask.g);
    const float pooled_contact = saturate(water_mask.b);
    const float standing_water = smoothstep(0.04f, 0.16f, next_depth);

    const float rain_gain = rain_amount * rain_absorption;
    const float seep_gain = standing_water * (wetness_absorption * 1.10f) + next_depth * 0.28f;
    const float bank_gain =
        max(water_coverage * shoreline_absorption, shoreline_contact * (shoreline_absorption * 1.20f));
    const float evaporation =
        evaporation_rate * (1.0f - saturate(rain_amount * 0.80f + water_coverage * 0.55f));
    const float saturation_loss = max(previous - 0.82f, 0.0f) * (saturation_decay * 0.65f);

    float moisture =
        previous +
        (rain_gain + seep_gain + bank_gain - evaporation - saturation_loss) * moisture_dt;
    moisture = lerp(moisture, neighbor_average, diffusion_rate * moisture_dt);
    moisture = saturate(moisture + rain_wet_hint * (0.06f * moisture_dt));

    const float shoreline_band =
        saturate(max(shoreline_contact, water_coverage * 0.75f + standing_water * 0.50f));
    const float retained_rainfall_memory = previous_state.b * saturate(1.0f - moisture_dt * 0.65f);
    const float rainfall_memory =
        saturate(max(retained_rainfall_memory, rain_amount * 0.65f + rain_wet_hint * 0.35f));
    const float retained_saturation = previous_state.a * saturate(1.0f - moisture_dt * 0.22f);
    const float saturation =
        saturate(max(max(next_depth * 1.10f, moisture), max(pooled_contact, retained_saturation)));

    return float4(moisture, shoreline_band, rainfall_memory, saturation);
}

float4 ComputeWaterInteractionSample(
    float4 visible_water,
    float4 water_mask,
    float4 soil_moisture)
{
    const float surface_interaction = saturate(
        max(
            water_mask.r,
            visible_water.r * 0.88f + visible_water.g * 0.22f));
    const float shoreline_influence = saturate(
        max(
            water_mask.g,
            max(soil_moisture.g, water_mask.b * 0.28f)));
    const float pooled_interaction = saturate(
        max(
            water_mask.b,
            visible_water.b * 0.92f));
    const float retained_wetness = saturate(
        max(
            soil_moisture.r * 0.72f,
            soil_moisture.a * 0.38f + soil_moisture.b * 0.08f));

    return float4(
        surface_interaction,
        shoreline_influence,
        pooled_interaction,
        retained_wetness);
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
    const float4 rain_state = SampleRainState(coord);

    const float previous_depth = SamplePreviousDepth(coord);
    const float rain_input =
        max(rain_state.r, 0.0f) *
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
    const float atmospheric_temperature = saturate(meteorograph_sample.w);
    const float wind_strength = saturate(length(atmospheric_wind));
    const float2 wind_dir = SafeNormalize(atmospheric_wind);
    const float wind_surface_factor =
        smoothstep(0.01f, 0.12f, next_depth) *
        wind_strength *
        (0.55f + basin_factor * 0.45f);
    float2 water_velocity_xy = flow_vector / max(average_depth * 4.0f + 0.04f, 0.08f);
    water_velocity_xy += wind_dir * ((0.008f + next_depth * 0.035f) * wind_surface_factor);
    float water_speed = saturate(length(water_velocity_xy) * 3.5f);
    float transport_energy =
        saturate(water_speed * (0.55f + saturate(total_flux * 5.0f) * 0.45f + wind_surface_factor * 0.12f));

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

    const float resolved_surface_height =
        IsEdgeCell(coord)
            ? max(terrain_height + next_depth, kWaterBaseHeight)
            : terrain_height + next_depth;

    const float depth_preview = smoothstep(0.01f, 0.12f, next_depth);
    const float2 world_pos = ComputeWorldPosition(coord);
    const float along_wind = dot(world_pos, wind_dir);
    const float cross_wind = dot(world_pos, float2(-wind_dir.y, wind_dir.x));
    const float wind_phase =
        along_wind * 0.040f -
        time_seconds * lerp(0.14f, 0.95f, wind_strength) +
        cross_wind * 0.010f;
    const float ripple_a = 0.5f + 0.5f * sin(wind_phase * 6.2831853f);
    const float ripple_b = 0.5f + 0.5f * sin(wind_phase * 12.5663706f + 0.85f);
    const float wind_glint =
        pow(saturate(ripple_a * 0.72f + ripple_b * 0.28f), 2.2f) *
        smoothstep(0.015f, 0.10f, max(next_depth, runoff * 0.08f));
    const float flow_sheen =
        pow(saturate(0.5f + 0.5f * dot(SafeNormalize(flow_vector), wind_dir)), 2.0f) *
        water_speed * 0.30f;

    float3 preview_color = lerp(
        float3(0.035f, 0.075f, 0.120f),
        float3(0.140f, 0.235f, 0.325f),
        saturate(next_depth * 1.45f + water_speed * 0.22f));
    preview_color = lerp(preview_color, float3(0.82f, 0.90f, 0.98f), wind_glint * 0.48f);
    preview_color += float3(0.16f, 0.20f, 0.24f) * flow_sheen;

    const float preview_alpha =
        saturate(max(standing_water * 0.24f, runoff * 0.30f) + wind_glint * 0.18f);
    const float4 visible_water = ComputeVisibleWaterSample(
        terrain_height,
        west_terrain,
        east_terrain,
        south_terrain,
        north_terrain,
        next_depth,
        outflow);
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
    const float4 soil_moisture = ComputeSoilMoistureSample(
        coord,
        water_dt,
        rain_state,
        next_depth,
        water_mask);
    const float4 water_interaction =
        ComputeWaterInteractionSample(visible_water, water_mask, soil_moisture);
    const float normal_y = rsqrt(1.0f + dot(terrain_gradient, terrain_gradient));
    const float slope_amount = saturate(1.0f - normal_y);
    const float surface_slope =
        smoothstep(1.0f - rock_slope_end, 1.0f - rock_slope_start, slope_amount);
    const float shoreline =
        smoothstep(
            water_surface_height + shoreline_offset_start,
            water_surface_height + shoreline_offset_end,
            terrain_height);
    const float shoreline_wetness = water_interaction.g;
    const float pooled_wetness = water_interaction.b;
    const float retained_wetness = water_interaction.a;
    const float surface_wetness = water_interaction.r;
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

    g_WaterFlowOut[dispatch_thread_id.xy] = outflow;
    g_SoilMoistureOut[dispatch_thread_id.xy] = soil_moisture;
    g_WaterInteractionOut[dispatch_thread_id.xy] = water_interaction;
    g_TerrainSurfaceDataOut[dispatch_thread_id.xy] =
        float4(surface_slope, beach_mask, humidity, roughness);
    g_WaterVelocityOut[dispatch_thread_id.xy] =
        float4(water_velocity_xy, water_speed, transport_energy);
    g_WaterSedimentOut[dispatch_thread_id.xy] =
        float4(suspended_sediment, deposition_tendency, erosion_tendency, sediment_capacity);
    g_ErosionDeltaOut[dispatch_thread_id.xy] =
        float4(erosion_amount, deposition_amount, deposition_amount - erosion_amount, saturate(max(transport_energy, water_speed)));
    g_WaterSurfaceHeight[dispatch_thread_id.xy] =
        float4(terrain_height, resolved_surface_height, next_depth, depth_preview);
}
