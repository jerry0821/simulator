cbuffer WATER_SURFACE_HEIGHT_CONSTANT_BUFFER : register(b0)
{
    float water_surface_height;
    float minimum_depth_for_surface;
    float padding0;
    float padding1;
    float padding2;
    uint width;
    uint height;
    uint initialize_from_water_level;
    uint padding3;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D<float4> g_SurfaceWater : register(t1);
RWTexture2D<float2> g_WaterSurfaceHeight : register(u0);

int2 ClampCoord(int2 coord)
{
    return clamp(coord, int2(0, 0), int2(int(width) - 1, int(height) - 1));
}

float SampleTerrainHeight(int2 coord)
{
    return g_TerrainHeight.Load(int3(ClampCoord(coord), 0)).r;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const int2 coord = int2(dispatch_thread_id.xy);
    const float terrain_height = SampleTerrainHeight(coord);
    // x is the terrain channel and stays terrain-driven.
    const float resolved_terrain_height = terrain_height;
    const float initialized_surface_height = water_surface_height;
    const float surface_water_amount =
        initialize_from_water_level != 0u
            ? max(initialized_surface_height - terrain_height, 0.0f)
            : max(g_SurfaceWater.Load(int3(ClampCoord(coord), 0)).r, 0.0f);
    const float clamped_surface_water_amount =
        surface_water_amount < minimum_depth_for_surface ? 0.0f : surface_water_amount;
    const float resolved_surface_height = terrain_height + clamped_surface_water_amount;

    g_WaterSurfaceHeight[dispatch_thread_id.xy] = float2(resolved_terrain_height, resolved_surface_height);
}
