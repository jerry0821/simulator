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

float4 main(PS_INPUT ps_in) : SV_TARGET
{
    float2 worldUV = WorldToFieldUv(ps_in.posW.xz);
    const float2 sample_uv = ComputeWaterSampleUv(worldUV);
    const float2 terrain_water_height = water_surface_height_tex.SampleLevel(samp, sample_uv, 0.0f).xy;
    const float water_depth = max(terrain_water_height.y - terrain_water_height.x, 0.0f);
    const float depth_visibility = smoothstep(0.003f, 0.014f, water_depth);

    const float4 velocity_sample = water_velocity_tex.SampleLevel(samp, sample_uv, 0.0f);
    const float4 sediment_sample = water_sediment_tex.SampleLevel(samp, sample_uv, 0.0f);
    const float4 packed_normal_sample = terrain_normal_tex.SampleLevel(samp, sample_uv, 0.0f);
    const float3 terrain_normal = DecodeUpNormal(packed_normal_sample.xy);
    const float3 water_normal = DecodeUpNormal(packed_normal_sample.zw);

    const float2 flow_velocity = velocity_sample.xy;
    const float water_speed = saturate(velocity_sample.z);
    const float transport_energy = saturate(velocity_sample.w);
    const float suspended_sediment = saturate(sediment_sample.x);
    const float deposition = saturate(sediment_sample.y);
    const float erosion = saturate(sediment_sample.z);
    const float sediment_capacity = saturate(sediment_sample.w);

    float3 surface_normal = water_normal;
    surface_normal.xz += flow_velocity * 0.0008f;
    surface_normal = normalize(surface_normal);
    surface_normal = normalize(lerp(surface_normal, terrain_normal, saturate(0.55f - water_depth * 5.0f)));

    const float3 view_dir = normalize(camera_position - ps_in.posW);
    const float3 light_dir = normalize(float3(-0.34f, 0.88f, 0.24f));
    const float fresnel = pow(1.0f - saturate(dot(surface_normal, view_dir)), fresnel_power);
    const float ndotl = saturate(dot(surface_normal, light_dir));
    const float specular = pow(saturate(dot(reflect(-light_dir, surface_normal), view_dir)), 28.0f) *
        (0.12f + highlight_strength * 0.56f);

    const float depth_factor = smoothstep(0.008f, 0.22f, water_depth);
    const float deep_factor = smoothstep(0.04f, 0.34f, water_depth);
    const float shallow_factor = 1.0f - deep_factor;
    const float muddiness = saturate(suspended_sediment * 0.70f + deposition * 0.22f);
    const float foam_hint =
        smoothstep(0.12f, 0.42f, water_speed + erosion * 0.20f) *
        smoothstep(0.004f, 0.05f, water_depth) *
        (0.22f);

    float3 shallow_color = diffuse_color.rgb * float3(0.84f, 1.02f, 1.10f);
    float3 deep_color = diffuse_color.rgb * float3(0.42f, 0.70f, 1.16f);
    float3 sediment_color = float3(0.41f, 0.33f, 0.24f);
    float3 sky_reflection = float3(0.58f, 0.72f, 0.90f);

    float3 water_color = lerp(shallow_color, deep_color, deep_factor);
    water_color = lerp(water_color, sediment_color, muddiness * shallow_factor * 0.58f);
    water_color += sky_reflection * fresnel * (0.22f + ndotl * 0.18f);
    water_color += float3(1.0f, 1.0f, 1.0f) * specular;
    water_color += float3(0.14f, 0.18f, 0.22f) * water_speed * depth_factor;
    water_color = lerp(water_color, float3(0.92f, 0.97f, 1.00f), foam_hint * 0.25f);

    float alpha =
        diffuse_color.a * 0.82f +
        depth_factor * 0.22f +
        fresnel * 0.18f +
        water_speed * 0.06f +
        sediment_capacity * 0.04f;
    alpha = saturate(alpha * depth_visibility);
    clip(alpha - 0.002f);

    return float4(saturate(water_color), alpha);
}
