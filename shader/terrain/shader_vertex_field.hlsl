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

cbuffer CB_Light : register(b3)
{
    float4x4 lightViewProj;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D<float4> g_MeteorographMap : register(t2);
SamplerState g_Sampler : register(s0);

static const float kWorldSideLength = 2048.0f;
static const float kWorldHalfExtent = kWorldSideLength * 0.5f;

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 345.45f));
    p += dot(p, p + 34.345f);
    return frac(p.x * p.y);
}

float ValueNoise(float2 p)
{
    float2 cell = floor(p);
    float2 local = frac(p);
    float2 smooth = local * local * (3.0f - 2.0f * local);

    float v00 = Hash21(cell + float2(0.0f, 0.0f));
    float v10 = Hash21(cell + float2(1.0f, 0.0f));
    float v01 = Hash21(cell + float2(0.0f, 1.0f));
    float v11 = Hash21(cell + float2(1.0f, 1.0f));

    float vx0 = lerp(v00, v10, smooth.x);
    float vx1 = lerp(v01, v11, smooth.x);
    return lerp(vx0, vx1, smooth.y);
}

float Fbm(float2 p)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    float2 domain = p;

    [unroll]
    for (int octave = 0; octave < 4; ++octave)
    {
        value += ValueNoise(domain) * amplitude;
        domain = domain * 2.03f + float2(17.0f, 9.0f);
        amplitude *= 0.5f;
    }

    return value;
}

struct VS_IN
{
    float4 posL : POSITION0;
    float4 normalL : NORMAL0;
    float4 blend : COLOR0;
    float2 uv : TEXCOORD0;
};

struct VS_OUT
{
    float4 posH : SV_POSITION;
    float4 posW : POSITION0;
    float4 normalW : NORMAL0;
    float4 blend : COLOR0;
    float2 uv : TEXCOORD0;
    float4 shadowPos : TEXCOORD1;
    float4 meteorographData : TEXCOORD2;
    float2 macroData : TEXCOORD3;
};

int2 WorldToHeightCoord(float2 world_xz)
{
    uint height_width = 0;
    uint height_height = 0;
    g_TerrainHeight.GetDimensions(height_width, height_height);
    height_width = max(height_width, 1u);
    height_height = max(height_height, 1u);

    const float2 tex_size = float2(height_width, height_height);
    const float2 data_coord = floor(
        (world_xz + float2(kWorldHalfExtent, kWorldHalfExtent)) *
        (tex_size / kWorldSideLength));
    return clamp(
        int2(data_coord),
        int2(0, 0),
        int2(int(height_width) - 1, int(height_height) - 1));
}

float SampleTerrainHeightWorld(float2 world_xz)
{
    return g_TerrainHeight.Load(int3(WorldToHeightCoord(world_xz), 0)).x;
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

    float4 posW = mul(float4(vi.posL.xyz, 1.0f), world);
    float2 fieldUV = WorldToFieldUv(posW.xz);
    posW.y = SampleTerrainHeightWorld(posW.xz);

    float4 posV = mul(posW, view);
    vo.posH = mul(posV, proj);

    vo.normalW = mul(float4(vi.normalL.xyz, 0.0f), world);
    vo.posW = posW;
    vo.blend = vi.blend;
    vo.uv = fieldUV;
    vo.shadowPos = mul(vo.posW, lightViewProj);

    float2 climateUV = WorldToFieldUv(vo.posW.xz);
    uint meteorographWidth = 0;
    uint meteorographHeight = 0;
    g_MeteorographMap.GetDimensions(meteorographWidth, meteorographHeight);
    meteorographWidth = max(meteorographWidth, 1u);
    meteorographHeight = max(meteorographHeight, 1u);
    int2 meteorographCoord = int2(
        climateUV * float2(meteorographWidth - 1u, meteorographHeight - 1u) + 0.5f);
    vo.meteorographData = g_MeteorographMap.Load(int3(meteorographCoord, 0));

    vo.macroData.x = Fbm(vo.posW.xz * 0.0038f + float2(31.0f, -17.0f));
    vo.macroData.y = Fbm(vo.posW.xz * 0.0075f + float2(-9.0f, 23.0f));

    return vo;
}
