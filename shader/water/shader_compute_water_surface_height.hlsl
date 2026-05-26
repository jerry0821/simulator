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
static const float kWaterFlowDamping = 0.005f;
static const float kHydrologicalCycleRate = 0.5f;
static const float kWorldGravity = 9.8f;
static const float kWaterViscosity = 1.0f;
static const float kWindEffectIntensity = 0.65f;
static const uint kWindEffectWaveSize = 64u;
static const float kWaterBaseHeight = 0.0f;
static const float kDryDepthEpsilon = 0.008f;
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

float NormalizeClimateTemperature(float temperature_celsius)
{
    return saturate((temperature_celsius - kPolarTemperature) / max(kEquatorialTemperature - kPolarTemperature, 1.0e-4f));
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

float SamplePreviousWaterHeight(int2 coord)
{
    const float previous_water_height = SamplePreviousStateRaw(coord).y;
    return initialize_from_water_level != 0u
        ? kWaterBaseHeight
        : previous_water_height;
}

float SamplePreviousDepth(int2 coord)
{
    return max(SamplePreviousWaterHeight(coord) - SampleWorkingTerrainHeight(coord), 0.0f);
}

float SampleRainAmount(int2 coord)
{
    const float2 uv = (float2(ClampCoord(coord)) + 0.5f) / float2(width, height);
    return max(g_RainMap.SampleLevel(g_TerrainSampler, uv, 0.0f), 0.0f);
}

float2 ComputeWorldPosition(int2 coord)
{
    const float2 uv = (float2(coord) + 0.5f) / float2(width, height);
    return float2((uv.x - 0.5f) * field_width, (uv.y - 0.5f) * field_depth);
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
    const float2 world_pos = ComputeWorldPosition(coord);
    const float safe_radius = max(injection_radius, 1.0e-4f);
    const float distance_to_injection = length(world_pos - float2(injection_center_x, injection_center_z));
    return injection_amount *
        saturate(1.0f - smoothstep(safe_radius * 0.30f, safe_radius, distance_to_injection)) *
        injection_active;
}

float2 ClampVectorWithLength(float2 value, float min_length, float max_length)
{
    const float length_sq = dot(value, value);
    const float value_length = sqrt(max(length_sq, 1.0e-8f));
    const float active = length_sq > 1.0e-8f ? 1.0f : 0.0f;
    return value * (clamp(value_length, min_length, max_length) / value_length) * active;
}

float4 ComputeOutflowFromPreviousState(int2 coord)
{
    const float water_dt = max(min(delta_time_seconds, kWaterDeltaTime), 1.0e-4f);
    const float terrain_height = SampleWorkingTerrainHeight(coord);
    const float water_height = SamplePreviousWaterHeight(coord);
    const float available_depth = max(water_height - terrain_height, 0.0f);
    const float coord_active = InBounds(coord) && available_depth > kDryDepthEpsilon ? 1.0f : 0.0f;

    const float4 previous_flow_raw = max(g_PreviousWaterFlow.Load(int3(ClampCoord(coord), 0)), 0.0f.xxxx);
    const float cell_size_x = field_width / max(float(width), 1.0f);
    const float cell_size_z = field_depth / max(float(height), 1.0f);
    const float cell_area = max(cell_size_x * cell_size_z, 1.0e-4f);
    const float cell_interval = max(max(cell_size_x, cell_size_z), 1.0e-4f);
    const float water_flow_pipe_area = cell_area / kWaterViscosity;

    const float4 neighbor_terrain_height = float4(
        SampleWorkingTerrainHeight(coord + int2(1, 0)),
        SampleWorkingTerrainHeight(coord + int2(-1, 0)),
        SampleWorkingTerrainHeight(coord + int2(0, 1)),
        SampleWorkingTerrainHeight(coord + int2(0, -1)));
    const float4 neighbor_water_height = float4(
        SamplePreviousWaterHeight(coord + int2(1, 0)),
        SamplePreviousWaterHeight(coord + int2(-1, 0)),
        SamplePreviousWaterHeight(coord + int2(0, 1)),
        SamplePreviousWaterHeight(coord + int2(0, -1)));
    const float4 neighbor_surface_height = max(neighbor_terrain_height, neighbor_water_height);
    const float surface_height = max(terrain_height, water_height);
    const float4 terrain_barrier =
        max(sign(surface_height - (neighbor_terrain_height + kDryDepthEpsilon)), 0.0f.xxxx);
    const float4 previous_flow = previous_flow_raw * terrain_barrier;
    float4 surface_delta_height = surface_height - neighbor_surface_height;

    const float4 meteorograph_sample =
        g_Meteorograph.SampleLevel(g_TerrainSampler, (float2(ClampCoord(coord)) + 0.5f) / float2(width, height), 0.0f);
    if ((uint(coord.x) % kWindEffectWaveSize) == 0u || (uint(coord.y) % kWindEffectWaveSize) == 0u)
    {
        const float2 resized_wind = ClampVectorWithLength(meteorograph_sample.xy, 0.8f, 64.0f);
        const float4 wind_effect = float4(-resized_wind.x, resized_wind.x, -resized_wind.y, resized_wind.y);
        const float4 max_disturbance = max(neighbor_surface_height - neighbor_terrain_height, 0.0f.xxxx) * water_dt;
        surface_delta_height -= clamp(
            wind_effect * kWindEffectIntensity * max(length(surface_delta_height), 0.001f),
            -max_disturbance,
            max_disturbance) * terrain_barrier;
    }
    surface_delta_height *= terrain_barrier;

    float4 candidate = max(
        previous_flow +
            water_dt *
            max(flow_rate, 0.0f) *
            water_flow_pipe_area *
            ((kWorldGravity * surface_delta_height) / cell_interval),
        0.0f.xxxx) * terrain_barrier;
    const float total_candidate = dot(candidate, 1.0f.xxxx);

    const float movable_volume = available_depth * cell_area * max_outflow_fraction;
    const float scale = total_candidate > 1.0e-6f
        ? min((movable_volume / max(total_candidate * water_dt, 1.0e-6f)) * (1.0f - kWaterFlowDamping), 1.0f)
        : 1.0f;
    return candidate * scale * coord_active;
}

float2 ComputeUpstreamCoordOffset(float2 flow_vector)
{
    return clamp(flow_vector * 4.0f, -1.0f.xx, 1.0f.xx);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const int2 coord = int2(dispatch_thread_id.xy);
    const float water_dt = max(min(delta_time_seconds, kWaterDeltaTime), 1.0e-4f);
    const float cell_size_x = field_width / max(float(width), 1.0f);
    const float cell_size_z = field_depth / max(float(height), 1.0f);
    const float cell_area = max(cell_size_x * cell_size_z, 1.0e-4f);

    float terrain_height = SampleWorkingTerrainHeight(coord);
    float water_height = initialize_from_water_level != 0u
        ? kWaterBaseHeight
        : SamplePreviousWaterHeight(coord);

    const float basin_factor = ComputeBasinFactor(terrain_height);
    const float rain_input =
        SampleRainAmount(coord) *
        accumulation_rate *
        kHydrologicalCycleRate *
        water_dt *
        lerp(0.55f, 1.0f, basin_factor);
    water_height += rain_input + ComputeInjectedWater(coord);

    const float4 outflow = ComputeOutflowFromPreviousState(coord);
    const float incoming =
        ComputeOutflowFromPreviousState(coord + int2(-1, 0)).x +
        ComputeOutflowFromPreviousState(coord + int2(1, 0)).y +
        ComputeOutflowFromPreviousState(coord + int2(0, -1)).z +
        ComputeOutflowFromPreviousState(coord + int2(0, 1)).w;
    water_height += water_dt * (incoming - dot(outflow, 1.0f.xxxx)) / cell_area;

    const float4 meteorograph_sample =
        g_Meteorograph.SampleLevel(g_TerrainSampler, (float2(coord) + 0.5f) / float2(width, height), 0.0f);
    const float wind_strength = saturate(length(meteorograph_sample.xy)) * 0.35f;
    const float atmospheric_humidity = saturate(meteorograph_sample.z);
    const float atmospheric_temperature = NormalizeClimateTemperature(meteorograph_sample.w);
    const float evaporation_loss =
        evaporation_rate *
        water_dt *
        60.0f *
        lerp(1.08f, 0.58f, basin_factor) *
        lerp(0.88f, 1.18f, wind_strength) *
        lerp(0.84f, 1.16f, atmospheric_temperature) *
        lerp(1.08f, 0.82f, atmospheric_humidity);
    water_height = max(water_height - min(max(water_height - terrain_height, 0.0f), evaporation_loss), kWaterBaseHeight);

    const float2 terrain_gradient = float2(
        SampleWorkingTerrainHeight(coord + int2(1, 0)) - SampleWorkingTerrainHeight(coord + int2(-1, 0)),
        SampleWorkingTerrainHeight(coord + int2(0, 1)) - SampleWorkingTerrainHeight(coord + int2(0, -1))) * 0.5f;
    const float terrain_slope = saturate(length(terrain_gradient) * 0.55f);

    const float source_depth = max(SamplePreviousWaterHeight(coord) - terrain_height, 0.0f);
    float next_depth = max(water_height - terrain_height, 0.0f);
    const float average_depth = max((source_depth + next_depth) * 0.5f, 1.0e-3f);
    const float2 flow_vector = float2(outflow.x - outflow.y, outflow.z - outflow.w);
    float2 water_velocity_xy = flow_vector / max(average_depth * max(cell_size_x, cell_size_z), 0.1f) * water_dt;
    float water_speed = next_depth > kDryDepthEpsilon ? saturate(length(water_velocity_xy) * 2.8f) : 0.0f;
    float transport_energy = saturate(water_speed * (0.55f + saturate(dot(outflow, 1.0f.xxxx) * 5.0f) * 0.45f));

    float suspended_sediment = 0.0f;
    float deposition_tendency = 0.0f;
    float erosion_tendency = 0.0f;
    float sediment_capacity = 0.0f;
    if (next_depth > kDryDepthEpsilon)
    {
        const int2 upstream_coord = ClampCoord(coord - int2(round(ComputeUpstreamCoordOffset(flow_vector))));
        const float previous_sediment = saturate(g_PreviousWaterSediment.Load(int3(upstream_coord, 0)).x);
        sediment_capacity =
            saturate(water_speed * 0.62f + terrain_slope * 0.38f) *
            saturate(next_depth * 2.8f + 0.12f);
        const float sediment_delta = sediment_capacity - previous_sediment;
        const float erosion_amount =
            min(max(sediment_delta, 0.0f) * kHydraulicErosionRate * water_dt * 12.0f, kMaxHydraulicErosion);
        const float deposition_amount =
            min(max(-sediment_delta, 0.0f) * kHydraulicDepositionRate * water_dt * 12.0f, kMaxHydraulicDeposition);
        terrain_height += deposition_amount - erosion_amount;
        suspended_sediment = saturate(previous_sediment + erosion_amount - deposition_amount * 0.85f);
        erosion_tendency = saturate(erosion_amount * 50.0f);
        deposition_tendency = saturate(deposition_amount * 60.0f);

        const float neighbor_height =
            (SampleWorkingTerrainHeight(coord + int2(1, 0)) +
             SampleWorkingTerrainHeight(coord + int2(-1, 0)) +
             SampleWorkingTerrainHeight(coord + int2(0, 1)) +
             SampleWorkingTerrainHeight(coord + int2(0, -1))) * 0.25f;
        const float thermal_excess = max(terrain_slope - kThermalSlopeThreshold * 0.42f, 0.0f);
        terrain_height = lerp(
            terrain_height,
            neighbor_height,
            saturate(water_dt * kThermalErosionRate * thermal_excess));
    }

    next_depth = max(water_height - terrain_height, 0.0f);
    const float shallow_sheet = 1.0f - smoothstep(0.04f, 0.16f, next_depth);
    const float steep_sheet = smoothstep(0.22f, 0.55f, terrain_slope);
    const float basin_keep = smoothstep(0.15f, 0.65f, basin_factor);
    const float sheet_prune = saturate(shallow_sheet * steep_sheet * (1.0f - basin_keep * 0.85f));
    const float sheet_loss = next_depth * sheet_prune * 0.90f;
    water_height -= sheet_loss;
    next_depth = max(water_height - terrain_height, 0.0f);

    if (next_depth <= kDryDepthEpsilon || IsEdgeCell(coord))
    {
        water_velocity_xy = 0.0f.xx;
        water_speed = 0.0f;
        transport_energy = 0.0f;
    }

    water_height = next_depth > kDryDepthEpsilon
        ? max(water_height, terrain_height)
        : min(water_height, terrain_height - kDryDepthEpsilon);

    const float normal_y = rsqrt(1.0f + dot(terrain_gradient, terrain_gradient));
    const float surface_slope =
        smoothstep(1.0f - rock_slope_end, 1.0f - rock_slope_start, saturate(1.0f - normal_y));
    const float water_to_terrain = terrain_height - water_height;
    const float beach_mask = max(1.0f - max(water_to_terrain * 10.0f, 0.0f), 0.0f);
    const float humidity = max((water_to_terrain - 0.5f) / max(basin_fade, 1.0e-4f), 0.0f);
    const float roughness = saturate(lerp(0.24f, 0.92f, max(erosion_tendency, surface_slope * 0.65f)));

    g_WaterFlowOut[dispatch_thread_id.xy] = next_depth > kDryDepthEpsilon ? outflow : 0.0f.xxxx;
    g_TerrainSurfaceDataOut[dispatch_thread_id.xy] =
        float4(surface_slope, beach_mask, humidity, roughness);
    g_WaterVelocityOut[dispatch_thread_id.xy] =
        float4(water_velocity_xy, water_speed, transport_energy);
    g_WaterSedimentOut[dispatch_thread_id.xy] =
        float4(suspended_sediment, deposition_tendency, erosion_tendency, sediment_capacity);
    g_WaterSurfaceHeight[dispatch_thread_id.xy] =
        float2(terrain_height, water_height);
}
