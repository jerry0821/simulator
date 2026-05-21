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
Texture2D<float4> g_Meteorograph : register(t3);
SamplerState g_GrassDataSampler : register(s0);
RWTexture2D<float4> g_GrassData : register(u0);

static const float kPolarTemperature = -10.0f;
static const float kEquatorialTemperature = 30.0f;

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

float NormalizeClimateTemperature(float temperature_celsius)
{
    return saturate((temperature_celsius - kPolarTemperature) / max(kEquatorialTemperature - kPolarTemperature, 1.0e-4f));
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
    float vegetation_suitability = g_TerrainVegetationSuitability.SampleLevel(g_GrassDataSampler, uv, 0.0f).r;
    float4 surface_data = g_TerrainSurfaceData.SampleLevel(g_GrassDataSampler, uv, 0.0f);
    float slope = saturate(surface_data.r);
    float beach = saturate(surface_data.g);
    float humidity = saturate(surface_data.b);
    float roughness = saturate(surface_data.a);
    float4 meteorograph = g_Meteorograph.SampleLevel(g_GrassDataSampler, uv, 0.0f);
    float climate_humidity = saturate(meteorograph.z);
    float climate_temperature = NormalizeClimateTemperature(meteorograph.w);
    float wind_strength = saturate(length(meteorograph.xy));

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
    possibility *= lerp(0.82f, 1.04f, climate_humidity);
    possibility *= lerp(0.90f, 1.02f, 1.0f - abs(climate_temperature - 0.52f) * 1.35f);
    possibility = saturate(possibility);

    float density_hash = Hash21(float2(dispatch_thread_id.xy) + float2(19.7f, 3.1f));
    float aridity = saturate((1.0f - humidity) * 0.75f + roughness * 0.25f);
    float scaling = lerp(1.30f, 0.72f, aridity);
    scaling *= lerp(0.92f, 1.08f, vegetation_suitability);
    scaling *= lerp(0.92f, 1.08f, climate_humidity);
    scaling *= lerp(1.00f, 0.94f, wind_strength);

    g_GrassData[dispatch_thread_id.xy] = float4(possibility, density_hash, scaling, 0.0f);
}
