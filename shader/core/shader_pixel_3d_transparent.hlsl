cbuffer PS_CONSTANT_BUFFER : register(b0)
{
    float4 diffuse_color;
};

struct PS_INPUT
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

Texture2D tex;
SamplerState samp;

float4 main(PS_INPUT ps_in) : SV_TARGET
{
    float4 sampled = tex.Sample(samp, ps_in.uv) * diffuse_color * ps_in.color;

    float2 centered_uv = ps_in.uv * 2.0f - 1.0f;
    float circle_alpha = 1.0f - length(centered_uv);
    circle_alpha = smoothstep(0.0f, 1.0f, circle_alpha);

    sampled.rgb *= sampled.a * circle_alpha;
    sampled.a *= circle_alpha;
    clip(sampled.a - 0.01f);
    return sampled;
}
