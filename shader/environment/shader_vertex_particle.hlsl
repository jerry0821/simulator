cbuffer CB_Matrix : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
}

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

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;

    float4 worldPos = mul(vi.posL, world);
    float4 viewPos = mul(worldPos, view);

    vo.posH = mul(viewPos, projection);

    vo.color = vi.color;
    vo.uv = vi.uv;

    return vo;
}