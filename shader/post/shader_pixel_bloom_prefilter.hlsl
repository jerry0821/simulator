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
    float3 color = SourceTex.Sample(Sampler, saturate(input_pixel.uv)).rgb;
    float brightness = max(color.r, max(color.g, color.b));
    float soft_knee = max(g_SoftKnee, 1.0e-4f);
    float knee_low = g_Threshold - soft_knee;
    float softness = saturate((brightness - knee_low) / (soft_knee * 2.0f));
    float contribution = max(brightness - g_Threshold, 0.0f) + softness * softness * soft_knee;
    contribution /= max(brightness, 1.0e-4f);
    return float4(color * contribution, 1.0f);
}
