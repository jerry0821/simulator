cbuffer PS_CONSTANT_BUFFER0 : register(b0)
{
    float4 diffuse_color;
};

cbuffer PS_CONSTANT_BUFFER1 : register(b6)
{
    float3 camera_position;
    float fresnel_power;
    float highlight_strength;
    float time_seconds;
    float padding0;
    float padding1;
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

Texture2D<float4> water_surface_height_tex : register(t0);
Texture2D<float4> water_velocity_tex : register(t1);
Texture2D<float4> water_sediment_tex : register(t2);
Texture2D<float4> terrain_normal_tex : register(t3);
Texture2D scene_depth_tex : register(t4);
Texture2D<float4> flow_normal_tex : register(t5);
SamplerState samp : register(s0);

static const float kWorldSideLength = 2048.0f;
static const float kWorldHalfExtent = kWorldSideLength * 0.5f;

float2 SafeNormalize2(float2 value)
{
    const float len_sq = dot(value, value);
    return len_sq > 1.0e-6f ? value * rsqrt(len_sq) : float2(1.0f, 0.0f);
}

float2 SafeWaveDir(float2 base_dir, float2 flow_dir, float flow_influence)
{
    return SafeNormalize2(base_dir + flow_dir * flow_influence);
}

float2 ComputeWaterSampleUv(float2 uv)
{
    uint tex_width = 0;
    uint tex_height = 0;
    water_surface_height_tex.GetDimensions(tex_width, tex_height);
    const float2 texel_size = 1.0f / max(float2(tex_width, tex_height), 1.0f.xx);
    const float2 half_texel = texel_size * 0.5f;
    return clamp(uv, half_texel, 1.0f.xx - half_texel);
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
    float2 worldUV = WorldToFieldUv(ps_in.posW.xz);
    const float2 sample_uv = ComputeWaterSampleUv(worldUV);
    const float2 terrain_water_height = water_surface_height_tex.SampleLevel(samp, sample_uv, 0.0f).xy;
    const float water_depth = max(terrain_water_height.y - terrain_water_height.x, 0.0f);
    const float water_visibility = smoothstep(0.004f, 0.024f, water_depth);

    const float4 velocity_sample = water_velocity_tex.SampleLevel(samp, sample_uv, 0.0f);
    const float4 sediment_sample = water_sediment_tex.SampleLevel(samp, sample_uv, 0.0f);

    const float2 flow_velocity = velocity_sample.xy;
    const float flow_speed = length(flow_velocity);
    const float water_speed_factor = saturate(length(flow_velocity) * 8.0f + velocity_sample.z * 0.65f);
    const float suspended_sediment = saturate(sediment_sample.x);
    const float2 flow_dir = SafeNormalize2(flow_velocity + float2(0.12f, 0.05f));

    const float2 normal_uv = ComputeWaterSampleUv(WorldToFieldUv(ps_in.posW.xz));
    const float4 packed_normal_sample = terrain_normal_tex.SampleLevel(samp, normal_uv, 0.0f);
    float3 terrain_normal = DecodeUpNormal(packed_normal_sample.xy);
    float3 surface_normal = DecodeUpNormal(packed_normal_sample.zw);
    const float slope_flatness = smoothstep(0.78f, 0.94f, terrain_normal.y);
    const float standing_visibility = smoothstep(0.020f, 0.080f, water_depth);
    const float runoff_visibility =
        smoothstep(0.010f, 0.040f, water_depth) *
        slope_flatness *
        lerp(0.30f, 0.72f, water_speed_factor);
    const float water_presence = max(standing_visibility, runoff_visibility);
    surface_normal = lerp(surface_normal, normalize(float3(surface_normal.xy * 2.0f, surface_normal.z)), water_speed_factor);

    const float camera_distance = length(ps_in.posW - camera_position);
    const float ripple_distance_fade = 1.0f - smoothstep(90.0f, 220.0f, camera_distance);
    const float ripple_strength = water_presence * ripple_distance_fade * (0.22f + water_speed_factor * 0.28f);
    const float phase = frac(time_seconds * (0.055f + flow_speed * 0.14f));
    const float phase_a = phase;
    const float phase_b = frac(phase + 0.5f);
    const float flow_blend = abs(phase * 2.0f - 1.0f);
    const float2 ripple_base_uv = ps_in.posW.xz * 0.018f;
    const float2 ripple_scroll = flow_dir * lerp(0.080f, 0.240f, saturate(flow_speed * 2.5f));
    const float2 ripple_uv_a = ripple_base_uv + ripple_scroll * phase_a;
    const float2 ripple_uv_b = ripple_base_uv * 1.37f - ripple_scroll * phase_b + float2(0.31f, 0.57f);
    const float2 flow_xy_a = flow_normal_tex.Sample(samp, ripple_uv_a).rg * 2.0f - 1.0f;
    const float2 flow_xy_b = flow_normal_tex.Sample(samp, ripple_uv_b).rg * 2.0f - 1.0f;
    const float2 flow_xy = lerp(flow_xy_a, flow_xy_b, flow_blend) * ripple_strength;
    const float ripple_luma = lerp(flow_xy_a.x + flow_xy_a.y, flow_xy_b.x + flow_xy_b.y, flow_blend) * 0.5f;
    const float3 flow_normal = DecodeUpNormal(flow_xy);

    surface_normal = normalize(lerp(surface_normal, flow_normal, ripple_strength));
    surface_normal = normalize(lerp(surface_normal, terrain_normal, saturate(0.50f - water_depth * 4.5f) + (1.0f - slope_flatness) * 0.85f));

    const float3 light_dir = normalize(float3(-0.34f, 0.88f, 0.24f));
    const float3 view_dir = normalize(camera_position - ps_in.posW);
    const float3 half_dir = normalize(light_dir + view_dir);
    const float fresnel = pow(1.0f - saturate(dot(surface_normal, view_dir)), fresnel_power);
    const float ndotl = saturate(dot(surface_normal, light_dir));
    const float ndoth = saturate(dot(surface_normal, half_dir));

    float4 base_color = lerp(
        float4(0.10f, 0.22f, 0.34f, 1.0f),
        float4(0.26f, 0.24f, 0.18f, 1.0f),
        saturate(suspended_sediment * 5.0f));
    base_color = lerp(base_color, float4(0.28f, 0.36f, 0.50f, 1.0f), water_speed_factor);
    base_color.rgb *= 1.0f + ripple_luma * ripple_strength * 0.18f;
    base_color.rgb *= diffuse_color.rgb;

    const float specular = 0.0f;
    const float deep_absorption = smoothstep(0.05f, 0.24f, water_depth);
    const float3 absorbed_color = lerp(base_color.rgb, base_color.rgb * float3(0.72f, 0.80f, 0.88f), deep_absorption);
    const float3 ambient = absorbed_color * float3(0.26f, 0.28f, 0.32f);
    const float3 diffuse = absorbed_color * (0.24f + ndotl * 0.56f);
    const float3 reflection_tint = float3(0.38f, 0.48f, 0.66f) * (0.03f + water_speed_factor * 0.02f);
    float3 water_color = ambient + diffuse + specular.xxx;
    water_color += reflection_tint * fresnel;
    water_color = lerp(water_color, float3(0.92f, 0.94f, 0.98f), water_speed_factor * 0.08f);

    const int2 pixel = int2(ps_in.posH.xy);
    const float scene_depth = scene_depth_tex.Load(int3(pixel, 0)).r;
    const float2 screen_uv = ComputeScreenUv(ps_in.posH);
    const float3 scene_world_pos = ComputeSceneWorldPos(screen_uv, scene_depth);
    const float scene_sample_valid = scene_depth < 0.99995f ? 1.0f : 0.0f;
    const float true_depth_fade =
        scene_sample_valid > 0.5f
            ? length(scene_world_pos - ps_in.posW)
            : 0.0f;
    const float depth_factor = smoothstep(0.012f, 0.22f, water_depth);
    const float deep_factor = smoothstep(0.04f, 0.34f, water_depth);
    const float standing_alpha = smoothstep(0.012f, 0.090f, water_depth);
    const float runoff_alpha =
        smoothstep(0.006f, 0.038f, water_depth) *
        slope_flatness *
        lerp(0.26f, 0.65f, water_speed_factor);
    const float depth_edge = smoothstep(0.03f, 0.45f, true_depth_fade);
    float alpha = max(standing_alpha, runoff_alpha);
    alpha = lerp(alpha, 1.0f, depth_edge * (0.18f + depth_factor * 0.32f));
    alpha *= water_presence;
    alpha *= saturate(0.72f + log(length(ps_in.posW - camera_position) + 1.0f) * 0.16f);
    alpha *= lerp(0.82f, 1.0f, saturate(depth_factor * 0.65f + deep_factor * 0.35f));
    alpha = saturate(alpha);
    clip(alpha - 0.003f);

    return float4(saturate(water_color), alpha);
}
