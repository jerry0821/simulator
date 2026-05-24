#ifndef SHADER_COMMON_PBR_HLSLI
#define SHADER_COMMON_PBR_HLSLI

static const float kPbrPi = 3.14159265f;
static const float kPbrEpsilon = 1.0e-4f;

struct PbrSurface
{
    float3 baseColor;
    float3 normal;
    float3 viewDir;
    float3 lightDir;
    float3 lightColor;
    float3 ambientColor;
    float roughness;
    float metallic;
    float specular;
    float ambientOcclusion;
    float shadow;
};

float Pow5(float value)
{
    float value2 = value * value;
    return value2 * value2 * value;
}

float3 FresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0f - f0) * Pow5(1.0f - saturate(cosTheta));
}

float DistributionGGX(float nDotH, float roughness)
{
    float a = max(roughness * roughness, 0.045f);
    float a2 = a * a;
    float denom = nDotH * nDotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(kPbrPi * denom * denom, kPbrEpsilon);
}

float GeometrySchlickGGX(float nDotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) * 0.125f;
    return nDotV / max(nDotV * (1.0f - k) + k, kPbrEpsilon);
}

float GeometrySmith(float nDotV, float nDotL, float roughness)
{
    return GeometrySchlickGGX(nDotV, roughness) *
        GeometrySchlickGGX(nDotL, roughness);
}

float3 DiffuseBurley(float3 baseColor, float roughness, float nDotV, float nDotL, float lDotH)
{
    float fd90 = 0.5f + 2.0f * roughness * lDotH * lDotH;
    float lightScatter = 1.0f + (fd90 - 1.0f) * Pow5(1.0f - nDotL);
    float viewScatter = 1.0f + (fd90 - 1.0f) * Pow5(1.0f - nDotV);
    return baseColor * (lightScatter * viewScatter) / kPbrPi;
}

float3 DefaultPbrShading(PbrSurface surface)
{
    float3 n = normalize(surface.normal);
    float3 v = normalize(surface.viewDir);
    float3 l = normalize(surface.lightDir);
    float3 h = normalize(v + l);

    float roughness = clamp(surface.roughness, 0.04f, 1.0f);
    float metallic = saturate(surface.metallic);
    float specular = saturate(surface.specular);
    float ao = saturate(surface.ambientOcclusion);
    float shadow = saturate(surface.shadow);

    float nDotV = max(dot(n, v), kPbrEpsilon);
    float nDotL = saturate(dot(n, l));
    float nDotH = saturate(dot(n, h));
    float lDotH = saturate(dot(l, h));

    float3 dielectricF0 = lerp(0.02f.xxx, 0.08f.xxx, specular);
    float3 f0 = lerp(dielectricF0, surface.baseColor, metallic);
    float3 fresnel = FresnelSchlick(lDotH, f0);
    float distribution = DistributionGGX(nDotH, roughness);
    float geometry = GeometrySmith(nDotV, nDotL, roughness);
    float3 specularBrdf = distribution * geometry * fresnel /
        max(4.0f * nDotV * nDotL, kPbrEpsilon);

    float3 diffuseEnergy = (1.0f - fresnel) * (1.0f - metallic);
    float3 diffuseBrdf = DiffuseBurley(surface.baseColor, roughness, nDotV, nDotL, lDotH);

    // Project lightColor as game-light intensity, not physical radiance.
    float3 direct = (diffuseEnergy * diffuseBrdf * kPbrPi + specularBrdf) *
        surface.lightColor * nDotL * shadow;
    float3 ambient = surface.ambientColor * surface.baseColor * ao;
    return ambient + direct;
}

#endif // SHADER_COMMON_PBR_HLSLI
