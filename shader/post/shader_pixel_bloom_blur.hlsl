Texture2D SourceTex : register(t0);
SamplerState Sampler : register(s0);

cbuffer BLOOM_CONSTANT_BUFFER : register(b0)
{
    float2 g_TexelSize;
    float2 g_BlurDirection;
    float g_Threshold;
    float g_SoftKnee;
    float g_BlurScale;
    float g_Padding0;
};

struct PS_INPUT
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD;
};

float4 main(PS_INPUT input_pixel) : SV_TARGET
{
    static const float weights[5] = {
        0.227027f,
        0.1945946f,
        0.1216216f,
        0.0540540f,
        0.0162162f
    };

    float2 offset_step = g_TexelSize * g_BlurDirection * max(g_BlurScale, 0.01f);
    float3 color = SourceTex.Sample(Sampler, saturate(input_pixel.uv)).rgb * weights[0];

    [unroll]
    for (int tap = 1; tap < 5; ++tap)
    {
        float2 offset = offset_step * tap;
        color += SourceTex.Sample(Sampler, saturate(input_pixel.uv + offset)).rgb * weights[tap];
        color += SourceTex.Sample(Sampler, saturate(input_pixel.uv - offset)).rgb * weights[tap];
    }

    return float4(color, 1.0f);
}
