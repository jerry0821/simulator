cbuffer FINAL_TERRAIN_HEIGHT_CONSTANT_BUFFER : register(b0)
{
    float erosion_strength;
    float deposition_strength;
    float min_height;
    float max_height;
    float min_delta;
    float max_delta;
    uint width;
    uint height;
};

Texture2D<float2> g_BaseTerrainHeight : register(t0);
Texture2D g_ErosionDelta : register(t1);
RWTexture2D<float> g_FinalTerrainHeight : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    float base_height = g_BaseTerrainHeight.Load(int3(dispatch_thread_id.xy, 0)).r;
    float final_height = clamp(base_height, min_height, max_height);

    g_FinalTerrainHeight[dispatch_thread_id.xy] = final_height;
}
