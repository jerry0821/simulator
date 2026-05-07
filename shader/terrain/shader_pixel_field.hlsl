cbuffer PS_CONSTANT_BUFFER0 : register(b0)
{
    float4 diffuse_color;
};

cbuffer PS_CONSTANT_BUFFER1 : register(b1)
{
    float4 ambient_color;
};

cbuffer PS_CONSTANT_BUFFER2 : register(b2)
{
    float4 directional_world_vector;
    float4 directional_color;
};

cbuffer PS_CONSTANT_BUFFER3 : register(b3)
{
    float3 eye_posW;
    float specular_power;
    float4 specular_color;
};

cbuffer PS_TERRAIN_MATERIAL : register(b4)
{
    float g_GrassSlopeMin;
    float g_GrassSlopeMax;
    float g_GrassNoiseStrength;
    float g_GrassHeightStart;
    float g_GrassHeightEnd;
    float g_RockSlopeStart;
    float g_RockSlopeEnd;
    float g_RockHeightStart;
    float g_RockHeightEnd;
    float g_StoneNoiseScale;
    float g_ShorelineOffsetStart;
    float g_ShorelineOffsetEnd;
    float g_LowlandHeightStart;
    float g_LowlandHeightEnd;
    float g_GrassCoverageMin;
    float g_WaterHeight;
};

cbuffer PS_PRESENTATION_SETTINGS : register(b5)
{
    float g_UseTerrainClassification;
    float3 g_PresentationPadding;
};

struct PS_INPUT
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

Texture2D texRock : register(t0);
Texture2D texStone : register(t1);
Texture2D g_ShadowMap : register(t2);
Texture2D texGrass : register(t3);
Texture2D texClimate : register(t4);
Texture2D g_TerrainClassification : register(t5);

SamplerState samp : register(s0);
SamplerState shadowSamp : register(s1);

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

float4 CalcTriplanar(Texture2D tex, SamplerState tex_sampler, float3 posW, float3 normalW, float scale)
{
    float3 blendWeights = abs(normalW);
    blendWeights = pow(blendWeights, 4.0f);
    blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z);

    float2 uvX = posW.zy * scale;
    float2 uvY = posW.xz * scale;
    float2 uvZ = posW.xy * scale;

    float4 colX = tex.Sample(tex_sampler, uvX);
    float4 colY = tex.Sample(tex_sampler, uvY);
    float4 colZ = tex.Sample(tex_sampler, uvZ);

    return colX * blendWeights.x + colY * blendWeights.y + colZ * blendWeights.z;
}

float4 SampleTerrainClassification(float2 world_xz)
{
    const float field_width = 512.0f;
    const float field_depth = 512.0f;
    float2 uv = float2(
        saturate((world_xz.x + field_width * 0.5f) / field_width),
        saturate((world_xz.y + field_depth * 0.5f) / field_depth));
    return g_TerrainClassification.SampleLevel(samp, uv, 0.0f);
}

