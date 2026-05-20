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

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;
    float4 posW = mul(vi.posL, world);
    // AfterglowRender-style: absolute world-pos clamp sampling
    static const float kWorldSideLength = 2048.0f;
    static const float kWorldCenterOffset = -1024.0f;
    float2 worldUV = clamp((posW.xz - kWorldCenterOffset) / kWorldSideLength, 0.0f, 1.0f);
    const float2 sample_uv = ComputeWaterSampleUv(worldUV);
    const float2 terrain_water_height = water_surface_height_tex.SampleLevel(samp, sample_uv, 0.0f).xy;
    const float water_depth = max(terrain_water_height.y - terrain_water_height.x, 0.0f);
    const float surface_lift =
        water_depth > 0.0f
            ? (0.008f + min(water_depth * 0.030f, 0.014f))
            : 0.0f;
    posW.y = terrain_water_height.y + surface_lift;
    float4 posV = mul(posW, view);
    vo.posH = mul(posV, proj);
    vo.posW = posW.xyz;
    vo.uv = worldUV;
    return vo;
}
