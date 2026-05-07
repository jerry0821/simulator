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
    float water_surface_height = water_surface_height_tex.SampleLevel(samp, saturate(vi.uv), 0.0f).r;
    float4 surface_water = surface_water_tex.SampleLevel(samp, saturate(vi.uv), 0.0f);
    float water_depth = surface_water.r;
    float standing_water = surface_water.b;
    float pooled_weight = smoothstep(0.10f, 0.58f, standing_water);
    float depth_weight = smoothstep(0.03f, 0.16f, water_depth);
    float displacement_weight = saturate(pooled_weight * 0.82f + depth_weight * 0.34f);
    float4 displaced_posL = vi.posL;
    float4 posW = mul(displaced_posL, world);
    float displacement_delta = clamp(water_surface_height - posW.y, -0.06f, 0.82f);
    posW.y += displacement_delta * displacement_weight;
    float4 posV = mul(posW, view);
    vo.posH = mul(posV, proj);
    vo.posW = posW.xyz;
    vo.uv = vi.uv;
    return vo;
}
