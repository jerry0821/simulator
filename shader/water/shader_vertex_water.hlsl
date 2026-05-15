cbuffer VS_CONSTANT_BUFFER0 : register(b0)
{
    float4x4 world;
};

cbuffer VS_CONSTANT_BUFFER1 : register(b1)
{
    float4x4 view;
};

cbuffer VS_CONSTANT_BUFFER2 : register(b2)
{
    float4x4 proj;
};

Texture2D water_surface_height_tex : register(t0);
Texture2D surface_water_tex : register(t1);
SamplerState samp : register(s0);

struct VS_IN
{
    float4 posL : POSITION0;
    float4 normalL : NORMAL0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct VS_OUT
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;
    float2 terrain_water_height = water_surface_height_tex.SampleLevel(samp, saturate(vi.uv), 0.0f).rg;
    float4 posW = mul(vi.posL, world);
    posW.y = terrain_water_height.y;
    float4 posV = mul(posW, view);
    vo.posH = mul(posV, proj);
    vo.posW = posW.xyz;
    vo.uv = vi.uv;
    return vo;
}
