cbuffer GRASS_DATA_CONSTANT_BUFFER : register(b0)
{
    float field_width;
    float field_depth;
    uint width;
    uint height;
};

Texture2D g_TerrainNormal : register(t0);
Texture2D g_TerrainVegetationSuitability : register(t1);
Texture2D g_TerrainSurfaceData : register(t2);
SamplerState g_GrassDataSampler : register(s0);
RWTexture2D<float4> g_GrassData : register(u0);

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
    float normal_y = saturate(normal_sample.w);
    float vegetation_suitability = g_TerrainVegetationSuitability.SampleLevel(g_GrassDataSampler, uv, 0.0f).r;
    float4 surface_data = g_TerrainSurfaceData.SampleLevel(g_GrassDataSampler, uv, 0.0f);
    float slope = saturate(surface_data.r);
    float beach = saturate(surface_data.g);
    float humidity = saturate(surface_data.b);
    float roughness = saturate(surface_data.a);

    float slope_support = smoothstep(0.42f, 0.76f, normal_y);
    float base_possibility =
        vegetation_suitability *
        lerp(1.0f, 0.18f, slope) *
        lerp(1.0f, 0.52f, beach);
    float possibility =
        base_possibility *
        lerp(0.88f, 1.0f, vegetation_suitability) *
        lerp(0.90f, 1.0f, slope_support) *
        lerp(0.76f, 1.0f, humidity) *
        lerp(1.0f, 0.92f, roughness);
    possibility = saturate(possibility);

    float density_hash = Hash21(float2(dispatch_thread_id.xy) + float2(19.7f, 3.1f));
    float aridity = saturate((1.0f - humidity) * 0.75f + roughness * 0.25f);
    float scaling = lerp(1.30f, 0.72f, aridity);
    scaling *= lerp(0.92f, 1.08f, vegetation_suitability);

    g_GrassData[dispatch_thread_id.xy] = float4(possibility, density_hash, scaling, 0.0f);
}
