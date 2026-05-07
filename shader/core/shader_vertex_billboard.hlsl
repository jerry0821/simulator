/*==============================================================================

   ビルボード描画用頂点シェーダー [shader_vertex_billboard.hlsl]
														 Author : Jerry
														 Date   : 2025/11/14
--------------------------------------------------------------------------------

==============================================================================*/

// 定数バッファ
cbuffer VS_CONSTANT_BUFFER : register(b0)
{
    float4x4 world;
};

cbuffer VS_CONSTANT_BUFFER : register(b1)
{
    float4x4 view;
};

cbuffer VS_CONSTANT_BUFFER : register(b2)
{
    float4x4 proj;
};

cbuffer VS_CONSTANT_BUFFER : register(b3)
{
    float2 scale;
    float2 translation;
};
 
struct VS_IN
{
    float4 posL : POSITION0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};
 
struct VS_OUT
{
    float4 posH : SV_POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};
 
//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUT main(VS_IN vi)
{
    VS_OUT vo;
    
    // 座標変換
    float4x4 mtxWV = mul(world, view);
    float4x4 mtxWVP = mul(mtxWV, proj);
    vo.posH = mul(vi.posL, mtxWVP);
    
    vo.color = vi.color;
    
    vo.uv = vi.uv * scale + translation;
    
    return vo;
}