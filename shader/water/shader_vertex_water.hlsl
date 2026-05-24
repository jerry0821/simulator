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

int2 WorldToHeightCoord(float2 world_xz)
{
    uint tex_width = 0;
    uint tex_height = 0;
    water_surface_height_tex.GetDimensions(tex_width, tex_height);
    tex_width = max(tex_width, 1u);
    tex_height = max(tex_height, 1u);

    const float2 tex_size = float2(tex_width, tex_height);
    const float2 data_coord = floor(
        (world_xz + float2(kWorldHalfExtent, kWorldHalfExtent)) *
        (tex_size / kWorldSideLength));
    return clamp(
        int2(data_coord),
        int2(0, 0),
        int2(int(tex_width) - 1, int(tex_height) - 1));
}

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;
    float4 posW = mul(vi.posL, world);
    const int2 coord = WorldToHeightCoord(posW.xz);
    const float4 water_data = water_surface_height_tex.Load(int3(coord, 0));
    // water_data: .x = terrain_height, .y = surface_height, .z = water_depth
    const float terrain_height = water_data.x;
    const float water_depth = max(water_data.z, 0.0f);
    const float surface_height = max(water_data.y, terrain_height);
    posW.y = water_depth > 0.0f ? surface_height : terrain_height;
    float4 posV = mul(posW, view);
    vo.posH = mul(posV, proj);
    vo.posW = posW.xyz;
    vo.uv = vi.uv;
    return vo;
}