float4 main(PS_INPUT ps_in) : SV_TARGET
{
    float3 N = normalize(ps_in.normalW.xyz);

    float4 colorCliff = CalcTriplanar(texRock, samp, ps_in.posW.xyz, N, 0.85f);
    float4 colorStone = CalcTriplanar(texStone, samp, ps_in.posW.xyz, N, 0.22f);
    float4 colorGrass = CalcTriplanar(texGrass, samp, ps_in.posW.xyz, N, 0.18f);

    float slope = saturate(N.y);
    float height = ps_in.posW.y;
    float noise = texStone.Sample(samp, ps_in.posW.xz * g_StoneNoiseScale).r;
    float macroNoise = ps_in.macroData.x;
    float macroWarp = ps_in.macroData.y;
    float macroZone = saturate(macroNoise + (macroWarp - 0.5f) * 0.18f);

    float temperature = ps_in.climateData.r;
    float humidity = ps_in.climateData.g;
    float rainfall = ps_in.climateData.b;

    float4 classification = SampleTerrainClassification(ps_in.posW.xz);
    float grassCoverage = classification.r;
    float wetness = classification.g;
    float cliffMask = classification.b;
    float erosionMask = classification.a;
    float grassMask = smoothstep(g_GrassCoverageMin, 1.0f, grassCoverage);

    if (g_UseTerrainClassification < 0.5f)
    {
        float legacyGrassSlope = smoothstep(g_GrassSlopeMin, g_GrassSlopeMax, slope);
        float legacyGrassHeight = 1.0f - smoothstep(g_GrassHeightStart, g_GrassHeightEnd, height);
        float legacyNoise = saturate(0.65f + (macroZone - 0.5f) * 0.55f + (noise - 0.5f) * 0.18f);
        grassMask = saturate(legacyGrassSlope * legacyGrassHeight * legacyNoise);

        float legacyRockSlope = 1.0f - smoothstep(g_RockSlopeStart, g_RockSlopeEnd, slope);
        float legacyRockHeight = smoothstep(g_RockHeightStart, g_RockHeightEnd, height);
        cliffMask = saturate(legacyRockSlope * 1.10f + legacyRockHeight * 0.38f);

        wetness = saturate(1.0f - smoothstep(
            g_WaterHeight + g_ShorelineOffsetStart * 0.35f,
            g_WaterHeight + g_ShorelineOffsetEnd * 0.90f,
            height));
        erosionMask = 0.0f;
    }

    float stoneVariation = saturate(0.86f + (noise - 0.5f) * 0.28f + (macroZone - 0.5f) * 0.12f + (macroWarp - 0.5f) * 0.10f);
    float shorelineWetness = saturate(wetness);
    float4 groundColor = colorStone * stoneVariation;
    groundColor.rgb *= lerp(1.0f.xxx, float3(0.72f, 0.76f, 0.82f), shorelineWetness * (0.30f + rainfall * 0.20f));
    groundColor.rgb *= lerp(1.0f.xxx, float3(0.88f, 0.84f, 0.78f), erosionMask * 0.22f);

    float grassVariation = saturate(
        0.78f +
        (macroZone - 0.5f) * (0.24f + g_GrassNoiseStrength * 0.45f) +
        (humidity - 0.5f) * 0.26f -
        shorelineWetness * 0.08f);
    float4 grassColor = colorGrass;
    grassColor.rgb *= grassVariation;
    grassColor.rgb *= lerp(float3(0.88f, 0.88f, 0.82f), float3(0.72f, 0.96f, 0.78f), humidity);
    grassColor.rgb *= lerp(float3(0.94f, 0.96f, 1.02f), float3(1.05f, 0.98f, 0.90f), temperature * 0.35f);
    grassColor.rgb *= lerp(1.0f.xxx, float3(0.86f, 0.90f, 0.82f), erosionMask * 0.18f);

    float terrainSurfaceMask = grassMask * (1.0f - cliffMask);
    float4 tex_color = lerp(groundColor, grassColor, terrainSurfaceMask);
    tex_color = lerp(tex_color, colorCliff, cliffMask);

    float3 climateTint =
        lerp(float3(0.97f, 0.96f, 0.94f), float3(0.96f, 1.01f, 0.98f), humidity * 0.55f);
    climateTint *= lerp(float3(0.98f, 0.99f, 1.01f), float3(1.02f, 0.99f, 0.96f), temperature * 0.24f);
    tex_color.rgb *= climateTint;

    float3 material_color = tex_color.rgb * diffuse_color.rgb;

    float shadowFactor = 1.0f;
    float3 sPos = ps_in.shadowPos.xyz / ps_in.shadowPos.w;

    float2 shadowUV;
    shadowUV.x = 0.5f * sPos.x + 0.5f;
    shadowUV.y = -0.5f * sPos.y + 0.5f;
    if (shadowUV.x >= 0.0f && shadowUV.x <= 1.0f &&
        shadowUV.y >= 0.0f && shadowUV.y <= 1.0f &&
        sPos.z >= 0.0f && sPos.z <= 1.0f)
    {
        float currentDepth = sPos.z;
        float3 L = normalize(-directional_world_vector.xyz);
        float cosTheta = saturate(dot(N, L));
        float bias = max(0.005f * (1.0f - cosTheta), 0.001f);

        shadowFactor = 0.0f;
        uint shadowWidth = 0;
        uint shadowHeight = 0;
        g_ShadowMap.GetDimensions(shadowWidth, shadowHeight);
        float2 texelSize = 1.0f / max(float2((float)shadowWidth, (float)shadowHeight), 1.0f.xx);

        const float2 offsets[4] = {
            float2(-0.5f, -0.5f),
            float2( 0.5f, -0.5f),
            float2(-0.5f,  0.5f),
            float2( 0.5f,  0.5f)
        };

        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            float2 offset = offsets[i] * texelSize;
            float pcfDepth = g_ShadowMap.Sample(shadowSamp, shadowUV + offset).r;
            shadowFactor += (currentDepth - bias) > pcfDepth ? 0.0f : 1.0f;
        }

        shadowFactor /= 4.0f;
        shadowFactor = lerp(0.4f, 1.0f, shadowFactor);
    }

    float3 lightVec = normalize(-directional_world_vector.xyz);
    float dl = (dot(lightVec, N) + 1.0f) * 0.5f;
    float3 diffuse = material_color * directional_color.rgb * dl * shadowFactor;
    float3 ambient = ambient_color.rgb * material_color;
    float3 final_color = ambient + diffuse;

    return float4(final_color, 1.0f);
}
