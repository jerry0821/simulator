cbuffer TERRAIN_NORMAL_CONSTANT_BUFFER : register(b0)
{
    float field_width;
    float field_depth;
    float time_seconds;
    float wind_ripple_strength;
    uint width;
    uint height;
    float wind_ripple_frequency;
    float wind_ripple_speed;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D<float4> g_WaterVelocity : register(t1);
SamplerState g_TerrainSampler : register(s0);
RWTexture2D<float4> g_TerrainNormal : register(u0);

int2 ClampCoord(int2 coord)
{
    return clamp(coord, int2(0, 0), int2(int(width) - 1, int(height) - 1));
}

float2 SampleTerrainWaterHeight(int2 coord)
{
    const float4 sample_value = g_TerrainHeight.Load(int3(ClampCoord(coord), 0));
    const float terrain_height = sample_value.x;
    return sample_value.xy;
}

float2 EncodeUpNormal(float3 normal)
{
    return normal.xz;
}

float3 CalculateUpNormal(float center_height, float right_height, float down_height)
{
    const float cell_size_x = field_width / max(float(width), 1.0f);
    const float cell_size_z = field_depth / max(float(height), 1.0f);
    const float2 delta_height = float2(
        center_height - right_height,
        center_height - down_height) /
        max(float2(cell_size_x, cell_size_z), 1.0e-4f.xx);
    return normalize(float3(delta_height.x, 1.0f, delta_height.y));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const int2 coord = int2(dispatch_thread_id.xy);
    const float2 height = SampleTerrainWaterHeight(coord);
    const float2 height_r = SampleTerrainWaterHeight(coord + int2(1, 0));
    const float2 height_d = SampleTerrainWaterHeight(coord + int2(0, 1));

    const float3 terrain_normal = CalculateUpNormal(height.x, height_r.x, height_d.x);
    const float3 water_normal = CalculateUpNormal(height.y, height_r.y, height_d.y);

    g_TerrainNormal[dispatch_thread_id.xy] = float4(
        EncodeUpNormal(terrain_normal),
        EncodeUpNormal(water_normal));
}
