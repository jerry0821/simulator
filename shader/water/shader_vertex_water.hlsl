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

cbuffer VS_CONSTANT_BUFFER3 : register(b3)
{
    float2 camera_xz;
    float water_mesh_interval;
    float padding0;
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
    const float2 global_offset =
        floor(camera_xz / max(water_mesh_interval, 1.0e-4f)) * water_mesh_interval;
    posW.xz += global_offset;

    const float2 terrain_water = LoadTerrainWater(posW.xz);
    // g_TerrainHeight: .x = terrain_height, .y = water_height
    posW.y = terrain_water.y;
    float4 posV = mul(posW, view);
    vo.posH = mul(posV, proj);
    vo.posW = posW.xyz;
    vo.uv = vi.uv;
    return vo;
}
