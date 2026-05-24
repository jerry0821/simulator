#include "../common/shader_common_pbr.hlsli"

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
    float g_PbrRoughnessBias;
    float g_PbrSpecularScale;
    float g_PbrDetailNormalStrength;
    float g_PbrLightIntensity;
    float g_PbrMetallic;
    float g_PbrAoStrength;
    float g_PbrDebugMode;
    float g_PbrPadding0;
};

cbuffer PS_PRESENTATION_SETTINGS : register(b5)
{
    float g_UseTerrainSurfacePresentation;
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
    float4 meteorographData : TEXCOORD2;
    float2 macroData : TEXCOORD3;
};

Texture2D texRock : register(t0);
Texture2D texStone : register(t1);
Texture2D g_ShadowMap : register(t2);
Texture2D texGrass : register(t3);
Texture2D g_TerrainVegetationSuitability : register(t4);
Texture2D g_TerrainSurfaceData : register(t5);
Texture2D g_TerrainNormalMap : register(t6);

SamplerState samp : register(s0);
SamplerState shadowSamp : register(s1);

static const float kPolarTemperature = -10.0f;
static const float kEquatorialTemperature = 30.0f;
static const float kWorldSideLength = 2048.0f;
static const float kWorldHalfExtent = kWorldSideLength * 0.5f;

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
    const float3 normal = float3(xz.x, y, xz.y);
    return dot(normal, normal) > 1.0e-4f ? normalize(normal) : float3(0.0f, 1.0f, 0.0f);
}

float3 SampleTerrainNormal(float2 world_xz)
{
    const float2 uv = WorldToFieldUv(world_xz);
    const float2 encodedNormal = g_TerrainNormalMap.SampleLevel(samp, uv, 0.0f).xy;
    return DecodeUpNormal(encodedNormal);
}

float4 SampleTerrainSurfaceData(float2 world_xz)
{
    float2 uv = WorldToFieldUv(world_xz);
    return g_TerrainSurfaceData.SampleLevel(samp, uv, 0.0f);
}

float SampleVegetationSuitability(float2 world_xz)
{
    float2 uv = WorldToFieldUv(world_xz);
    return g_TerrainVegetationSuitability.SampleLevel(samp, uv, 0.0f).r;
}

float NormalizeClimateTemperature(float temperature_celsius)
{
    return saturate((temperature_celsius - kPolarTemperature) / max(kEquatorialTemperature - kPolarTemperature, 1.0e-4f));
}

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

    float v00 = Hash21(cell);
    float v10 = Hash21(cell + float2(1.0f, 0.0f));
    float v01 = Hash21(cell + float2(0.0f, 1.0f));
    float v11 = Hash21(cell + float2(1.0f, 1.0f));

    float vx0 = lerp(v00, v10, smooth.x);
    float vx1 = lerp(v01, v11, smooth.x);
    return lerp(vx0, vx1, smooth.y);
}

float Fbm3(float2 p)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    float2 domain = p;

    [unroll]
    for (int octave = 0; octave < 3; ++octave)
    {
        value += ValueNoise(domain) * amplitude;
        domain = domain * 2.07f + float2(19.1f, 7.7f);
        amplitude *= 0.5f;
    }

    return value;
}

float TerrainDetailHeight(float2 world_xz, float slopeBlend, float beachBlend, float snowMask)
{
    float stoneScale = max(g_StoneNoiseScale * 18.0f, 0.04f);
    float stoneDetail = Fbm3(world_xz * stoneScale);
    float grassDetail = Fbm3(world_xz * 0.42f + float2(11.0f, 3.0f));
    float snowDetail = Fbm3(world_xz * 0.11f + float2(5.0f, 17.0f));

    float detail = lerp(grassDetail * 0.42f, stoneDetail * 1.25f, slopeBlend);
    detail = lerp(detail, stoneDetail * 0.72f, beachBlend);
    detail = lerp(detail, snowDetail * 0.22f, snowMask);
    return detail;
}

