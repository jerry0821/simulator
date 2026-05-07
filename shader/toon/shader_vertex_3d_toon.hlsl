/*==============================================================================

   トゥーンレンダリング描画用頂点シェーダー [shader_vertex_3d_toon.hlsl]
														 Author : Jerry
														 Date   : 2025/12/12
--------------------------------------------------------------------------------

==============================================================================*/
cbuffer VS_CONSTANT_BUFFER : register(b0)
{
    float4x4 view;
    float4x4 proj;
};

cbuffer VS_CONSTANT_BUFFER : register(b1)
{
    float4x4 world;
};

struct VS_IN
{
    float4 posL : POSITION0;
    float4 normalL : NORMAL0; //Local法線
};
struct VS_OUT
{
    float4 posH : SV_POSITION;
    float4 normalW : NORMAL0;
    float4 posW : POSITION0;
};
//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUT main(VS_IN vi)
{
    VS_OUT vo;
    
    float4 worldPos = mul(vi.posL, world);
    vo.posW = worldPos;
    
    float4x4 mtxWVP = mul(mul(world, view), proj);
    vo.posH = mul(vi.posL, mtxWVP);
    
    vo.normalW = normalize(mul(vi.normalL, world));
    
    return vo; 
}