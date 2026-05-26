Texture2D MainTex : register(t0);
Texture2D BloomTex : register(t1);
Texture2D SceneDepthTex : register(t2);
Texture2D TerrainHeightTex : register(t3);
SamplerState Sampler : register(s0);

cbuffer CB_PostProcess : register(b0)
{
    float g_Time;
    float g_GlitchAmount;
    float2 padding0;
    float3 g_CameraPosition;
    float g_UnderwaterEnabled;
    float4 g_WaterSurfaceRect;
    float g_WaterHeight;
    float g_UnderwaterDepthRange;
    float2 padding1;
    float4 g_UnderwaterTint;
    float4 g_PostFxPrimary;
    float4 g_PostFxSecondary;
    float4 g_PostFxFog;
    float4 g_PostFxScatter;
    float4 g_PostFxTone;
    float4x4 g_InverseViewProjection;
};

struct PS_INPUT
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD;
};

float rand(float seed)
{
    return frac(sin(seed * 12.9898) * 43758.5453);
}

float rand2(float2 seed)
{
    return frac(sin(dot(seed, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float2 ComputeWaterUV(float2 world_xz)
{
    float2 half_size = max(g_WaterSurfaceRect.zw * 0.5f, float2(1.0e-4f, 1.0e-4f));
    return (world_xz - g_WaterSurfaceRect.xy) / (half_size * 2.0f) + 0.5f;
}

float2 SampleTerrainWaterHeights(float2 world_xz)
{
    float2 terrain_water_height = float2(0.0f, 0.0f);
    float2 water_uv = ComputeWaterUV(world_xz);
    if (!any(water_uv < 0.0f) && !any(water_uv > 1.0f))
    {
        terrain_water_height =
            TerrainHeightTex.SampleLevel(Sampler, saturate(water_uv), 0.0f).xy;
    }

    return terrain_water_height;
}

float ComputeCameraUnderwaterAmount()
{
    float underwater_amount = 0.0f;
    if (g_UnderwaterEnabled > 0.5f)
    {
        float2 terrain_water_height = SampleTerrainWaterHeights(g_CameraPosition.xz);
        float local_water_depth = max(terrain_water_height.y - terrain_water_height.x, 0.0f);
        float water_presence = smoothstep(0.004f, 0.040f, local_water_depth);
        float resolved_surface_height =
            local_water_depth > 1.0e-4f
                ? terrain_water_height.y
                : g_WaterHeight;
        float submerge_depth = resolved_surface_height - g_CameraPosition.y;
        float immediate_contact = smoothstep(-0.01f, 0.06f, submerge_depth);
        float depth_fill = saturate(submerge_depth / max(g_UnderwaterDepthRange, 0.05f));
        float submerge = max(immediate_contact * 0.70f, depth_fill);
        underwater_amount = water_presence * submerge;
    }

    return underwater_amount;
}

float3 ComputeNearWorldPos(float2 uv)
{
    float2 ndc_xy = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clip_near = float4(ndc_xy, 0.0f, 1.0f);
    float4 world_near = mul(clip_near, g_InverseViewProjection);
    world_near.xyz /= max(world_near.w, 1.0e-6f);
    return world_near.xyz;
}

float SampleSceneDepth(float4 posH)
{
    int2 pixel = int2(posH.xy);
    return SceneDepthTex.Load(int3(pixel, 0)).r;
}

float3 ComputeSceneWorldPos(float2 uv, float raw_depth)
{
    float2 ndc_xy = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clip_pos = float4(ndc_xy, raw_depth, 1.0f);
    float4 world_pos = mul(clip_pos, g_InverseViewProjection);
    world_pos.xyz /= max(world_pos.w, 1.0e-6f);
    return world_pos.xyz;
}

float3 ComputeFogEnvColor(float3 view_dir)
{
    float sky_t = saturate(view_dir.y * 0.5f + 0.5f);
    float horizon_boost = saturate(1.0f - abs(view_dir.y));
    float3 horizon_color = float3(0.83f, 0.78f, 0.76f);
    float3 zenith_color = float3(0.66f, 0.74f, 0.88f);
    float3 env_color = lerp(horizon_color, zenith_color, pow(sky_t, 0.72f));
    env_color += horizon_boost * float3(0.035f, 0.030f, 0.020f);
    return env_color;
}

float3 ComputeFogScatterColor(float3 view_dir, float3 env_color)
{
    const float3 sun_dir = normalize(float3(-0.24f, 0.92f, 0.30f));
    const float3 sun_color = float3(1.00f, 0.84f, 0.68f);
    float scatter_focus = lerp(8.0f, 28.0f, saturate(g_PostFxScatter.y));
    float sun_forward = pow(saturate(dot(view_dir, sun_dir)), scatter_focus);
    float horizon_scatter = pow(saturate(1.0f - abs(view_dir.y)), 1.3f);
    float ambient_boost = 0.10f + horizon_scatter * 0.30f;
    float3 scatter_color = env_color * (1.0f + ambient_boost);
    scatter_color += sun_color * (sun_forward * (0.35f + horizon_scatter * 0.55f));
    return scatter_color;
}

float ComputeHeightFogFactor(float3 world_pos, float scene_distance)
{
    float fog_term = max(
        g_CameraPosition.y - world_pos.y +
        g_PostFxFog.z +
        scene_distance * g_PostFxFog.y,
        0.0f);
    float height_fog = fog_term * 0.01f;
    height_fog = height_fog * height_fog * g_PostFxFog.x;
    return saturate(1.0f - exp(-2.0f * height_fog));
}

float ComputeUnderwaterScreenFactor(float3 near_world_pos)
{
    float2 terrain_water_height = SampleTerrainWaterHeights(near_world_pos.xz);
    float local_water_depth = max(terrain_water_height.y - terrain_water_height.x, 0.0f);
    float water_presence = smoothstep(0.004f, 0.040f, local_water_depth);
    float resolved_surface_height =
        local_water_depth > 1.0e-4f
            ? terrain_water_height.y
            : g_WaterHeight;
    float underwater_height = resolved_surface_height - near_world_pos.y;
    return water_presence * smoothstep(0.0f, 0.018f, underwater_height + 0.010f);
}

float3 ApplyUnderwater(float3 color, float underwater_amount)
{
    float3 result = color;

    if (underwater_amount > 0.0f)
    {
        float luminance = dot(color, float3(0.299f, 0.587f, 0.114f));
        float3 desaturated = lerp(color, luminance.xxx, 0.18f * underwater_amount);
        float3 fogged = lerp(desaturated, g_UnderwaterTint.rgb, 0.22f + underwater_amount * 0.28f);
        result = lerp(color, fogged, underwater_amount);
    }

    return result;
}

float3 ApplyHeightFog(float3 color, float3 world_pos, float scene_distance, float raw_depth)
{
    float fog_enabled = (g_PostFxFog.x > 0.0f && raw_depth < 0.99995f) ? 1.0f : 0.0f;
    float3 view_delta = world_pos - g_CameraPosition;
    float view_len_sq = max(dot(view_delta, view_delta), 1.0e-6f);
    float3 view_dir = view_delta * rsqrt(view_len_sq);
    float3 env_color = ComputeFogEnvColor(view_dir);
    float3 scatter_color = ComputeFogScatterColor(view_dir, env_color);
    float fog_factor = ComputeHeightFogFactor(world_pos, scene_distance) * fog_enabled;
    float scatter_mix = saturate(g_PostFxScatter.x) * saturate(fog_factor * 1.15f);
    float3 fog_base = lerp(color, env_color, saturate(g_PostFxFog.w));
    float3 fog_target = lerp(fog_base, scatter_color, scatter_mix);
    return lerp(color, fog_target, fog_factor);
}

float2 ApplyGlitchUV(float2 uv)
{
    float2 result_uv = uv;

    if (g_GlitchAmount > 0.0f)
    {
        float slices = 25.0f;
        float sliceY = floor(uv.y * slices);
        float noise = rand(sliceY + floor(g_Time * 10.0f));

        if (noise < g_GlitchAmount)
        {
            result_uv.x += (rand(noise) - 0.5f) * 0.1f * g_GlitchAmount;
        }
    }

    return result_uv;
}

float3 SampleSceneColor(float2 uv)
{
    float chromatic_aberration = g_PostFxSecondary.x;
    float2 radial = uv - 0.5f;
    float radius = saturate(length(radial) * 2.05f);
    float edge_strength = pow(smoothstep(0.12f, 0.95f, radius), 1.65f);
    float2 chroma_offset = radial * chromatic_aberration * lerp(0.15f, 2.10f, edge_strength);

    if (g_GlitchAmount > 0.0f)
    {
        chroma_offset *= 1.0f + g_GlitchAmount * 1.6f;
    }

    float3 scene_color = MainTex.Sample(Sampler, saturate(uv)).rgb;
    float g = MainTex.Sample(Sampler, saturate(uv - chroma_offset)).g;
    float b = MainTex.Sample(Sampler, saturate(uv - chroma_offset * 2.25f)).b;
    return float3(scene_color.r, g, b);
}

float3 ApplyFilmGrain(float3 color, float2 uv, float2 pixel_pos, float raw_depth)
{
    float grain_strength = g_PostFxPrimary.x;
    float grain_debug_boost = max(g_PostFxPrimary.z, 1.0f);
    float3 result = color;

    if (grain_strength > 0.0f && raw_depth < 0.99995f)
    {
        float grain_speed = g_PostFxPrimary.y;
        float update_rate = lerp(10.0f, 24.0f, saturate(grain_speed * 0.25f));
        float time_seed = frac(floor(g_Time * update_rate) * 0.073f);
        float2 grain_pixel = floor(pixel_pos * 0.85f);

        float grain_a = rand2(grain_pixel + float2(time_seed * 127.1f, time_seed * 311.7f));
        float grain_b = rand2(grain_pixel.yx * 0.73f + float2(time_seed * 191.4f, time_seed * 83.2f));
        float grain_c = rand2((grain_pixel + 17.0f) * 1.13f + float2(time_seed * 53.7f, time_seed * 271.9f));
        float grain_luma = ((grain_a + grain_b) * 0.5f - 0.5f) * 2.0f;
        float3 grain_rgb = float3(grain_luma, grain_luma, grain_luma);
        grain_rgb = lerp(grain_rgb, float3(grain_a, grain_b, grain_c) * 2.0f - 1.0f, 0.12f);

        float luminance = dot(color, float3(0.299f, 0.587f, 0.114f));
        float midtone_visibility = 1.0f - abs(luminance - 0.42f) * 1.75f;
        float tone_visibility = saturate(lerp(0.42f, 1.0f, saturate(midtone_visibility)));
        float grain_amount = grain_strength * grain_debug_boost * 0.28f * tone_visibility;
        result = saturate(color + grain_rgb * grain_amount);
    }

    return result;
}

float3 ApplyAtmosphericDust(float3 color, float2 uv, float scene_distance, float raw_depth)
{
    float dust_strength = g_PostFxSecondary.w;
    float3 result = color;

    if (dust_strength > 0.0f && raw_depth < 0.99995f)
    {
        float time_seed = frac(floor(g_Time * 20.0f) * 0.05f);
        float3 dust_hash = float3(
            rand2(uv * float2(817.0f, 593.0f) + float2(time_seed, time_seed * 1.4f)),
            rand2(uv.yx * float2(467.0f, 997.0f) + float2(time_seed * 0.7f, time_seed * 1.1f)),
            rand2((uv + uv.yx) * float2(709.0f, 389.0f) + float2(time_seed * 1.8f, time_seed * 0.5f)));
        float dust_grey = dot(dust_hash, float3(0.3333f, 0.3333f, 0.3333f));
        float3 base_noise = lerp(dust_hash, dust_grey.xxx, 0.5f) * 2.0f - 1.0f;
        float sparse_threshold = saturate(0.14f + dust_strength * 0.95f);
        base_noise = any(abs(base_noise) > sparse_threshold.xxx) ? 0.0f.xxx : base_noise;

        float distance_visibility = smoothstep(40.0f, 260.0f, scene_distance);
        float depth_visibility = smoothstep(0.18f, 0.88f, raw_depth);
        float dust_visibility = saturate(max(distance_visibility, depth_visibility) * 0.42f);
        float luminance = dot(color, float3(0.299f, 0.587f, 0.114f));
        float highlight_suppression = lerp(1.0f, 0.68f, saturate(luminance * 1.05f));
        float dust_amount = dust_strength * dust_visibility * highlight_suppression;
        float3 dust_color = lerp(base_noise, dust_grey.xxx * 2.0f - 1.0f, 0.65f);
        result = saturate(color + dust_color * dust_amount);
    }

    return result;
}

float3 ApplyVignette(float3 color, float2 uv)
{
    float vignette_intensity = g_PostFxPrimary.w;
    float3 result = color;

    if (vignette_intensity > 0.0f)
    {
        float vignette_hardness = saturate(g_PostFxSecondary.z) * 0.5f;
        float vignette_scale = lerp(0.42f, 0.90f, saturate(vignette_intensity / 0.60f));
        float vignette_mask = 1.0f - smoothstep(
            min(vignette_hardness, 0.5f),
            max(1.0f - vignette_hardness, 0.501f),
            saturate(distance(0.5f.xx, uv) * vignette_scale));
        float vignette_mix = saturate(vignette_intensity * 1.35f);
        result = color * lerp(1.0f, vignette_mask, vignette_mix);
    }

    return result;
}

float3 ApplyBloom(float3 color, float2 uv)
{
    float bloom_intensity = g_PostFxSecondary.y;
    float3 result = color;

    if (bloom_intensity > 0.0f)
    {
        float3 bloom = BloomTex.Sample(Sampler, saturate(uv)).rgb;
        result = color + bloom * bloom_intensity;
    }

    return result;
}

float3 ToneMapReinhard(float3 color)
{
    return color / (1.0f + color);
}

float3 ToneMapACES(float3 color)
{
    return saturate((color * (2.51f * color + 0.03f)) / (color * (2.43f * color + 0.59f) + 0.14f));
}

float3 ToneMapFilmic(float3 color)
{
    color = max(0.0f.xxx, color - 0.004f);
    return (color * (6.2f * color + 0.5f)) / (color * (6.2f * color + 1.7f) + 0.06f);
}

float3 ApplyToneMapping(float3 color)
{
    color = max(color, 0.0f.xxx);
    color *= exp2(g_PostFxTone.x);

    int tone_mode = (int)(g_PostFxTone.y + 0.5f);
    if (tone_mode == 1)
    {
        color = ToneMapReinhard(color);
    }
    else if (tone_mode == 2)
    {
        color = ToneMapACES(color);
    }
    else if (tone_mode == 3)
    {
        color = ToneMapFilmic(color);
    }

    color *= max(g_PostFxTone.w, 0.0f);
    float gamma_value = max(g_PostFxTone.z, 0.10f);
    color = pow(saturate(color), 1.0f / gamma_value);
    return saturate(color);
}

float4 main(PS_INPUT input_pixel) : SV_TARGET
{
    float2 uv = input_pixel.uv;
    float3 near_world_pos = ComputeNearWorldPos(uv);
    float raw_scene_depth = SampleSceneDepth(input_pixel.posH);
    float3 scene_world_pos = ComputeSceneWorldPos(uv, raw_scene_depth);
    float scene_distance = length(scene_world_pos - g_CameraPosition);
    float camera_underwater_amount = ComputeCameraUnderwaterAmount();
    float underwater_screen_factor = ComputeUnderwaterScreenFactor(near_world_pos);
    float underwater_amount = max(underwater_screen_factor, camera_underwater_amount * 0.35f);
    float2 sample_uv = ApplyGlitchUV(uv);
    float3 color = SampleSceneColor(sample_uv);
    color = ApplyHeightFog(color, scene_world_pos, scene_distance, raw_scene_depth);
    color = ApplyUnderwater(color, underwater_amount);
    color = ApplyBloom(color, sample_uv);
    color = ApplyFilmGrain(color, uv, input_pixel.posH.xy, raw_scene_depth);
    color = ApplyAtmosphericDust(color, uv, scene_distance, raw_scene_depth);
    color = ApplyVignette(color, uv);
    color = ApplyToneMapping(color);
    return float4(color, 1.0f);
}
