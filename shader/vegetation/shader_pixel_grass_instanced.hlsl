#include "../common/shader_common_pbr.hlsli"

cbuffer PS_MATERIAL : register(b0)
{
    float4 diffuse_color;
};

cbuffer PS_AMBIENT : register(b1)
{
    float4 ambient_color;
};

cbuffer PS_DIRECTIONAL : register(b2)
{
    float4 directional_world_vector;
    float4 directional_color;
};

cbuffer PS_SPECULAR : register(b3)
{
    float3 eye_posW;
    float specular_power;
    float4 specular_color;
};

struct PS_INPUT
{
    float4 posH : SV_POSITION;
    float4 posW : POSITION0;
    float4 normalW : NORMAL0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

Texture2D tex : register(t0);
SamplerState samp : register(s0);

float3 ComputeBladeNormal(float3 normalW, float3 viewDir)
{
    float3 normal = normalize(normalW);
    // Grass cards should shade as thin two-sided foliage, not as hard opaque planes.
    return dot(normal, viewDir) < 0.0f ? -normal : normal;
}

float4 main(PS_INPUT ps_in) : SV_TARGET
{
    float4 texel = tex.Sample(samp, ps_in.uv);
    float4 color = texel * ps_in.color * diffuse_color;

    float alpha_edge = smoothstep(0.04f, 0.18f, color.a);
    clip(color.a - 0.045f);

    float3 viewDir = normalize(eye_posW - ps_in.posW.xyz);
    float3 lightDir = normalize(-directional_world_vector.xyz);
    float3 normal = ComputeBladeNormal(ps_in.normalW.xyz, viewDir);

    float vertical = saturate(ps_in.uv.y);
    float tipMask = smoothstep(0.45f, 1.0f, vertical);
    float rootMask = 1.0f - smoothstep(0.05f, 0.45f, vertical);
    float3 bladeColor = color.rgb;
    bladeColor *= lerp(0.72f, 1.12f, tipMask);
    bladeColor = lerp(bladeColor * float3(0.68f, 0.58f, 0.36f), bladeColor, 1.0f - rootMask * 0.34f);

    PbrSurface surface;
    surface.baseColor = saturate(bladeColor);
    surface.normal = normal;
    surface.viewDir = viewDir;
    surface.lightDir = lightDir;
    surface.lightColor = directional_color.rgb * 1.12f;
    surface.ambientColor = ambient_color.rgb * 0.82f;
    surface.roughness = 0.78f;
    surface.metallic = 0.0f;
    surface.specular = 0.22f;
    surface.ambientOcclusion = lerp(0.68f, 1.0f, tipMask);
    surface.shadow = 1.0f;

    float3 shaded = DefaultPbrShading(surface);

    float backLight = pow(saturate(dot(-normal, lightDir)), 1.7f);
    float rim = pow(1.0f - saturate(dot(normal, viewDir)), 2.4f);
    float3 transmission = bladeColor * directional_color.rgb * backLight * 0.34f;
    float3 edgeGlow = bladeColor * directional_color.rgb * rim * tipMask * 0.16f;

    float3 finalColor = (shaded + transmission + edgeGlow) * lerp(0.86f, 1.0f, alpha_edge);
    return float4(finalColor, 1.0f);
}
