/*==============================================================================
   toon 描画用頂点シェーダー [shader_vertex_toon_main.hlsl]
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
    float4 posW : POSITION;
    float4 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    float4 posW = mul(input.posL, mWorld);
    float4 posV = mul(posW, mView);
    output.posH = mul(posV, mProj);
    
    output.posW = posW;

    output.normal = normalize(mul(float4(input.normalL.xyz, 0.0f), mWorld));
    
    output.uv = input.uv;
    
    output.color = input.color;

    return output;
}