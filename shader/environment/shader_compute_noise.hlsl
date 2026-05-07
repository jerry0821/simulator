cbuffer NoiseConstants : register(b0)
{
    float g_TimeSeconds;
    float g_NoiseScale;
    float g_FlowSpeedX0;
    float g_FlowSpeedY0;
    float g_FlowSpeedX1;
    float g_FlowSpeedY1;
    float g_BandStrength;
    float g_Contrast;
    uint g_TextureWidth;
    uint g_TextureHeight;
};

RWTexture2D<float4> g_OutputTexture : register(u0);

float Hash21(float2 p)
{
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 78.233);
    return frac(p.x * p.y);
}

float ValueNoise(float2 uv)
{
    float2 cell = floor(uv);
    float2 local = frac(uv);
    float2 smooth = local * local * (3.0f - 2.0f * local);

    float n00 = Hash21(cell + float2(0.0f, 0.0f));
    float n10 = Hash21(cell + float2(1.0f, 0.0f));
    float n01 = Hash21(cell + float2(0.0f, 1.0f));
    float n11 = Hash21(cell + float2(1.0f, 1.0f));

    float nx0 = lerp(n00, n10, smooth.x);
    float nx1 = lerp(n01, n11, smooth.x);
    return lerp(nx0, nx1, smooth.y);
}

float FBM(float2 uv)
{
    float value = 0.0f;
    float amplitude = 0.55f;
    float frequency = 1.0f;

    [unroll]
    for (int octave = 0; octave < 4; ++octave)
    {
        value += ValueNoise(uv * frequency) * amplitude;
        frequency *= 2.03f;
        amplitude *= 0.5f;
    }

    return value;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= g_TextureWidth || dispatch_thread_id.y >= g_TextureHeight)
    {
        return;
    }

    float2 uv = (dispatch_thread_id.xy + 0.5f) / float2(g_TextureWidth, g_TextureHeight);
    float2 flow0 = uv * (g_NoiseScale * 0.18f) + float2(g_TimeSeconds * g_FlowSpeedX0, g_TimeSeconds * g_FlowSpeedY0);
    float2 flow1 = uv * (g_NoiseScale * 0.32f) + float2(g_TimeSeconds * g_FlowSpeedX1, g_TimeSeconds * g_FlowSpeedY1);

    float base_noise = FBM(flow0);
    float detail_noise = FBM(flow1 + 12.7f);
    float bands = 0.5f + 0.5f * sin((uv.x * 1.2f + uv.y * 0.9f + g_TimeSeconds * 0.25f) * 14.0f);

    float noise = saturate(base_noise * 0.7f + detail_noise * 0.2f + bands * g_BandStrength);
    noise = saturate(pow(noise, max(0.05f, g_Contrast)));
    float ripple = saturate(detail_noise * 0.85f + bands * 0.15f);
    float3 color = float3(noise, ripple, 1.0f - noise * 0.45f);

    g_OutputTexture[dispatch_thread_id.xy] = float4(color, 1.0f);
}
