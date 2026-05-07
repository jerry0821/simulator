RWTexture2D<float4> g_Output : register(u0);

cbuffer CS_CLIMATE_FIELD : register(b0)
{
    float g_TimeSeconds;
    float g_TemperatureScale;
    float g_HumidityScale;
    float g_RainfallScale;
    float g_PaddingX;
    float g_PaddingY;
    float g_PaddingZ;
    float g_PaddingW;
    uint g_Width;
    uint g_Height;
    uint g_Padding0;
    uint g_Padding1;
};

float Hash21(float2 p)
{
    p = frac(p * float2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return frac(p.x * p.y);
}

float ValueNoise(float2 p)
{
    float2 cell = floor(p);
    float2 local = frac(p);
    float2 smooth = local * local * (3.0 - 2.0 * local);

    float v00 = Hash21(cell + float2(0.0, 0.0));
    float v10 = Hash21(cell + float2(1.0, 0.0));
    float v01 = Hash21(cell + float2(0.0, 1.0));
    float v11 = Hash21(cell + float2(1.0, 1.0));

    return lerp(lerp(v00, v10, smooth.x), lerp(v01, v11, smooth.x), smooth.y);
}

float FBM(float2 p, float time_offset)
{
    float amplitude = 0.5f;
    float value = 0.0f;
    float2 domain = p;

    [unroll]
    for (int octave = 0; octave < 4; ++octave)
    {
        value += ValueNoise(domain + time_offset) * amplitude;
        domain = domain * 2.03f + float2(17.0f, 9.0f);
        amplitude *= 0.5f;
    }

    return value;
}

float Remap01(float value, float min_value, float max_value)
{
    return saturate((value - min_value) / max(max_value - min_value, 0.0001f));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= g_Width || dispatch_thread_id.y >= g_Height)
    {
        return;
    }

    float2 uv = (dispatch_thread_id.xy + 0.5f) / float2(g_Width, g_Height);
    float2 centered = uv - float2(0.5f, 0.5f);
    float radial = saturate(length(centered) * 1.65f);

    // Large-scale continental pattern: broad climate zones that evolve slowly.
    float base_continent = FBM(uv * (g_TemperatureScale * 0.55f), g_TimeSeconds * 0.0035f);
    float temperature_front = FBM(uv * (g_TemperatureScale * 0.95f) + float2(11.0f, -7.5f), g_TimeSeconds * 0.005f);
    float humidity_front = FBM(uv * (g_HumidityScale * 0.85f) + float2(-13.2f, 18.6f), g_TimeSeconds * 0.004f);
    float pressure_variation = FBM(uv * 2.1f + float2(5.2f, 9.4f), g_TimeSeconds * 0.0065f);
    float rainfall_cells = FBM(uv * (g_RainfallScale * 1.10f) + float2(21.3f, 3.8f), g_TimeSeconds * 0.009f);
    float storm_detail = FBM(uv * (g_RainfallScale * 1.8f) + float2(-8.0f, 14.0f), g_TimeSeconds * 0.013f);

    float warm_bias = 0.30f + base_continent * 0.42f + temperature_front * 0.20f + radial * 0.10f;
    float temperature = saturate(warm_bias);

    // Humidity is broad and inversely related to hotter continental regions,
    // with pressure-like variation adding coherent local differences.
    float humidity_base = 0.28f + humidity_front * 0.44f + pressure_variation * 0.18f - temperature * 0.18f;
    float humidity = saturate(humidity_base);

    // Rainfall is derived from humidity first, then shaped into patchy storm cells.
    float condensation = Remap01(humidity - temperature * 0.22f, 0.18f, 0.72f);
    float storm_mask = Remap01(rainfall_cells + storm_detail * 0.35f, 0.42f, 0.82f);
    float rainfall = saturate(condensation * 0.72f + storm_mask * humidity * 0.38f - temperature * 0.08f);

    g_Output[dispatch_thread_id.xy] = float4(temperature, humidity, rainfall, 1.0f);
}
