cbuffer SURFACE_WATER_CONSTANT_BUFFER : register(b0)
{
    float water_height;
    float accumulation_rate;
    float evaporation_rate;
    float seepage_rate;
    float basin_fade;
    float downhill_flow_rate;
    float flow_damping;
    float max_outflow_fraction;
    float field_width;
    float field_depth;
    float injection_center_x;
    float injection_center_z;
    float injection_radius;
    float injection_amount;
    float time_seconds;
    uint width;
    uint height;
    uint presentation_only;
    uint seed_from_water_level;
    uint injection_enabled;
    uint padding0;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D g_RainMap : register(t1);
Texture2D g_WindField : register(t2);
Texture2D<float2> g_PreviousTerrainWaterHeight : register(t3);
SamplerState g_SurfaceSampler : register(s0);
RWTexture2D<float4> g_SurfaceWater : register(u0);
RWTexture2D<float4> g_SurfaceWaterFlow : register(u1);
RWTexture2D<float4> g_SurfaceWaterFlowPreview : register(u2);
RWTexture2D<float4> g_WaterVelocity : register(u3);
RWTexture2D<float4> g_WaterSediment : register(u4);

static const float kWaterDeltaTime = 1.0f / 60.0f;
static const float kWorldGravity = 9.8f;
static const float kPipeArea = 1.0f;

float ComputeBasinFactor(float terrain_height)
{
    float basin_depth = water_height - terrain_height;
    return saturate((basin_depth + 2.0f) / max(basin_fade, 1.0e-4f));
}

bool InBounds(int2 coord)
{
    return coord.x >= 0 && coord.y >= 0 && coord.x < int(width) && coord.y < int(height);
}

bool IsEdgeCell(int2 coord)
{
    return coord.x <= 0 || coord.y <= 0 || coord.x >= int(width) - 1 || coord.y >= int(height) - 1;
}

float2 ComputeWorldPosition(int2 coord)
{
    float2 uv = (float2(coord) + 0.5f) / float2(width, height);
    return float2(
        (uv.x - 0.5f) * field_width,
        (uv.y - 0.5f) * field_depth);
}

float SampleTerrainHeight(int2 coord)
{
    return InBounds(coord) ? g_TerrainHeight.Load(int3(coord, 0)).r : water_height;
}

float4 SampleRain(int2 coord)
{
    return InBounds(coord) ? g_RainMap.Load(int3(coord, 0)) : float4(0.0f, 0.0f, 0.0f, 0.0f);
}

float2 SafeNormalize(float2 v)
{
    float len_sq = dot(v, v);
    return len_sq > 1.0e-6f ? v * rsqrt(len_sq) : float2(1.0f, 0.0f);
}

struct CellState
{
    float terrain_height;
    float previous_water;
    float rain_amount;
    float rain_hint;
    float basin_factor;
    float source_water;
};

CellState LoadCellState(int2 coord)
{
    CellState state = (CellState)0;
    state.terrain_height = SampleTerrainHeight(coord);

    float4 rain_sample = SampleRain(coord);
    const float previous_water_height = InBounds(coord)
        ? g_PreviousTerrainWaterHeight.Load(int3(coord, 0)).g
        : state.terrain_height;

    state.previous_water = max(previous_water_height - state.terrain_height, 0.0f);
    state.rain_amount = rain_sample.r;
    state.rain_hint = rain_sample.g;
    state.basin_factor = ComputeBasinFactor(state.terrain_height);
    state.source_water = 0.0f;
    if (IsEdgeCell(coord))
    {
        state.basin_factor *= 0.15f;
    }

    float retained_rain =
        max(state.rain_amount, 0.0f) *
        lerp(accumulation_rate * 0.010f, accumulation_rate * 0.028f, state.basin_factor);
    const float recharge_drive = saturate(state.rain_amount);
    float basin_recharge = 0.0f;

    float injected_water = 0.0f;
    if (injection_enabled != 0u && injection_radius > 1.0e-4f && injection_amount > 1.0e-4f)
    {
        const float2 world_pos = ComputeWorldPosition(coord);
        const float distance_to_injection =
            length(world_pos - float2(injection_center_x, injection_center_z));
        const float injection_falloff =
            1.0f - smoothstep(injection_radius * 0.30f, injection_radius, distance_to_injection);
        injected_water = injection_amount * saturate(injection_falloff);
    }

    if (presentation_only != 0u)
    {
        const float seeded_water = max(water_height - state.terrain_height, 0.0f);
        state.rain_amount = 0.0f;
        state.rain_hint = 0.0f;
        state.source_water = max(state.previous_water, seeded_water) + injected_water;
        return state;
    }

    state.source_water = max(state.previous_water + retained_rain + basin_recharge + injected_water, 0.0f);
    return state;
}

float4 ComputeDirectionalOutflow(int2 coord)
{
    float4 candidate_outflow = float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (InBounds(coord))
    {
        const CellState center = LoadCellState(coord);
        if (center.source_water > 1.0e-5f)
        {
            const int2 east_coord = coord + int2(1, 0);
            const int2 west_coord = coord + int2(-1, 0);
            const int2 north_coord = coord + int2(0, 1);
            const int2 south_coord = coord + int2(0, -1);

            const CellState east = LoadCellState(east_coord);
            const CellState west = LoadCellState(west_coord);
            const CellState north = LoadCellState(north_coord);
            const CellState south = LoadCellState(south_coord);

            const float surface_center = center.terrain_height + center.source_water;

            if (InBounds(east_coord))
            {
                candidate_outflow.x = max(surface_center - (east.terrain_height + east.source_water), 0.0f);
            }
            else
            {
                candidate_outflow.x = max(surface_center - water_height, 0.0f) * 0.12f;
            }
            if (InBounds(west_coord))
            {
                candidate_outflow.y = max(surface_center - (west.terrain_height + west.source_water), 0.0f);
            }
            else
            {
                candidate_outflow.y = max(surface_center - water_height, 0.0f) * 0.12f;
            }
            if (InBounds(north_coord))
            {
                candidate_outflow.z = max(surface_center - (north.terrain_height + north.source_water), 0.0f);
            }
            else
            {
                candidate_outflow.z = max(surface_center - water_height, 0.0f) * 0.12f;
            }
            if (InBounds(south_coord))
            {
                candidate_outflow.w = max(surface_center - (south.terrain_height + south.source_water), 0.0f);
            }
            else
            {
                candidate_outflow.w = max(surface_center - water_height, 0.0f) * 0.12f;
            }

            candidate_outflow *= downhill_flow_rate * kWaterDeltaTime * kWorldGravity * kPipeArea;

            const float total_candidate =
                candidate_outflow.x +
                candidate_outflow.y +
                candidate_outflow.z +
                candidate_outflow.w;
            if (total_candidate > 1.0e-5f)
            {
                const float movable_water = center.source_water * max_outflow_fraction;
                const float scale = min((1.0f - saturate(flow_damping * 0.08f)) * movable_water / total_candidate, 1.0f);
                candidate_outflow *= scale;
            }
            else
            {
                candidate_outflow = 0.0f;
            }
        }
    }

    return candidate_outflow;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const int2 coord = int2(dispatch_thread_id.xy);

    if (presentation_only != 0u)
    {
        const CellState center = LoadCellState(coord);
        const float water_amount = max(center.source_water, 0.0f);
        const float standing_water = smoothstep(0.03f, 0.22f, water_amount);
        const float depth_preview = smoothstep(0.01f, 0.12f, water_amount);
        const float preview_alpha = saturate(depth_preview * 0.16f + standing_water * 0.28f);
        const float3 preview_color = lerp(
            float3(0.030f, 0.050f, 0.085f),
            float3(0.110f, 0.185f, 0.265f),
            saturate(water_amount * 1.35f));

        g_SurfaceWater[dispatch_thread_id.xy] = float4(water_amount, depth_preview, standing_water, 1.0f);
        g_SurfaceWaterFlow[dispatch_thread_id.xy] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        g_SurfaceWaterFlowPreview[dispatch_thread_id.xy] = float4(preview_color, preview_alpha);
        g_WaterVelocity[dispatch_thread_id.xy] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        g_WaterSediment[dispatch_thread_id.xy] = float4(0.0f, 1.0f, 0.0f, 0.0f);
        return;
    }

    const CellState center = LoadCellState(coord);

    const float4 outflow = ComputeDirectionalOutflow(coord);
    const float incoming =
        ComputeDirectionalOutflow(coord + int2(-1, 0)).x +
        ComputeDirectionalOutflow(coord + int2(1, 0)).y +
        ComputeDirectionalOutflow(coord + int2(0, -1)).z +
        ComputeDirectionalOutflow(coord + int2(0, 1)).w;

    float water_amount =
        max(center.source_water - (outflow.x + outflow.y + outflow.z + outflow.w) + incoming, 0.0f);

    const float evaporation =
        min(
            water_amount,
            lerp(evaporation_rate * 1.35f, evaporation_rate * 0.55f, center.basin_factor) *
                (1.0f - saturate(center.rain_amount * 0.75f)));
    water_amount = max(water_amount - evaporation, 0.0f);

    const float seepage =
        min(
            water_amount,
            seepage_rate *
                (1.0f - center.basin_factor) *
                (0.35f + 0.65f * (1.0f - saturate(center.rain_amount))));
    water_amount = max(water_amount - seepage, 0.0f);

    if (IsEdgeCell(coord))
    {
        const float edge_target_depth = max(water_height - center.terrain_height, 0.0f);
        water_amount = lerp(water_amount, edge_target_depth, 0.10f);
    }

    const float standing_water =
        smoothstep(0.05f, 0.18f, water_amount) *
        lerp(0.35f, 1.0f, center.basin_factor);

    const float2 flow_vector = float2(outflow.x - outflow.y, outflow.w - outflow.z);
    const float total_flux = outflow.x + outflow.y + outflow.z + outflow.w;
    const float flow_strength = saturate(total_flux * 6.0f);
    const float east_terrain = SampleTerrainHeight(coord + int2(1, 0));
    const float west_terrain = SampleTerrainHeight(coord + int2(-1, 0));
    const float north_terrain = SampleTerrainHeight(coord + int2(0, 1));
    const float south_terrain = SampleTerrainHeight(coord + int2(0, -1));
    const float2 terrain_gradient = float2(
        east_terrain - west_terrain,
        north_terrain - south_terrain) * 0.5f;
    const float terrain_slope = saturate(length(terrain_gradient) * 0.55f);
    const float2 water_velocity_xy =
        flow_vector / max(water_amount * 4.0f + 0.04f, 0.08f);
    const float water_speed = saturate(length(water_velocity_xy) * 3.2f);
    const float transport_energy = saturate(water_speed * (0.45f + flow_strength * 0.55f));
    const float sediment_capacity =
        saturate(water_speed * 0.58f + terrain_slope * 0.42f) *
        saturate(water_amount * 2.6f + center.basin_factor * 0.25f);
    const float erosion_tendency =
        saturate(terrain_slope * water_speed * 1.35f + flow_strength * 0.18f);
    const float deposition_tendency =
        saturate((1.0f - water_speed) * (standing_water * 0.72f + center.basin_factor * 0.28f));
    const float suspended_sediment =
        saturate(sediment_capacity * (0.52f + transport_energy * 0.38f));
    const float water_glow = saturate(water_amount * 1.4f + standing_water * 0.65f + center.rain_hint * 0.10f);
    const float2 safe_flow_dir = SafeNormalize(flow_vector);
    const float2 uv = (float2(coord) + 0.5f) / float2(width, height);
    const float4 wind_sample = g_WindField.SampleLevel(g_SurfaceSampler, uv, 0.0f);
    const float2 wind_dir = SafeNormalize(wind_sample.xy * 2.0f - 1.0f);
    const float wind_strength = saturate(wind_sample.z);
    const float2 world_pos = ComputeWorldPosition(coord);
    const float along_wind = dot(world_pos, wind_dir);
    const float cross_wind = dot(world_pos, float2(-wind_dir.y, wind_dir.x));
    const float wind_phase =
        along_wind * 0.040f -
        time_seconds * lerp(0.12f, 0.95f, wind_strength) +
        cross_wind * 0.008f;
    const float wind_ripple_a = 0.5f + 0.5f * sin(wind_phase * 6.2831853f);
    const float wind_ripple_b = 0.5f + 0.5f * sin(wind_phase * 12.5663706f + cross_wind * 0.020f + 0.85f);
    const float wind_glint =
        pow(saturate(wind_ripple_a * 0.78f + wind_ripple_b * 0.22f), 2.4f) *
        smoothstep(0.018f, 0.13f, max(water_amount, standing_water * 0.22f));
    const float flow_sheen =
        pow(saturate(0.5f + 0.5f * dot(safe_flow_dir, wind_dir)), 2.4f) *
        smoothstep(0.02f, 0.10f, water_amount) *
        flow_strength * 0.35f;

    float3 preview_color = lerp(
        float3(0.030f, 0.050f, 0.085f),
        float3(0.110f, 0.185f, 0.265f),
        water_glow);
    const float3 sparkle_color = lerp(float3(0.54f, 0.76f, 0.92f), float3(0.94f, 0.98f, 1.00f), wind_glint);
    preview_color = lerp(preview_color, sparkle_color, wind_glint * (0.35f + wind_strength * 0.45f));
    preview_color += sparkle_color * flow_sheen;
    preview_color = lerp(preview_color, float3(0.82f, 0.92f, 1.0f), standing_water * 0.08f);

    const float preview_alpha =
        saturate(
            max(
                standing_water * 0.28f,
                max(
                    smoothstep(0.03f, 0.11f, water_amount) * 0.16f,
                    wind_glint * (0.18f + wind_strength * 0.26f) + flow_sheen * 0.08f)));

    // WaterV2 step 1:
    //   R = authoritative water depth
    //   G/B/A = debug preview helpers only
    const float depth_preview = smoothstep(0.01f, 0.12f, water_amount);
    g_SurfaceWater[dispatch_thread_id.xy] = float4(water_amount, depth_preview, standing_water, 1.0f);
    g_SurfaceWaterFlow[dispatch_thread_id.xy] = outflow;
    g_SurfaceWaterFlowPreview[dispatch_thread_id.xy] = float4(preview_color, preview_alpha);
    g_WaterVelocity[dispatch_thread_id.xy] = float4(water_velocity_xy, water_speed, transport_energy);
    g_WaterSediment[dispatch_thread_id.xy] = float4(suspended_sediment, deposition_tendency, erosion_tendency, sediment_capacity);
}
