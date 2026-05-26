#ifndef SHADER_WATER_COMMON_HLSLI
#define SHADER_WATER_COMMON_HLSLI

static const float kWorldSideLength = 2048.0f;
static const float kWorldHalfExtent = kWorldSideLength * 0.5f;
static const float kWaterMeshInterval = 4.0f;

float2 WorldToFieldUv(float2 world_xz)
{
    return saturate((world_xz + kWorldHalfExtent.xx) / kWorldSideLength);
}

float2 ClampFieldUv(float2 uv)
{
    uint tex_width = 0;
    uint tex_height = 0;
    g_TerrainHeight.GetDimensions(tex_width, tex_height);
    const float2 texel_size = 1.0f / max(float2(tex_width, tex_height), 1.0f.xx);
    const float2 half_texel = texel_size * 0.5f;
    return clamp(uv, half_texel, 1.0f.xx - half_texel);
}

int2 WorldToFieldCoord(float2 world_xz)
{
    uint tex_width = 0;
    uint tex_height = 0;
    g_TerrainHeight.GetDimensions(tex_width, tex_height);
    tex_width = max(tex_width, 1u);
    tex_height = max(tex_height, 1u);

    const float2 tex_size = float2(tex_width, tex_height);
    const float2 data_coord = floor((world_xz + kWorldHalfExtent.xx) * (tex_size / kWorldSideLength));
    return clamp(int2(data_coord), int2(0, 0), int2(int(tex_width) - 1, int(tex_height) - 1));
}

float2 LoadTerrainWater(float2 world_xz)
{
    return g_TerrainHeight.Load(int3(WorldToFieldCoord(world_xz), 0)).xy;
}

float2 SampleTerrainWater(SamplerState data_sampler, float2 world_xz)
{
    return g_TerrainHeight.SampleLevel(data_sampler, ClampFieldUv(WorldToFieldUv(world_xz)), 0.0f).xy;
}

float4 SampleTerrain4(Texture2D<float4> data, SamplerState data_sampler, float2 world_xz)
{
    return data.SampleLevel(data_sampler, ClampFieldUv(WorldToFieldUv(world_xz)), 0.0f);
}

float3 ReconstructUpNormal(float2 encoded)
{
    const float2 xz = clamp(encoded, -1.0f.xx, 1.0f.xx);
    const float y = sqrt(saturate(1.0f - dot(xz, xz)));
    return float3(xz.x, y, xz.y);
}

#endif // SHADER_WATER_COMMON_HLSLI
