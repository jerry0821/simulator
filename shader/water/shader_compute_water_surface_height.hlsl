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
    uint width;
    uint height;
    uint initialize_from_water_level;
    uint injection_enabled;
    uint padding1;
    uint padding2;
    uint padding3;
    uint padding4;
    uint padding5;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D<float4> g_PreviousTerrainWaterHeight : register(t1);
Texture2D g_RainMap : register(t2);
Texture2D<float4> g_PreviousWaterFlow : register(t3);
Texture2D<float4> g_PreviousWaterSediment : register(t4);
RWTexture2D<float4> g_WaterSurfaceHeight : register(u0);
RWTexture2D<float4> g_WaterFlowOut : register(u1);
RWTexture2D<float4> g_WaterVelocityOut : register(u2);
RWTexture2D<float4> g_WaterSedimentOut : register(u3);
RWTexture2D<float4> g_ErosionDeltaOut : register(u4);

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

    const float edge_delta =
        max(center_surface - max(SampleWorkingTerrainHeight(coord), kWaterBaseHeight), 0.0f) * 0.12f;
    if (coord.x <= 0)
    {
        surface_delta_heights.y = edge_delta;
    }
    if (coord.x >= int(width) - 1)
    {
        surface_delta_heights.x = edge_delta;
    }
    if (coord.y <= 0)
    {
        surface_delta_heights.w = edge_delta;
    }
    if (coord.y >= int(height) - 1)
    {
        surface_delta_heights.z = edge_delta;
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

    const float previous_depth = SamplePreviousDepth(coord);
    const float rain_input =
        SampleRainAmount(coord) *
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
    const float2 water_velocity_xy = flow_vector / max(average_depth * 4.0f + 0.04f, 0.08f);
    const float water_speed = saturate(length(water_velocity_xy) * 3.5f);
    const float transport_energy =
        saturate(water_speed * (0.55f + saturate(total_flux * 5.0f) * 0.45f));

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
            evaporation_rate * water_dt * 60.0f * lerp(1.25f, 0.55f, basin_factor));
    next_depth = max(next_depth - evaporation_loss, 0.0f);

    const float seepage_loss =
        min(
            next_depth,
            seepage_rate * water_dt * 60.0f * (1.0f - basin_factor));
    next_depth = max(next_depth - seepage_loss, 0.0f);

    if (IsEdgeCell(coord))
    {
        next_depth = max(next_depth - min(next_depth, 0.01f * water_dt * 60.0f), 0.0f);
    }

    const float resolved_surface_height =
        IsEdgeCell(coord)
            ? max(terrain_height + next_depth, max(terrain_height, kWaterBaseHeight))
            : terrain_height + next_depth;

    g_WaterFlowOut[dispatch_thread_id.xy] = outflow;
    g_WaterVelocityOut[dispatch_thread_id.xy] =
        float4(water_velocity_xy, water_speed, transport_energy);
    g_WaterSedimentOut[dispatch_thread_id.xy] =
        float4(suspended_sediment, deposition_tendency, erosion_tendency, sediment_capacity);
    g_ErosionDeltaOut[dispatch_thread_id.xy] =
        float4(erosion_amount, deposition_amount, deposition_amount - erosion_amount, saturate(max(transport_energy, water_speed)));
    g_WaterSurfaceHeight[dispatch_thread_id.xy] =
        float4(terrain_height, resolved_surface_height, 0.0f, 0.0f);
}
