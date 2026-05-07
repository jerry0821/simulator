cbuffer VS_CONSTANT_BUFFER : register(b0)
{
    float4x4 view;
};

cbuffer VS_CONSTANT_BUFFER : register(b1)
{
    float4x4 proj;
};

struct VS_IN
{
    float3 posL : POSITION0;
    float3 normalL : NORMAL0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float4 world0 : TEXCOORD1;
    float4 world1 : TEXCOORD2;
    float4 world2 : TEXCOORD3;
    float4 world3 : TEXCOORD4;
};

struct VS_OUT
{
    float4 posH : SV_POSITION;
    float4 posW : POSITION0;
    float4 normalW : NORMAL0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

float4 MulPointByInstance(float4 point_value, VS_IN vi)
{
    return float4(
        dot(point_value, vi.world0),
        dot(point_value, vi.world1),
        dot(point_value, vi.world2),
        dot(point_value, vi.world3));
}

float3 MulVectorByInstance(float3 vector_value, VS_IN vi)
{
    return float3(
        dot(vector_value, vi.world0.xyz),
        dot(vector_value, vi.world1.xyz),
        dot(vector_value, vi.world2.xyz));
}

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;

    const float4 localPos = float4(vi.posL, 1.0f);
    const float4 worldPos = MulPointByInstance(localPos, vi);
    const float3 worldNormal = normalize(MulVectorByInstance(vi.normalL, vi));

    vo.posW = worldPos;
    vo.normalW = float4(worldNormal, 0.0f);
    vo.color = vi.color;
    vo.uv = vi.uv;

    const float4 viewPos = mul(worldPos, view);
    vo.posH = mul(viewPos, proj);

    return vo;
}
