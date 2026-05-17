cbuffer WATER_SURFACE_HEIGHT_CONSTANT_BUFFER : register(b0)
{
    float water_surface_height;
    float minimum_depth_for_surface;
    float flow_rate;
    float max_outflow_fraction;
    float evaporation_rate;
    uint width;
    uint height;
    uint initialize_from_water_level;
    uint padding1;
    uint padding2;
    uint padding3;
    uint padding4;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D<float2> g_PreviousTerrainWaterHeight : register(t1);
Texture2D g_RainMap : register(t2);
RWTexture2D<float2> g_WaterSurfaceHeight : register(u0);

bool InBounds(int2 coord)
{
    return coord.x >= 0 && coord.y >= 0 && coord.x < int(width) && coord.y < int(height);
}

float SampleTerrainHeight(int2 coord)
{
    return InBounds(coord)
        ? g_TerrainHeight.Load(int3(coord, 0)).r
        : water_surface_height;
}

float SamplePreviousWaterHeight(int2 coord)
{
    return InBounds(coord)
        ? g_PreviousTerrainWaterHeight.Load(int3(coord, 0)).y
        : water_surface_height;
}

float SampleRainAmount(int2 coord)
{
    return InBounds(coord)
        ? max(g_RainMap.Load(int3(coord, 0)).r, 0.0f)
        : 0.0f;
}

float ComputePreviousWaterDepth(int2 coord)
{
    const float terrain_height = SampleTerrainHeight(coord);
    const float previous_surface_height = SamplePreviousWaterHeight(coord);
    return max(previous_surface_height - terrain_height, 0.0f);
}

float ComputePreviousSurfaceHeight(int2 coord)
{
    const float terrain_height = SampleTerrainHeight(coord);
    const float previous_surface_height = SamplePreviousWaterHeight(coord);
    return max(previous_surface_height, terrain_height);
}

float4 ComputeDirectionalOutflow(int2 coord)
{
    float4 candidate = 0.0f.xxxx;
    if (!InBounds(coord))
    {
        return 0.0f;
    }

    const float available_water = ComputePreviousWaterDepth(coord);
    if (available_water <= 1.0e-5f)
    {
        return 0.0f;
    }

    const float surface_center = ComputePreviousSurfaceHeight(coord);
    candidate = float4(
        max(surface_center - ComputePreviousSurfaceHeight(coord + int2(1, 0)), 0.0f),
        max(surface_center - ComputePreviousSurfaceHeight(coord + int2(-1, 0)), 0.0f),
        max(surface_center - ComputePreviousSurfaceHeight(coord + int2(0, 1)), 0.0f),
        max(surface_center - ComputePreviousSurfaceHeight(coord + int2(0, -1)), 0.0f));

    candidate *= flow_rate;

    const float total_candidate = candidate.x + candidate.y + candidate.z + candidate.w;
    if (total_candidate <= 1.0e-5f)
    {
        return 0.0f;
    }

    const float movable_water = available_water * max_outflow_fraction;
    const float scale = min(movable_water / total_candidate, 1.0f);
    return candidate * scale;
}

bool IsEdgeCell(int2 coord)
{
    return coord.x == 0 || coord.y == 0 || coord.x == int(width) - 1 || coord.y == int(height) - 1;
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
    // y is the water channel and remains an independent surface level.
    // Initialization is an explicit one-time step. After that, y advances only from the
    // previous shared water state; later hydrology terms (rain / flow / loss) should become
    // explicit additions to this update instead of hidden terrain-driven reseeds.
    const float initialized_surface_height = water_surface_height;
    const float previous_water = ComputePreviousWaterDepth(coord);
    const float4 outflow = ComputeDirectionalOutflow(coord);
    const float incoming =
        ComputeDirectionalOutflow(coord + int2(-1, 0)).x +
        ComputeDirectionalOutflow(coord + int2(1, 0)).y +
        ComputeDirectionalOutflow(coord + int2(0, -1)).z +
        ComputeDirectionalOutflow(coord + int2(0, 1)).w;
    const float rain_injection = SampleRainAmount(coord) * 0.035f;
    const float edge_drainage = IsEdgeCell(coord) ? min(previous_water, 0.04f) : 0.0f;
    const float evaporation_loss = min(previous_water, evaporation_rate + edge_drainage);
    const float next_water =
        initialize_from_water_level != 0u
            ? max(initialized_surface_height - terrain_height, 0.0f)
            : max(
                previous_water -
                dot(outflow, 1.0f) +
                incoming +
                rain_injection -
                evaporation_loss,
                0.0f);
    const float resolved_surface_height = terrain_height + next_water;

    g_WaterSurfaceHeight[dispatch_thread_id.xy] = float2(resolved_terrain_height, resolved_surface_height);
}
