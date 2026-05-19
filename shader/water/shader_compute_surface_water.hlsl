cbuffer SURFACE_WATER_CONSTANT_BUFFER : register(b0)
{
    float time_seconds;
    float field_width;
    float field_depth;
    uint width;
    uint height;
    uint padding0;
    uint padding1;
    uint padding2;
};

Texture2D<float4> g_WaterSurfaceHeight : register(t0);
Texture2D<float4> g_AuthoritativeWaterFlow : register(t1);
Texture2D g_WindField : register(t2);
SamplerState g_SurfaceSampler : register(s0);
RWTexture2D<float4> g_SurfaceWater : register(u0);
RWTexture2D<float4> g_SurfaceWaterFlow : register(u1);
RWTexture2D<float4> g_SurfaceWaterFlowPreview : register(u2);
RWTexture2D<float4> g_WaterVelocity : register(u3);
RWTexture2D<float4> g_WaterSediment : register(u4);

bool InBounds(int2 coord)
{
    return coord.x >= 0 && coord.y >= 0 && coord.x < int(width) && coord.y < int(height);
}

int2 ClampCoord(int2 coord)
{
    return clamp(coord, int2(0, 0), int2(int(width) - 1, int(height) - 1));
}

float2 LoadTerrainWater(int2 coord)
{
    return g_WaterSurfaceHeight.Load(int3(ClampCoord(coord), 0)).xy;
}

float LoadDepth(int2 coord)
{
    float2 terrain_water = LoadTerrainWater(coord);
    return max(terrain_water.y - terrain_water.x, 0.0f);
}

float4 LoadFlow(int2 coord)
{
    return max(g_AuthoritativeWaterFlow.Load(int3(ClampCoord(coord), 0)), 0.0f.xxxx);
}

float2 ComputeWorldPosition(int2 coord)
{
    float2 uv = (float2(coord) + 0.5f) / float2(width, height);
    return float2(
        (uv.x - 0.5f) * field_width,
        (uv.y - 0.5f) * field_depth);
}

float2 SafeNormalize(float2 value)
{
    float len_sq = dot(value, value);
    return len_sq > 1.0e-6f ? value * rsqrt(len_sq) : float2(1.0f, 0.0f);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const int2 coord = int2(dispatch_thread_id.xy);
    const float2 terrain_water = LoadTerrainWater(coord);
    const float water_depth = max(terrain_water.y - terrain_water.x, 0.0f);
    const float4 flow = LoadFlow(coord);
    const float2 flow_vector = float2(flow.x - flow.y, flow.w - flow.z);
    const float total_flux = dot(flow, 1.0f.xxxx);

    const float east_terrain = LoadTerrainWater(coord + int2(1, 0)).x;
    const float west_terrain = LoadTerrainWater(coord + int2(-1, 0)).x;
    const float north_terrain = LoadTerrainWater(coord + int2(0, 1)).x;
    const float south_terrain = LoadTerrainWater(coord + int2(0, -1)).x;
    const float2 terrain_gradient = float2(
        east_terrain - west_terrain,
        north_terrain - south_terrain) * 0.5f;
    const float terrain_slope = saturate(length(terrain_gradient) * 0.55f);

    const float2 water_velocity_xy =
        flow_vector / max(water_depth * 4.0f + 0.04f, 0.08f);
    const float water_speed = saturate(length(water_velocity_xy) * 3.5f);
    const float transport_energy = saturate(water_speed * (0.55f + saturate(total_flux * 5.0f) * 0.45f));

    const float standing_water = smoothstep(0.03f, 0.18f, water_depth);
    const float runoff = smoothstep(0.004f, 0.045f, total_flux);
    const float depth_preview = smoothstep(0.01f, 0.12f, water_depth);

    const float sediment_capacity =
        saturate(water_speed * 0.62f + terrain_slope * 0.38f) *
        saturate(water_depth * 2.8f + 0.12f);
    const float erosion_tendency =
        saturate(terrain_slope * water_speed * 1.2f + runoff * 0.22f);
    const float deposition_tendency =
        saturate((1.0f - water_speed) * (standing_water * 0.70f + 0.18f));
    const float suspended_sediment =
        saturate(sediment_capacity * (0.55f + transport_energy * 0.35f));

    const float2 uv = (float2(coord) + 0.5f) / float2(width, height);
    const float4 wind_sample = g_WindField.SampleLevel(g_SurfaceSampler, uv, 0.0f);
    const float2 wind_dir = SafeNormalize(wind_sample.xy * 2.0f - 1.0f);
    const float wind_strength = saturate(wind_sample.z);
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
        smoothstep(0.015f, 0.10f, max(water_depth, runoff * 0.08f));
    const float flow_sheen =
        pow(saturate(0.5f + 0.5f * dot(SafeNormalize(flow_vector), wind_dir)), 2.0f) *
        water_speed * 0.30f;

    float3 preview_color = lerp(
        float3(0.035f, 0.075f, 0.120f),
        float3(0.140f, 0.235f, 0.325f),
        saturate(water_depth * 1.45f + water_speed * 0.22f));
    preview_color = lerp(preview_color, float3(0.82f, 0.90f, 0.98f), wind_glint * 0.48f);
    preview_color += float3(0.16f, 0.20f, 0.24f) * flow_sheen;

    const float preview_alpha =
        saturate(max(standing_water * 0.24f, runoff * 0.30f) + wind_glint * 0.18f);

    g_SurfaceWater[dispatch_thread_id.xy] = float4(water_depth, depth_preview, standing_water, runoff);
    g_SurfaceWaterFlow[dispatch_thread_id.xy] = flow;
    g_SurfaceWaterFlowPreview[dispatch_thread_id.xy] = float4(preview_color, preview_alpha);
    g_WaterVelocity[dispatch_thread_id.xy] = float4(water_velocity_xy, water_speed, transport_energy);
    g_WaterSediment[dispatch_thread_id.xy] = float4(
        suspended_sediment,
        deposition_tendency,
        erosion_tendency,
        sediment_capacity);
}
