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

Texture2D<float4> g_Meteorograph : register(t0);
SamplerState g_ClimateSampler : register(s0);
RWTexture2D<float4> g_RainMap : register(u0);

float Smoothstep01(float v)
{
    float t = saturate(v);
    return t * t * (3.0 - 2.0 * t);
}

float2 SafeNormalize(float2 value)
{
    const float len_sq = dot(value, value);
    return len_sq > 1.0e-6f ? value * rsqrt(len_sq) : float2(1.0f, 0.0f);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const float2 uv = (float2(dispatch_thread_id.xy) + 0.5f) / float2(width, height);
    const float2 texel = 1.0f / float2(width, height);

    const float4 meteo_center = g_Meteorograph.SampleLevel(g_ClimateSampler, uv, 0.0f);
    const float4 meteo_right = g_Meteorograph.SampleLevel(g_ClimateSampler, uv + float2(texel.x, 0.0f), 0.0f);
    const float4 meteo_left = g_Meteorograph.SampleLevel(g_ClimateSampler, uv + float2(-texel.x, 0.0f), 0.0f);
    const float4 meteo_up = g_Meteorograph.SampleLevel(g_ClimateSampler, uv + float2(0.0f, texel.y), 0.0f);
    const float4 meteo_down = g_Meteorograph.SampleLevel(g_ClimateSampler, uv + float2(0.0f, -texel.y), 0.0f);

    const float humidity_center = saturate(meteo_center.z);
    const float temperature_center = saturate(meteo_center.w);
    const float2 wind_velocity = meteo_center.xy;
    const float wind_strength = saturate(length(wind_velocity));
    const float2 wind_dir = SafeNormalize(wind_velocity);

    float humidity_sum = humidity_center * 0.40f;
    humidity_sum += saturate(meteo_right.z) * 0.15f;
    humidity_sum += saturate(meteo_left.z) * 0.15f;
    humidity_sum += saturate(meteo_up.z) * 0.15f;
    humidity_sum += saturate(meteo_down.z) * 0.15f;

    const float temperature_average =
        (temperature_center +
         saturate(meteo_right.w) +
         saturate(meteo_left.w) +
         saturate(meteo_up.w) +
         saturate(meteo_down.w)) * 0.20f;
    const float humidity_gradient =
        abs(meteo_right.z - meteo_left.z) + abs(meteo_up.z - meteo_down.z);
    const float wind_alignment = dot(wind_dir, float2(0.7071f, -0.7071f));
    const float advection =
        wind_alignment * rain_advection * 0.04f +
        humidity_gradient * 0.10f +
        wind_strength * 0.06f;

    float condensation =
        saturate((humidity_sum - temperature_average * 0.24f - rain_threshold + advection) * rain_contrast);
    const float storminess =
        saturate(humidity_center * 0.65f + wind_strength * 0.24f - temperature_center * 0.14f);
    float rain = condensation * (0.45f + storminess * 0.55f);
    rain *= max(rain_multiplier, 0.0f);
    rain = max(rain, saturate(force_rain * humidity_center));
    rain = Smoothstep01(rain);

    const float wet_hint =
        saturate(rain * 0.62f + humidity_center * 0.28f + wind_strength * 0.10f);
    g_RainMap[dispatch_thread_id.xy] = float4(rain, wet_hint, humidity_center, 1.0f);
}
