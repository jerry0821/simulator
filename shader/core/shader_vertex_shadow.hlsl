cbuffer SHADOW_CONSTANT_BUFFER : register(b0)
{
    float4x4 world;
    float4x4 lightViewProjection;
};

struct VS_IN
{
    float3 posL : POSITION0;
    float3 normalL : NORMAL0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct VS_OUT
{
    float4 posH : SV_POSITION;
};

VS_OUT main(VS_IN input)
{
    VS_OUT output;
    float4 worldPos = mul(float4(input.posL, 1.0f), world);
    output.posH = mul(worldPos, lightViewProjection);
    return output;
}