float3 ApplyTerrainDetailNormal(float3 normalW, float2 world_xz, float slopeBlend, float beachBlend, float snowMask)
{
    normalW = normalize(normalW);
    float strength = saturate(g_PbrDetailNormalStrength);

    const float sampleStep = 1.35f;
    float heightX =
        TerrainDetailHeight(world_xz + float2(sampleStep, 0.0f), slopeBlend, beachBlend, snowMask) -
        TerrainDetailHeight(world_xz - float2(sampleStep, 0.0f), slopeBlend, beachBlend, snowMask);
    float heightZ =
        TerrainDetailHeight(world_xz + float2(0.0f, sampleStep), slopeBlend, beachBlend, snowMask) -
        TerrainDetailHeight(world_xz - float2(0.0f, sampleStep), slopeBlend, beachBlend, snowMask);

    float3 tangentX = float3(1.0f, 0.0f, 0.0f) - normalW * dot(normalW, float3(1.0f, 0.0f, 0.0f));
    tangentX = dot(tangentX, tangentX) > 1.0e-4f ? normalize(tangentX) : float3(0.0f, 0.0f, 1.0f);
    float3 tangentZ = normalize(cross(normalW, tangentX));
    float materialStrength = lerp(0.65f, 1.25f, slopeBlend);
    materialStrength = lerp(materialStrength, 0.72f, beachBlend);
    materialStrength = lerp(materialStrength, 0.36f, snowMask);
    return normalize(normalW - (tangentX * heightX + tangentZ * heightZ) * strength * materialStrength);
}

float3 ApplySurfaceVariant(float3 srcColor, float beachMask, float humidity, float temperature)
{
    float3 color = srcColor;
    float beachLuminance = dot(color, float3(0.299f, 0.587f, 0.114f));
    float3 beachColor = beachLuminance.xxx * float3(1.10f, 1.02f, 0.76f);
    color = lerp(color, beachColor, beachMask);

    float3 humidColor = color * float3(0.78f, 0.86f, 0.72f);
    color = lerp(color, humidColor, humidity);

    float coldMask = saturate((0.35f - temperature) * 1.6f);
    color = lerp(color, float3(0.74f, 0.74f, 0.78f), coldMask * (0.12f + beachMask * 0.08f));
    return color;
}

