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

Texture2D g_TerrainHeight : register(t0);

#include "shader_water_common.hlsli"

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
    float4 posW = mul(vi.posL, world);
    const float base_water_height = posW.y;
    const float2 terrain_water = LoadTerrainWater(posW.xz);
    // g_TerrainHeight: .x = terrain_height, .y = water_height
    const float water_depth = terrain_water.y - terrain_water.x;
    const float simulated_height_weight = smoothstep(0.35f, 1.20f, water_depth);
    posW.y = lerp(base_water_height, terrain_water.y, simulated_height_weight);
    float4 posV = mul(posW, view);
    vo.posH = mul(posV, proj);
    vo.posW = posW.xyz;
    vo.uv = vi.uv;
    return vo;
}
