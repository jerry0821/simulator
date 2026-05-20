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

Texture2D g_HeightMap : register(t0);
Texture2D g_TerrainNormalMap : register(t1);
Texture2D g_ClimateMap : register(t2);
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
    float3 climateData : TEXCOORD2;
    float2 macroData : TEXCOORD3;
};

float SampleTerrainHeight(float2 uv)
{
    uint height_width = 0;
    uint height_height = 0;
    g_HeightMap.GetDimensions(height_width, height_height);
    float2 height_resolution = max(float2(height_width, height_height), 1.0f.xx);
    int2 coord = int2(saturate(uv) * (height_resolution - 1.0f) + 0.5f);
    return g_HeightMap.Load(int3(coord, 0)).x;
}

float2 WorldToFieldUv(float2 world_xz)
{
    return float2(
        saturate((world_xz.x + kWorldHalfExtent) / kWorldSideLength),
        saturate((world_xz.y + kWorldHalfExtent) / kWorldSideLength));
}

float3 DecodeUpNormal(float2 encoded)
{
    const float2 xz = clamp(encoded, -1.0f.xx, 1.0f.xx);
    const float y = sqrt(saturate(1.0f - dot(xz, xz)));
    return float3(xz.x, y, xz.y);
}

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;

    float3 tempWorldPos = mul(float4(vi.posL.xyz, 1.0f), world).xyz;
    float2 fieldUV = WorldToFieldUv(tempWorldPos.xz);

    float4 displacedPosL = vi.posL;
    displacedPosL.y = SampleTerrainHeight(fieldUV);
    float4 encodedNormal = g_TerrainNormalMap.SampleLevel(g_Sampler, fieldUV, 0.0f);
    float3 displacedNormalL = DecodeUpNormal(encodedNormal.xy);
    if (dot(displacedNormalL, displacedNormalL) < 1.0e-4f)
    {
        displacedNormalL = float3(0.0f, 1.0f, 0.0f);
    }

    float4x4 mtxWV = mul(world, view);
    float4x4 mtxWVP = mul(mtxWV, proj);
    vo.posH = mul(displacedPosL, mtxWVP);

    vo.normalW = mul(float4(displacedNormalL, 0.0f), world);
    vo.posW = mul(displacedPosL, world);
    vo.blend = vi.blend;
    vo.uv = fieldUV;
    vo.shadowPos = mul(vo.posW, lightViewProj);

    float2 climateUV = WorldToFieldUv(vo.posW.xz);
    uint climateWidth = 0;
    uint climateHeight = 0;
    g_ClimateMap.GetDimensions(climateWidth, climateHeight);
    climateWidth = max(climateWidth, 1u);
    climateHeight = max(climateHeight, 1u);
    int2 climateCoord = int2(climateUV * float2(climateWidth - 1u, climateHeight - 1u) + 0.5f);
    vo.climateData = g_ClimateMap.Load(int3(climateCoord, 0)).rgb;

    vo.macroData.x = Fbm(vo.posW.xz * 0.0038f + float2(31.0f, -17.0f));
    vo.macroData.y = Fbm(vo.posW.xz * 0.0075f + float2(-9.0f, 23.0f));

    return vo;
}
