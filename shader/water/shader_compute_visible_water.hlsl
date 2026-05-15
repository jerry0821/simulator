cbuffer VISIBLE_WATER_CONSTANT_BUFFER : register(b0)
{
    float runoff_depth_min;
    float visible_depth_min;
    float standing_water_min;
    float slope_suppress_start;
    float slope_suppress_end;
    float flow_runoff_scale;
    float flow_visibility_suppress;
    float padding0;
    uint width;
    uint height;
    uint padding1;
    uint padding2;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D g_SurfaceWater : register(t1);
Texture2D g_SurfaceWaterFlow : register(t2);
SamplerState g_SurfaceSampler : register(s0);
RWTexture2D<float4> g_VisibleWater : register(u0);

float Smoothstep01(float v)
{
    float t = saturate(v);
    return t * t * (3.0f - 2.0f * t);
}

float SampleTerrain(int2 coord)
{
    return g_TerrainHeight.Load(int3(coord, 0)).r;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const int2 coord = int2(dispatch_thread_id.xy);
    const int2 left_coord = int2(max(coord.x - 1, 0), coord.y);
    const int2 right_coord = int2(min(coord.x + 1, int(width) - 1), coord.y);
    const int2 up_coord = int2(coord.x, max(coord.y - 1, 0));
    const int2 down_coord = int2(coord.x, min(coord.y + 1, int(height) - 1));

    const float4 surface = g_SurfaceWater.Load(int3(coord, 0));
    const float4 flow = g_SurfaceWaterFlow.Load(int3(coord, 0));

    const float terrain_center = SampleTerrain(coord);
    const float terrain_left = SampleTerrain(left_coord);
    const float terrain_right = SampleTerrain(right_coord);
    const float terrain_up = SampleTerrain(up_coord);
    const float terrain_down = SampleTerrain(down_coord);

    const float slope =
        max(abs(terrain_right - terrain_left), abs(terrain_down - terrain_up));
    const float slope_keep =
        1.0f - Smoothstep01(saturate((slope - slope_suppress_start) / max(slope_suppress_end - slope_suppress_start, 1.0e-4f)));

    const float water_depth = surface.r;
    const float standing_water = surface.b;
    const float flow_strength = saturate((flow.x + flow.y + flow.z + flow.w) * 5.0f);

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

    g_VisibleWater[dispatch_thread_id.xy] = float4(
        visible_water,
        runoff_hint,
        pooled_preview,
        preview_alpha);
}
