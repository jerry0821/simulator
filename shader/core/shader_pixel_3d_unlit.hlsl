/*==============================================================================

   3D描画用ピクセルシェーダー(ライトなし) [shader_pixel_3d_unlit.hlsl]
														 Author : Youhei Sato
														 Date   : 2025/11/21
--------------------------------------------------------------------------------

==============================================================================*/

cbuffer PS_CONSTANT_BUFFER : register(b0)
{
    float4 diffuse_color;
};

struct PS_INPUT
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D tex; // テクスチャ
SamplerState samp; // テクスチャさんプラ

float4 main(PS_INPUT ps_in) : SV_TARGET
{
    // return ps_in.color;
    return tex.Sample(samp, ps_in.uv) * diffuse_color;
}
