/*==============================================================================

   3D cutout pixel shader [shader_pixel_3d_cutout.hlsl]

==============================================================================*/

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
    float4 color = tex.Sample(samp, ps_in.uv) * diffuse_color * ps_in.color;
    const float alpha_cutoff = 0.20f;
    const float alpha_width = max(fwidth(color.a) * 1.6f, 1.0e-3f);
    const float coverage =
        smoothstep(alpha_cutoff - alpha_width, alpha_cutoff + alpha_width, color.a);
    clip(coverage - 0.02f);
    color.rgb *= coverage;
    color.a = 1.0f;
    return color;
}
