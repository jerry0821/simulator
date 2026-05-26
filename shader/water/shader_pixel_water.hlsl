#include "../common/shader_common_pbr.hlsli"

cbuffer PS_CONSTANT_BUFFER0 : register(b0)
{
    float4 diffuse_color;
};

cbuffer PS_CONSTANT_BUFFER1 : register(b6)
{
    float3 camera_position;
    float highlight_strength;
};

cbuffer PS_CONSTANT_BUFFER2 : register(b7)
{
    float4x4 inverse_view_projection;
};

struct PS_INPUT
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD0;
    float2 uv : TEXCOORD1;
    bool isFrontFace : SV_IsFrontFace;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D<float4> water_velocity_tex : register(t1);
Texture2D<float4> water_sediment_tex : register(t2);
Texture2D<float4> terrain_normal_tex : register(t3);
Texture2D scene_depth_tex : register(t4);
SamplerState samp : register(s0);

#include "shader_water_common.hlsli"

float2 ComputeScreenUv(float4 posH)
{
    uint width = 0;
    uint height = 0;
    scene_depth_tex.GetDimensions(width, height);
    const float2 inv_size = 1.0f / max(float2(width, height), 1.0f.xx);
    return saturate(posH.xy * inv_size);
}

float3 ComputeSceneWorldPos(float2 uv, float raw_depth)
{
    const float2 ndc_xy = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    const float4 clip_pos = float4(ndc_xy, raw_depth, 1.0f);
    float4 world_pos = mul(clip_pos, inverse_view_projection);
    world_pos.xyz /= max(world_pos.w, 1.0e-6f);
    return world_pos.xyz;
}

float4 main(PS_INPUT ps_in) : SV_TARGET
{
    const float2 sample_uv = ClampFieldUv(WorldToFieldUv(ps_in.posW.xz));
    const float2 terrain_water_height = SampleTerrainWater(samp, ps_in.posW.xz);
    const float water_depth = terrain_water_height.y - terrain_water_height.x;

    const float4 velocity_sample = water_velocity_tex.SampleLevel(samp, sample_uv, 0.0f);
    const float4 sediment_sample = water_sediment_tex.SampleLevel(samp, sample_uv, 0.0f);
    const float water_speed_factor = saturate(length(velocity_sample.xy) * 8.0f + velocity_sample.z * 0.65f);
    const float suspended_sediment = saturate(sediment_sample.x);

    const float4 packed_normal_sample = SampleTerrain4(terrain_normal_tex, samp, ps_in.posW.xz);
    float3 surface_normal = ReconstructUpNormal(packed_normal_sample.zw);
    const float shallow_normal_fade = smoothstep(0.08f, 0.35f, max(water_depth, 0.0f));
    surface_normal = normalize(lerp(float3(0.0f, 1.0f, 0.0f), surface_normal, shallow_normal_fade));
    surface_normal = lerp(
        surface_normal,
        normalize(float3(surface_normal.x * 2.0f, surface_normal.y, surface_normal.z * 2.0f)),
        water_speed_factor * shallow_normal_fade);
    surface_normal = normalize(surface_normal);

    const float3 view_dir = normalize(camera_position - ps_in.posW);
    const float3 light_dir = normalize(float3(-0.34f, 0.88f, 0.24f));

    float3 base_color = lerp(
        float3(0.25f, 0.40f, 0.45f),
        float3(0.40f, 0.30f, 0.20f),
        saturate(suspended_sediment * 5.0f));
    base_color = lerp(base_color, float3(0.60f, 0.60f, 0.65f), water_speed_factor);
    base_color *= diffuse_color.rgb;

    const float metallic = lerp(0.25f, 0.10f, water_speed_factor);
    const float specular = lerp(0.50f, 0.10f, water_speed_factor);
    const float roughness = lerp(0.15f, 0.30f, water_speed_factor);

    PbrSurface water_surface;
    water_surface.baseColor = base_color;
    water_surface.normal = surface_normal;
    water_surface.viewDir = view_dir;
    water_surface.lightDir = light_dir;
    water_surface.lightColor = float3(1.0f, 0.96f, 0.88f) * (1.0f + highlight_strength);
    water_surface.ambientColor = float3(0.08f, 0.10f, 0.12f);
    water_surface.roughness = roughness;
    water_surface.metallic = ps_in.isFrontFace ? metallic : 0.0f;
    water_surface.specular = ps_in.isFrontFace ? specular : 0.01f;
    water_surface.ambientOcclusion = 1.0f;
    water_surface.shadow = 1.0f;

    float3 water_color = DefaultPbrShading(water_surface);
    water_color += base_color * 0.18f;

    const int2 pixel = int2(ps_in.posH.xy);
    const float scene_depth = scene_depth_tex.Load(int3(pixel, 0)).r;
    const float scene_sample_valid = scene_depth < 0.99995f ? 1.0f : 0.0f;
    const float2 screen_uv = ComputeScreenUv(ps_in.posH);
    const float3 scene_world_pos = ComputeSceneWorldPos(screen_uv, scene_depth);
    const float scene_depth_fade =
        scene_sample_valid > 0.5f
            ? length(scene_world_pos - ps_in.posW)
            : 0.0f;
    const float water_absorption =
        saturate(water_depth * 0.10f + scene_depth_fade * 0.035f);
    water_color = lerp(water_color, float3(0.42f, 0.62f, 0.68f), water_absorption);

    const float edge_fade_distance = 0.40f;
    const float camera_height_delta = camera_position.y - ps_in.posW.y;
    float alpha =
        ps_in.isFrontFace
            ? saturate(pow(saturate(scene_depth_fade * 0.10f), 0.25f))
            : clamp(-camera_height_delta * 0.01f + 0.5f, 0.8f, 1.0f);
    alpha -= 1.0f - saturate(scene_depth_fade / edge_fade_distance);
    alpha = max(alpha, 0.0f);
    alpha *= scene_sample_valid;

    const float camera_horizontality =
        pow(saturate(length(normalize(ps_in.posW - camera_position).xz)), 0.2f);
    alpha *= saturate(9.0f + 6.0f * log(max(length(ps_in.posW - camera_position), 1.0f)) * camera_horizontality);
    const float depth_opacity =
        smoothstep(0.08f, 0.85f, max(water_depth, 0.0f)) *
        lerp(0.72f, 0.98f, saturate(scene_depth_fade * 0.08f));
    alpha = max(alpha, depth_opacity);
    alpha = saturate(alpha);

    return float4(saturate(water_color), alpha);
}
