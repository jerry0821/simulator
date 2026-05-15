cbuffer WATER_SURFACE_HEIGHT_CONSTANT_BUFFER : register(b0)
{
    float water_surface_height;
    float standing_water_flatten_start;
    float standing_water_flatten_end;
    float minimum_depth_for_surface;
    float erosion_strength;
    float deposition_strength;
    float min_terrain_delta;
    float max_terrain_delta;
    uint width;
    uint height;
    uint padding0;
    uint padding1;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D g_SurfaceWater : register(t1);
Texture2D g_ErosionDelta : register(t2);
Texture2D<float2> g_PreviousTerrainWaterHeight : register(t3);
RWTexture2D<float2> g_WaterSurfaceHeight : register(u0);

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
    const float resolved_terrain_height = terrain_height;
    const float resolved_surface_height = max(resolved_terrain_height + water_depth, resolved_terrain_height);

    g_WaterSurfaceHeight[dispatch_thread_id.xy] = float2(resolved_terrain_height, resolved_surface_height);
}
