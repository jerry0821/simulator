cbuffer WATER_SURFACE_HEIGHT_CONSTANT_BUFFER : register(b0)
{
    float water_surface_height;
    float minimum_depth_for_surface;
    uint width;
    uint height;
    uint padding0;
    uint padding1;
    uint padding2;
    uint padding3;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D g_SurfaceWater : register(t1);
Texture2D<float2> g_PreviousTerrainWaterHeight : register(t2);
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
    const float2 previous_terrain_water = g_PreviousTerrainWaterHeight.Load(int3(coord, 0));
    const float water_depth = max(surface_water.r, 0.0f);
    // x is the terrain channel and stays terrain-driven.
    const float resolved_terrain_height = terrain_height;
    // y is the water channel and remains an independent surface level.
    // SurfaceWater decides where water is visible; y itself should not inherit terrain peaks.
    const float previous_surface_height = previous_terrain_water.y;
    const float seeded_surface_height = max(previous_surface_height, water_surface_height);
    const float resolved_surface_height =
        water_depth > minimum_depth_for_surface
            ? seeded_surface_height
            : previous_surface_height;

    g_WaterSurfaceHeight[dispatch_thread_id.xy] = float2(resolved_terrain_height, resolved_surface_height);
}
