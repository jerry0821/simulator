/*==============================================================================

   toon outline描画用ピクセルシェーダー[ [shader_pixel_toon_outline.hlsl]
                                                         Author : Jerry
                                                         Date   : 2025/12/12
--------------------------------------------------------------------------------

==============================================================================*/
cbuffer CB_PS : register(b0)
{
    float4 materialColor;
    float4 outlineColor;
    int stepCount;
    float3 padding_ps;
};

struct PS_INPUT
{
    float4 posH : SV_POSITION;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    return outlineColor;
}