/*==============================================================================
   toon outline描画用頂点シェーダー [shader_vertex_toon_outline.hlsl]
==============================================================================*/

cbuffer ConstantBuffer : register(b0)
{
    matrix mWorld;
    float outlineWidth;
    float3 padding; 
}

cbuffer CBView : register(b1)
{
    matrix mView;
}

cbuffer CBProj : register(b2)
{
    matrix mProj;
}


struct VS_INPUT
{
    float4 posL : POSITION;
    float4 normalL : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 posH : SV_POSITION;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    float4 pos = input.posL;
    pos.xyz += input.normalL.xyz * outlineWidth;
    //float3 pos = input.posL.xyz + input.normalL.xyz * outlineWidth;

    pos = mul(pos, mWorld); // Model -> World
    pos = mul(pos, mView); // World -> View
    pos = mul(pos, mProj); // View -> Projection (Clip Space)
    
    output.posH = pos;
    //output.posH = mul(float4(pos, 1.0f), mWVP);
    

    return output;
}