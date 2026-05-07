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
    float4 posL : POSITION0;
    float4 normalL : NORMAL0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float4 world0 : TEXCOORD1;
    float4 world1 : TEXCOORD2;
    float4 world2 : TEXCOORD3;
    float4 world3 : TEXCOORD4;
    float4 instanceColor : COLOR1;
};

struct VS_OUT
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 MulPointByInstance(float4 point_value, VS_IN vi)
{
    return float4(
        dot(point_value, vi.world0),
        dot(point_value, vi.world1),
        dot(point_value, vi.world2),
        dot(point_value, vi.world3));
}

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;
    float4 posW = MulPointByInstance(float4(vi.posL.xyz, 1.0f), vi);
    float4 posV = mul(posW, view);
    vo.posH = mul(posV, proj);
    vo.uv = vi.uv;
    vo.color = vi.instanceColor;
    return vo;
}
