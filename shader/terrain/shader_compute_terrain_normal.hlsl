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

float2 SampleTerrainWaterHeight(int2 coord)
{
    coord.x = clamp(coord.x, 0, int(width) - 1);
    coord.y = clamp(coord.y, 0, int(height) - 1);
    const float4 sample_value = g_TerrainHeight.Load(int3(coord, 0));
    const float terrain_height = sample_value.x;
    const float water_surface_height = max(sample_value.y, terrain_height);
    return float2(terrain_height, water_surface_height);
}

float2 EncodeUpNormal(float3 normal)
{
    return normal.xz;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const int2 coord = int2(dispatch_thread_id.xy);
    const float2 left_height = SampleTerrainWaterHeight(coord + int2(-1, 0));
    const float2 right_height = SampleTerrainWaterHeight(coord + int2(1, 0));
    const float2 up_height = SampleTerrainWaterHeight(coord + int2(0, -1));
    const float2 down_height = SampleTerrainWaterHeight(coord + int2(0, 1));

    const float cell_size_x = field_width / max(float(width - 1), 1.0f);
    const float cell_size_z = field_depth / max(float(height - 1), 1.0f);

    const float3 terrain_tangent_x = float3(cell_size_x * 2.0f, right_height.x - left_height.x, 0.0f);
    const float3 terrain_tangent_z = float3(0.0f, down_height.x - up_height.x, cell_size_z * 2.0f);
    const float3 terrain_normal = normalize(cross(terrain_tangent_z, terrain_tangent_x));

    const float3 water_tangent_x = float3(cell_size_x * 2.0f, right_height.y - left_height.y, 0.0f);
    const float3 water_tangent_z = float3(0.0f, down_height.y - up_height.y, cell_size_z * 2.0f);
    const float3 water_normal = normalize(cross(water_tangent_z, water_tangent_x));

    g_TerrainNormal[dispatch_thread_id.xy] = float4(
        EncodeUpNormal(terrain_normal),
        EncodeUpNormal(water_normal));
}
