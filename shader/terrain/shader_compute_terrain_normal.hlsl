cbuffer TERRAIN_NORMAL_CONSTANT_BUFFER : register(b0)
{
    float field_width;
    float field_depth;
    uint width;
    uint height;
};

Texture2D g_TerrainHeight : register(t0);
SamplerState g_TerrainSampler : register(s0);
RWTexture2D<float4> g_TerrainNormal : register(u0);

float SampleHeight(int2 coord)
{
    coord.x = clamp(coord.x, 0, int(width) - 1);
    coord.y = clamp(coord.y, 0, int(height) - 1);
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
    const float left_height = SampleHeight(coord + int2(-1, 0));
    const float right_height = SampleHeight(coord + int2(1, 0));
    const float up_height = SampleHeight(coord + int2(0, -1));
    const float down_height = SampleHeight(coord + int2(0, 1));

    const float cell_size_x = field_width / max(float(width - 1), 1.0f);
    const float cell_size_z = field_depth / max(float(height - 1), 1.0f);

    const float3 tangent_x = float3(cell_size_x * 2.0f, right_height - left_height, 0.0f);
    const float3 tangent_z = float3(0.0f, down_height - up_height, cell_size_z * 2.0f);
    const float3 normal = normalize(cross(tangent_z, tangent_x));

    g_TerrainNormal[dispatch_thread_id.xy] = float4(normal, saturate(normal.y));
}
