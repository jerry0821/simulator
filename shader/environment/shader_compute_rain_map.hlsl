cbuffer RAIN_MAP_CONSTANT_BUFFER : register(b0)
{
    float time_seconds;
    float rain_threshold;
    float rain_contrast;
    float rain_advection;
    float rain_multiplier;
    float force_rain;
    uint width;
    uint height;
    uint padding0;
    uint padding1;
    uint padding2;
    uint padding3;
};

Texture2D g_ClimateField : register(t0);
Texture2D g_WindField : register(t1);
SamplerState g_ClimateSampler : register(s0);
RWTexture2D<float4> g_RainMap : register(u0);

float Smoothstep01(float v)
{
    float t = saturate(v);
    return t * t * (3.0 - 2.0 * t);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    float2 uv = (float2(dispatch_thread_id.xy) + 0.5f) / float2(width, height);
    float2 texel = 1.0f / float2(width, height);
    float4 climate_center = g_ClimateField.SampleLevel(g_ClimateSampler, uv, 0.0f);
    float humidity_center = climate_center.g;

    float humidity_sum = humidity_center * 0.40f;
    humidity_sum += g_ClimateField.SampleLevel(g_ClimateSampler, uv + float2(texel.x, 0.0f), 0.0f).g * 0.15f;
    humidity_sum += g_ClimateField.SampleLevel(g_ClimateSampler, uv + float2(-texel.x, 0.0f), 0.0f).g * 0.15f;
    humidity_sum += g_ClimateField.SampleLevel(g_ClimateSampler, uv + float2(0.0f, texel.y), 0.0f).g * 0.15f;
    humidity_sum += g_ClimateField.SampleLevel(g_ClimateSampler, uv + float2(0.0f, -texel.y), 0.0f).g * 0.15f;

    float rain = saturate((humidity_sum - rain_threshold) * rain_contrast);
    rain *= max(rain_multiplier, 0.0f);
    rain = max(rain, saturate(force_rain * humidity_center));
    rain = Smoothstep01(rain);

    float wet_hint = saturate(rain * 0.65f + humidity_center * 0.35f);
    g_RainMap[dispatch_thread_id.xy] = float4(rain, wet_hint, humidity_center, 1.0f);
}