float4 main(PS_INPUT ps_in) : SV_TARGET
{
    float3 N = SampleTerrainNormal(ps_in.posW.xz);
    float4 surface_data = SampleTerrainSurfaceData(ps_in.posW.xz);
    float vegetationSuitability = saturate(SampleVegetationSuitability(ps_in.posW.xz));
    float slopeMask = saturate(surface_data.r);
    float beachMask = saturate(surface_data.g);
    float humidity = saturate(surface_data.b);
    float roughness = saturate(surface_data.a);

    float climateHumidity = saturate(ps_in.meteorographData.z);
    float temperatureCelsius = ps_in.meteorographData.w;
    float temperature = NormalizeClimateTemperature(temperatureCelsius);
    float humidityMask = saturate(max(humidity, climateHumidity * 0.55f));
    float slopeBlend = smoothstep(g_RockSlopeStart, g_RockSlopeEnd, slopeMask);
    float beachBlend = saturate(beachMask * (1.0f - slopeBlend * 0.55f));

    float2 terrainTexCoord = ps_in.posW.xz * 0.05f;
    float2 uv_offset = (ps_in.macroData - 0.5f) * 0.16f;
    float2 grass_uv = terrainTexCoord + uv_offset * 0.35f;
    float2 beach_uv = terrainTexCoord * 0.92f - uv_offset * 0.18f;
    float2 slope_uv = terrainTexCoord * 1.08f + uv_offset.yx * 0.22f;

    float3 grassColor = texGrass.Sample(samp, grass_uv).rgb * 1.35f;
    float3 beachColor = texStone.Sample(samp, beach_uv).rgb * 0.92f;
    float3 slopeColor = texStone.Sample(samp, slope_uv).rgb * 0.84f;

    grassColor = ApplySurfaceVariant(grassColor, beachBlend, humidityMask, temperature);
    beachColor = ApplySurfaceVariant(beachColor * float3(0.96f, 0.92f, 0.82f), 1.0f, humidityMask * 0.45f, temperature);
    slopeColor = lerp(
        slopeColor,
        slopeColor * float3(0.74f, 0.78f, 0.82f),
        saturate(humidityMask * 0.35f + roughness * 0.18f));

    const float snowSlopeMask = 1.0f - smoothstep(0.68f, 0.94f, slopeMask);
    const float snowTemperatureMask = saturate(-temperatureCelsius * 0.5f);
    const float snowAltitudeSupport = smoothstep(g_RockHeightStart, g_RockHeightEnd, ps_in.posW.y);
    const float snowMask =
        saturate(max(snowTemperatureMask, snowTemperatureMask * 0.55f + snowAltitudeSupport * 0.45f) * snowSlopeMask);
    const float3 snowColor = lerp(
        float3(0.86f, 0.90f, 0.95f),
        float3(0.98f, 0.99f, 1.0f),
        saturate(roughness * 0.25f + humidityMask * 0.35f));

    float3 material_color = grassColor;
    material_color = lerp(material_color, beachColor, beachBlend);
    material_color = lerp(material_color, slopeColor, slopeBlend);
    const float vegetationFill = smoothstep(0.32f, 0.82f, vegetationSuitability);
    const float terrainGrassMask =
        saturate(max(
            vegetationFill * 0.95f,
            vegetationSuitability * (1.0f - beachBlend) * (1.0f - slopeBlend * 0.68f)));
    material_color = lerp(material_color, grassColor * 1.10f, terrainGrassMask * 0.90f);
    material_color = lerp(material_color, snowColor, snowMask);
    material_color *= diffuse_color.rgb;

    float shadowFactor = 1.0f;
    float3 sPos = ps_in.shadowPos.xyz / ps_in.shadowPos.w;
    float2 shadowUV = float2(0.5f * sPos.x + 0.5f, -0.5f * sPos.y + 0.5f);
    if (shadowUV.x >= 0.0f && shadowUV.x <= 1.0f &&
        shadowUV.y >= 0.0f && shadowUV.y <= 1.0f &&
        sPos.z >= 0.0f && sPos.z <= 1.0f)
    {
        float3 L = normalize(-directional_world_vector.xyz);
        float cosTheta = saturate(dot(N, L));
        float bias = max(0.004f * (1.0f - cosTheta), 0.001f);
        float shadowDepth = g_ShadowMap.Sample(shadowSamp, shadowUV).r;
        shadowFactor = (sPos.z - bias) > shadowDepth ? 0.55f : 1.0f;
    }

    N = ApplyTerrainDetailNormal(N, ps_in.posW.xz, slopeBlend, beachBlend, snowMask);

    float terrainRoughness = saturate(lerp(0.56f, 0.94f, roughness) + g_PbrRoughnessBias);
    terrainRoughness = lerp(terrainRoughness, 0.72f, beachBlend);
    terrainRoughness = lerp(terrainRoughness, 0.82f, slopeBlend);
    terrainRoughness = lerp(terrainRoughness, saturate(0.46f + g_PbrRoughnessBias), snowMask);

    float terrainSpecular = lerp(0.42f, 0.22f, slopeBlend);
    terrainSpecular = lerp(terrainSpecular, 0.50f, beachBlend * saturate(humidityMask + 0.25f));
    terrainSpecular = lerp(terrainSpecular, 0.30f, snowMask);
    terrainSpecular *= max(g_PbrSpecularScale, 0.0f);

    float3 terrainViewDir = normalize(eye_posW - ps_in.posW.xyz);
    float terrainMetallic = saturate(g_PbrMetallic);
    float terrainAO = lerp(
        1.0f,
        lerp(1.0f, 0.84f, saturate(slopeMask * roughness)),
        saturate(g_PbrAoStrength));

    PbrSurface terrainSurface;
    terrainSurface.baseColor = material_color;
    terrainSurface.normal = N;
    terrainSurface.viewDir = terrainViewDir;
    terrainSurface.lightDir = normalize(-directional_world_vector.xyz);
    terrainSurface.lightColor = directional_color.rgb * max(g_PbrLightIntensity, 0.0f);
    terrainSurface.ambientColor = ambient_color.rgb;
    terrainSurface.roughness = terrainRoughness;
    terrainSurface.metallic = terrainMetallic;
    terrainSurface.specular = terrainSpecular;
    terrainSurface.ambientOcclusion = terrainAO;
    terrainSurface.shadow = shadowFactor;
    float3 shaded_color = DefaultPbrShading(terrainSurface);

    int pbrDebugMode = (int)(g_PbrDebugMode + 0.5f);
    if (pbrDebugMode == 1)
    {
        shaded_color = material_color;
    }
    else if (pbrDebugMode == 2)
    {
        shaded_color = terrainMetallic.xxx;
    }
    else if (pbrDebugMode == 3)
    {
        shaded_color = terrainRoughness.xxx;
    }
    else if (pbrDebugMode == 4)
    {
        shaded_color = N * 0.5f + 0.5f;
    }
    else if (pbrDebugMode == 5)
    {
        shaded_color = terrainAO.xxx;
    }
    else if (pbrDebugMode == 6)
    {
        float3 dielectricF0 = lerp(0.02f.xxx, 0.08f.xxx, saturate(terrainSpecular));
        float3 f0 = lerp(dielectricF0, material_color, terrainMetallic);
        shaded_color = saturate(FresnelSchlick(saturate(dot(N, terrainViewDir)), f0) * 8.0f);
    }

    float3 surface_preview =
        float3(
            slopeMask,
            beachMask,
            saturate(humidity * 0.82f + roughness * 0.18f));
    float surface_presentation = saturate(g_UseTerrainSurfacePresentation);
    float3 final_color = lerp(surface_preview, shaded_color, surface_presentation);

    return float4(final_color, 1.0f);
}
