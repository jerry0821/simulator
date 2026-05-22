cbuffer PS_CONSTANT_BUFFER0 : register(b0)
{
    float4 diffuse_color;
};

cbuffer PS_CONSTANT_BUFFER1 : register(b1)
{
    float3 camera_position;
    float fresnel_power;
    float highlight_strength;
    float time_seconds;
    float padding0;
    float padding1;
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

float2 Hash22(float2 p)
{
    const float2 h = float2(
        dot(p, float2(127.1f, 311.7f)),
        dot(p, float2(269.5f, 183.3f)));
    return frac(sin(h) * 43758.5453f);
}

float4 main(PS_INPUT ps_in) : SV_TARGET
{
    float2 worldUV = WorldToFieldUv(ps_in.posW.xz);
    const float2 sample_uv = ComputeWaterSampleUv(worldUV);
    const float2 terrain_water_height = water_surface_height_tex.SampleLevel(samp, sample_uv, 0.0f).xy;
    const float water_depth = max(terrain_water_height.y - terrain_water_height.x, 0.0f);
    const float water_visibility = smoothstep(0.006f, 0.030f, water_depth);

    const float4 velocity_sample = water_velocity_tex.SampleLevel(samp, sample_uv, 0.0f);
    const float4 sediment_sample = water_sediment_tex.SampleLevel(samp, sample_uv, 0.0f);

    const float2 flow_velocity = velocity_sample.xy;
    const float water_speed_factor = saturate(length(flow_velocity) * 8.0f + velocity_sample.z * 0.65f);
    const float suspended_sediment = saturate(sediment_sample.x);

    const float2 coord_offset =
        (Hash22(ceil((ps_in.posH.xy + ps_in.posW.xz) * 128.0f) * 0.001f) * 2.0f - 1.0f) * 0.35f;
    const float2 normal_uv = ComputeWaterSampleUv(WorldToFieldUv(ps_in.posW.xz + coord_offset));
    const float4 packed_normal_sample = terrain_normal_tex.SampleLevel(samp, normal_uv, 0.0f);
    float3 terrain_normal = DecodeUpNormal(packed_normal_sample.xy);
    float3 surface_normal = DecodeUpNormal(packed_normal_sample.zw);
    surface_normal = lerp(surface_normal, normalize(float3(surface_normal.xy * 2.0f, surface_normal.z)), water_speed_factor);
    surface_normal = normalize(lerp(surface_normal, terrain_normal, saturate(0.50f - water_depth * 4.5f)));

    const float3 light_dir = normalize(float3(-0.34f, 0.88f, 0.24f));
    const float3 view_dir = normalize(camera_position - ps_in.posW);
    const float3 half_dir = normalize(light_dir + view_dir);
    const float fresnel = pow(1.0f - saturate(dot(surface_normal, view_dir)), fresnel_power);
    const float ndotl = saturate(dot(surface_normal, light_dir));
    const float ndoth = saturate(dot(surface_normal, half_dir));

    float4 base_color = lerp(
        float4(0.25f, 0.40f, 0.45f, 1.0f),
        float4(0.40f, 0.30f, 0.20f, 1.0f),
        saturate(suspended_sediment * 5.0f));
    base_color = lerp(base_color, float4(0.60f, 0.60f, 0.65f, 1.0f), water_speed_factor);
    base_color.rgb *= diffuse_color.rgb;

    const float specular_strength = lerp(0.50f, 0.10f, water_speed_factor) * (0.55f + highlight_strength * 0.45f);
    const float roughness = lerp(0.15f, 0.30f, water_speed_factor);
    const float specular = pow(ndoth, lerp(56.0f, 18.0f, roughness)) * specular_strength;
    const float3 ambient = base_color.rgb * float3(0.18f, 0.20f, 0.22f);
    const float3 diffuse = base_color.rgb * (0.18f + ndotl * 0.82f);
    const float3 reflection_tint = float3(0.58f, 0.72f, 0.90f) * (0.16f + water_speed_factor * 0.10f);
    float3 water_color = ambient + diffuse + specular.xxx;
    water_color += reflection_tint * fresnel;
    water_color = lerp(water_color, float3(0.92f, 0.94f, 0.98f), water_speed_factor * 0.08f);

    const int2 pixel = int2(ps_in.posH.xy);
    const float scene_depth = scene_depth_tex.Load(int3(pixel, 0)).r;
    const float pixel_depth = ps_in.posH.z;
    const float depth_fade = max(scene_depth - pixel_depth, 0.0f);
    const float depth_factor = smoothstep(0.012f, 0.22f, water_depth);
    const float deep_factor = smoothstep(0.04f, 0.34f, water_depth);
    const float scene_edge_alpha = pow(saturate(depth_fade * 24.0f), 0.28f);
    const float base_alpha =
        diffuse_color.a * water_visibility * (0.24f + depth_factor * 0.42f + deep_factor * 0.16f) +
        fresnel * water_visibility * 0.12f +
        water_speed_factor * water_visibility * 0.05f;
    float alpha = max(base_alpha, scene_edge_alpha * diffuse_color.a * water_visibility);
    alpha = lerp(alpha, alpha * 0.92f + deep_factor * 0.04f, ps_in.isFrontFace ? 1.0f : 0.0f);
    alpha *= saturate(0.72f + log(length(ps_in.posW - camera_position) + 1.0f) * 0.16f);
    alpha = saturate(alpha);
    clip(alpha - 0.010f);

    return float4(saturate(water_color), alpha);
}
