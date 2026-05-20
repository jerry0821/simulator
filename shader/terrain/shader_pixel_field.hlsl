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
    const float field_width = 2048.0f;
    const float field_depth = 2048.0f;
    float2 uv = float2(
        saturate((world_xz.x + field_width * 0.5f) / field_width),
        saturate((world_xz.y + field_depth * 0.5f) / field_depth));
    return g_TerrainSurfaceData.SampleLevel(samp, uv, 0.0f);
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
    float3 N = normalize(ps_in.normalW.xyz);
    float4 surface_data = SampleTerrainSurfaceData(ps_in.posW.xz);
    float slopeMask = saturate(surface_data.r);
    float beachMask = saturate(surface_data.g);
    float humidity = saturate(surface_data.b);
    float roughness = saturate(surface_data.a);

    float climateHumidity = saturate(ps_in.climateData.g);
    float temperature = saturate(ps_in.climateData.r);
    float humidityMask = saturate(max(humidity, climateHumidity * 0.55f));
    float slopeBlend = smoothstep(0.06f, 0.72f, slopeMask);
    float beachBlend = saturate(beachMask * (1.0f - slopeBlend * 0.55f));

    float2 terrainTexCoord = ps_in.posW.xz * 0.05f;
    float2 uv_offset = (ps_in.macroData - 0.5f) * 0.16f;
    float2 grass_uv = terrainTexCoord + uv_offset * 0.35f;
    float2 beach_uv = terrainTexCoord * 0.92f - uv_offset * 0.18f;
    float2 slope_uv = terrainTexCoord * 1.08f + uv_offset.yx * 0.22f;

    float3 grassColor = texGrass.Sample(samp, grass_uv).rgb * 1.35f;
    float3 beachColor = texStone.Sample(samp, beach_uv).rgb * 0.92f;
    float3 slopeColor = texRock.Sample(samp, slope_uv).rgb * 0.82f;

    grassColor = ApplySurfaceVariant(grassColor, beachBlend, humidityMask, temperature);
    beachColor = ApplySurfaceVariant(beachColor * float3(0.96f, 0.92f, 0.82f), 1.0f, humidityMask * 0.45f, temperature);
    slopeColor = lerp(
        slopeColor,
        slopeColor * float3(0.74f, 0.78f, 0.82f),
        saturate(humidityMask * 0.35f + roughness * 0.18f));

    float3 material_color = grassColor;
    material_color = lerp(material_color, beachColor, beachBlend);
    material_color = lerp(material_color, slopeColor, slopeBlend);
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
    float3 viewVec = normalize(eye_posW - ps_in.posW.xyz);
    float3 halfVec = normalize(lightVec + viewVec);
    float NdotL = saturate(dot(lightVec, N));
    float NdotH = saturate(dot(N, halfVec));
    float diffuseWrap = saturate(NdotL * 0.82f + 0.18f);
    float specularExponent = lerp(30.0f, 8.0f, roughness);
    float specularStrength = lerp(0.12f, 0.03f, roughness);
    float shorelineSheen = beachBlend * (0.04f + humidityMask * 0.08f);
    float specularTerm = pow(NdotH, specularExponent) * (specularStrength + shorelineSheen);

    float3 diffuse = material_color * directional_color.rgb * diffuseWrap * shadowFactor;
    float3 ambient = ambient_color.rgb * material_color;
    float3 specular = directional_color.rgb * specular_color.rgb * specularTerm * shadowFactor;
    float3 shaded_color = ambient + diffuse + specular;

    float3 surface_preview =
        float3(
            slopeMask,
            beachMask,
            saturate(humidity * 0.82f + roughness * 0.18f));
    float surface_presentation = saturate(g_UseTerrainSurfacePresentation);
    float3 final_color = lerp(surface_preview, shaded_color, surface_presentation);

    return float4(final_color, 1.0f);
}
