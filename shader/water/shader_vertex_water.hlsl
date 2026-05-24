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

Texture2D<float4> water_surface_height_tex : register(t0);
SamplerState samp : register(s0);

static const float kWorldSideLength = 2048.0f;
static const float kWorldHalfExtent = kWorldSideLength * 0.5f;

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

float2 ComputeWaterSampleUv(float2 uv)
{
    uint tex_width = 0;
    uint tex_height = 0;
    water_surface_height_tex.GetDimensions(tex_width, tex_height);
    const float2 texel_size = 1.0f / max(float2(tex_width, tex_height), 1.0f.xx);
    const float2 half_texel = texel_size * 0.5f;
    return clamp(uv, half_texel, 1.0f.xx - half_texel);
}

float2 WorldToFieldUv(float2 world_xz)
{
    return float2(
        saturate((world_xz.x + kWorldHalfExtent) / kWorldSideLength),
        saturate((world_xz.y + kWorldHalfExtent) / kWorldSideLength));
}

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;
    float4 posW = mul(vi.posL, world);
    float2 worldUV = WorldToFieldUv(posW.xz);
    const float2 sample_uv = ComputeWaterSampleUv(worldUV);
    const float water_surface_height = water_surface_height_tex.SampleLevel(samp, sample_uv, 0.0f).y;
    posW.y = water_surface_height;
    float4 posV = mul(posW, view);
    vo.posH = mul(posV, proj);
    vo.posW = posW.xyz;
    vo.uv = vi.uv;
    return vo;
}
