/*==============================================================================

   3D描画用頂点シェーダー [shader_vertex_3d.hlsl]
														 Author : Youhei Sato
														 Date   : 2025/05/15
--------------------------------------------------------------------------------

==============================================================================*/

// 定?バッファ
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



struct VS_IN
{
    float3 posL : POSITION0;
    float3 normalL : NORMAL0; //Local法線
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};
 
struct VS_OUT
{
    float4 posH : SV_POSITION;
    float4 posW : POSITION0;
    float4 normalW : NORMAL0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};
 
//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUT main(VS_IN vi)
{
    VS_OUT vo;
    
    float4 localPos = float4(vi.posL, 1.0f);
    float4 localNormal = float4(vi.normalL, 0.0f);
    // --- 座標変換 ---
    // World * View * Projection を一度に計算
    float4x4 mtxWVP = mul(mul(world, view), proj);
    vo.posH = mul(localPos, mtxWVP); // 頂点をクリップ空間へ
    
    // --- ピクセルシェーダへ渡すデータ ---
    
    // 1. ワールド座標
    vo.posW = mul(localPos, world);
    
    float4 normalW = mul(localNormal, world);
    //float4 normalW = mul(float4(vi.normalL.xyz, 0.0f), world);
    vo.normalW = normalize(normalW); // 法線を正規化（単位ベクトル化）する
    
    // 3. 他のデータをパススルー
    vo.color = vi.color;
    vo.uv = vi.uv;
    
    return vo;
}