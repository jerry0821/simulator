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
    float3 climateData : TEXCOORD2;
    float2 macroData : TEXCOORD3;
};

Texture2D texRock : register(t0);
Texture2D texStone : register(t1);
Texture2D g_ShadowMap : register(t2);
Texture2D texGrass : register(t3);
Texture2D texClimate : register(t4);
Texture2D g_TerrainSurfaceData : register(t5);

SamplerState samp : register(s0);
SamplerState shadowSamp : register(s1);

float4 SampleTerrainSurfaceData(float2 world_xz)
{
    const float field_width = 512.0f;
    const float field_depth = 512.0f;
    float2 uv = float2(
        saturate((world_xz.x + field_width * 0.5f) / field_width),
        saturate((world_xz.y + field_depth * 0.5f) / field_depth));
    return g_TerrainSurfaceData.SampleLevel(samp, uv, 0.0f);
}

float4 main(PS_INPUT ps_in) : SV_TARGET
{
    float3 N = normalize(ps_in.normalW.xyz);
    float4 surface_data = SampleTerrainSurfaceData(ps_in.posW.xz);
    float slopeMask = saturate(surface_data.r);
    float beachMask = saturate(surface_data.g);
    float humidity = saturate(surface_data.b);
    float roughness = saturate(surface_data.a);

    float grassMask =
        saturate((1.0f - slopeMask) * lerp(0.82f, 1.0f, humidity) * lerp(1.0f, 0.78f, beachMask));
    float cliffMask = slopeMask;
    float erosionMask = roughness;

    cliffMask = saturate(cliffMask);
    float surfaceGrass = saturate(grassMask * (1.0f - cliffMask));

    float2 uv_offset = (ps_in.macroData - 0.5f) * 0.18f;
    float2 grass_uv = ps_in.posW.xz * 0.035f + uv_offset;
    float2 stone_uv = ps_in.posW.xz * 0.045f - uv_offset * 0.5f;
    float2 rock_uv = ps_in.posW.xz * 0.028f + uv_offset.yx * 0.35f;

    float3 grassColor = texGrass.Sample(samp, grass_uv).rgb;
    float3 stoneColor = texStone.Sample(samp, stone_uv).rgb;
    float3 rockColor = texRock.Sample(samp, rock_uv).rgb;

    float climateHumidity = saturate(ps_in.climateData.g);
    float temperature = saturate(ps_in.climateData.r);
    float shorelineWetness = saturate(max(beachMask, humidity));

    grassColor *= lerp(float3(0.90f, 0.88f, 0.82f), float3(0.72f, 0.96f, 0.78f), climateHumidity);
    grassColor *= lerp(float3(0.96f, 0.98f, 1.02f), float3(1.04f, 0.98f, 0.92f), temperature * 0.30f);
    grassColor *= lerp(1.0f.xxx, float3(0.84f, 0.88f, 0.80f), roughness * 0.10f);

    stoneColor *= lerp(1.0f.xxx, float3(0.74f, 0.79f, 0.86f), shorelineWetness * 0.38f);
    stoneColor *= lerp(1.0f.xxx, float3(0.90f, 0.86f, 0.78f), roughness * 0.18f);

    rockColor *= lerp(float3(0.92f, 0.92f, 0.92f), float3(1.04f, 1.00f, 0.96f), temperature * 0.18f);
    rockColor *= lerp(1.0f.xxx, float3(0.84f, 0.88f, 0.92f), shorelineWetness * 0.16f);

    float3 material_color = lerp(grassColor, stoneColor, beachMask);
    material_color = lerp(material_color, rockColor, cliffMask);
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

    float3 lightVec = normalize(-directional_world_vector.xyz);
    float dl = (dot(lightVec, N) + 1.0f) * 0.5f;
    float3 diffuse = material_color * directional_color.rgb * dl * shadowFactor;
    float3 ambient = ambient_color.rgb * material_color;
    float3 shaded_color = ambient + diffuse;

    float3 surface_preview =
        float3(
            slopeMask,
            beachMask,
            saturate(humidity * 0.82f + roughness * 0.18f));
    float surface_presentation = saturate(g_UseTerrainSurfacePresentation);
    float3 final_color = lerp(surface_preview, shaded_color, surface_presentation);

    return float4(final_color, 1.0f);
}
