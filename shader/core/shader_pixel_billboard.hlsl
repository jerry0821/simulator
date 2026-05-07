/*==============================================================================

   Billboard pixel shader [shader_pixel_billboard.hlsl]
--------------------------------------------------------------------------------

==============================================================================*/

cbuffer PS_CONSTANT_BUFFER : register(b0)
{
    float4 diffuse_color;
};

struct PS_INPUT
{
    float4 posH : SV_POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

Texture2D tex;
SamplerState samp;

float4 main(PS_INPUT ps_in) : SV_TARGET
{
    float4 color = tex.Sample(samp, ps_in.uv) * ps_in.color * diffuse_color;
    clip(color.a - 0.04f);
    color.rgb *= saturate((color.a - 0.04f) * 1.5f + 0.85f);
    color.a = 1.0f;
    return color;
}
