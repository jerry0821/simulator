cbuffer SOIL_MOISTURE_CONSTANT_BUFFER : register(b0)
{
    float rain_absorption;
    float wetness_absorption;
    float shoreline_absorption;
    float evaporation_rate;
    float diffusion_rate;
    float saturation_decay;
    uint width;
    uint height;
};

Texture2D g_RainMap : register(t0);
Texture2D g_SurfaceWater : register(t1);
Texture2D g_WaterMask : register(t2);
Texture2D g_PreviousSoilMoisture : register(t3);
SamplerState g_SoilSampler : register(s0);
RWTexture2D<float4> g_SoilMoisture : register(u0);

float SamplePreviousMoisture(float2 uv)
{
    return g_PreviousSoilMoisture.SampleLevel(g_SoilSampler, saturate(uv), 0.0f).r;
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

    float4 rain_sample = g_RainMap.SampleLevel(g_SoilSampler, saturate(uv), 0.0f);
    float4 surface_sample = g_SurfaceWater.SampleLevel(g_SoilSampler, saturate(uv), 0.0f);
    float4 mask_sample = g_WaterMask.SampleLevel(g_SoilSampler, saturate(uv), 0.0f);

    float previous = SamplePreviousMoisture(uv);
    float neighbor_average =
        SamplePreviousMoisture(uv + float2(texel.x, 0.0f)) +
        SamplePreviousMoisture(uv + float2(-texel.x, 0.0f)) +
        SamplePreviousMoisture(uv + float2(0.0f, texel.y)) +
        SamplePreviousMoisture(uv + float2(0.0f, -texel.y));
    neighbor_average *= 0.25f;

    float rain_amount = rain_sample.r;
    float rain_wet_hint = rain_sample.g;
    float surface_amount = surface_sample.r;
    float water_coverage = mask_sample.a;
    float standing_water = smoothstep(0.04f, 0.16f, surface_amount);

    float rain_gain = rain_amount * rain_absorption;
    float seep_gain = standing_water * (wetness_absorption * 1.10f) + surface_amount * 0.28f;
    float bank_gain = water_coverage * (shoreline_absorption * 1.25f);
    float evaporation = evaporation_rate * (1.0f - saturate(rain_amount * 0.80f + water_coverage * 0.55f));
    float saturation_loss = max(previous - 0.82f, 0.0f) * (saturation_decay * 0.65f);

    float moisture = previous + rain_gain + seep_gain + bank_gain - evaporation - saturation_loss;
    moisture = lerp(moisture, neighbor_average, diffusion_rate);
    moisture = saturate(moisture + rain_wet_hint * 0.06f);

    float shoreline_band = saturate(water_coverage * 0.85f + standing_water * 0.50f);
    float rainfall_memory = saturate(rain_amount * 0.65f + rain_wet_hint * 0.35f);
    float saturation = saturate(max(surface_amount * 1.10f, moisture));

    g_SoilMoisture[dispatch_thread_id.xy] = float4(moisture, shoreline_band, rainfall_memory, saturation);
}
