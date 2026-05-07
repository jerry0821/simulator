cbuffer WATER_SURFACE_HEIGHT_CONSTANT_BUFFER : register(b0)
{
    float water_surface_height;
    float standing_water_flatten_start;
    float standing_water_flatten_end;
    float minimum_depth_for_surface;
    uint width;
    uint height;
    uint padding0;
    uint padding1;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D g_SurfaceWater : register(t1);
RWTexture2D<float> g_WaterSurfaceHeight : register(u0);

float Smoothstep01(float value)
{
    float t = saturate(value);
    return t * t * (3.0f - 2.0f * t);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const int2 coord = int2(dispatch_thread_id.xy);
    const float terrain_height = g_TerrainHeight.Load(int3(coord, 0)).r;
    const float4 surface_water = g_SurfaceWater.Load(int3(coord, 0));

    const float water_depth = max(surface_water.r, 0.0f);
    const float standing_water = saturate(surface_water.b);
    const float base_surface_height = water_surface_height;
    const float dynamic_surface_height = terrain_height + water_depth;
    const float flattened_lake_height = max(base_surface_height, dynamic_surface_height);
    const float flatten_amount =
        Smoothstep01(
            saturate(
                (standing_water - standing_water_flatten_start) /
                max(standing_water_flatten_end - standing_water_flatten_start, 1.0e-4f)));
    const float visible_depth_amount =
        Smoothstep01(saturate((water_depth - minimum_depth_for_surface) / 0.12f));
    const float surface_presence = saturate(max(flatten_amount, visible_depth_amount * 0.30f));

    const float resolved_shaped_height =
        lerp(dynamic_surface_height, flattened_lake_height, flatten_amount);
    float resolved_surface_height =
        lerp(base_surface_height, resolved_shaped_height, surface_presence);

    if (water_depth <= minimum_depth_for_surface && standing_water <= 1.0e-4f)
    {
        resolved_surface_height = base_surface_height;
    }

    g_WaterSurfaceHeight[dispatch_thread_id.xy] = resolved_surface_height;
}
