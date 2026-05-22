cbuffer GRASS_DATA_CONSTANT_BUFFER : register(b0)
{
    float field_width;
    float field_depth;
    uint width;
    uint height;
};

Texture2D g_TerrainNormal : register(t0);
Texture2D<float4> g_WaterSurfaceHeight : register(t1);
Texture2D g_TerrainVegetationSuitability : register(t2);
SamplerState g_GrassDataSampler : register(s0);
RWTexture2D<float4> g_GrassData : register(u0);

static const float kMaxGrassScaling = 1.60f;
static const float kMinGrassScaling = 0.45f;
static const float kMinDryDistance = 0.10f;
static const float kMaxDryDistance = 0.90f;

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 345.45f));
    p += dot(p, p + 34.345f);
    return frac(p.x * p.y);
}

float2 WorldToUv(float2 world_xz)
{
    return float2(
        saturate((world_xz.x + field_width * 0.5f) / field_width),
        saturate((world_xz.y + field_depth * 0.5f) / field_depth));
}

float TerrainNormalYFromPacked(float2 encoded)
{
    const float2 xz = clamp(encoded, -1.0f.xx, 1.0f.xx);
    return sqrt(saturate(1.0f - dot(xz, xz)));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    float2 uv = (float2(dispatch_thread_id.xy) + 0.5f) / float2(width, height);
    float2 world_xz = float2(
        uv.x * field_width - field_width * 0.5f,
        uv.y * field_depth - field_depth * 0.5f);

    float4 normal_sample = g_TerrainNormal.SampleLevel(g_GrassDataSampler, uv, 0.0f);
    float normal_y = saturate(TerrainNormalYFromPacked(normal_sample.xy));
    float4 water_surface_height = g_WaterSurfaceHeight.SampleLevel(g_GrassDataSampler, uv, 0.0f);
    float terrain_height = water_surface_height.x;
    float water_depth = max(water_surface_height.z, 0.0f);
    float vegetation_suitability = g_TerrainVegetationSuitability.SampleLevel(g_GrassDataSampler, uv, 0.0f).r;
    float grass_possibility = 0.0f;
    if (water_depth < 0.05f)
    {
        float slope_factor = saturate(length(normal_sample.xy) * 2.0f);
        grass_possibility = saturate(1.05f - slope_factor * 0.88f);
    }
    grass_possibility *= lerp(0.72f, 1.0f, vegetation_suitability);
    grass_possibility = saturate(grass_possibility);

    float density_hash = Hash21(float2(dispatch_thread_id.xy) + float2(19.7f, 3.1f));
    const float aridity = saturate(1.0f - vegetation_suitability);
    float scaling = lerp(kMaxGrassScaling, kMinGrassScaling, aridity);
    scaling *= lerp(0.90f, 1.05f, vegetation_suitability);

    g_GrassData[dispatch_thread_id.xy] = float4(grass_possibility, density_hash, scaling, 0.0f);
}
