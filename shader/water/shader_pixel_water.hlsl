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
    float surface_center_x;
    float surface_center_z;
    float surface_size_x;
    float surface_size_z;
    float padding0;
    float padding1;
};

struct PS_INPUT
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

Texture2D surface_water_tex;
Texture2D flow_field_tex : register(t1);
Texture2D scene_depth_tex : register(t2);
SamplerState samp;

struct WaterMaskState
{
    float coverage;
    float depth_gap;
    float depth_fade;
    float absorption;
    float extinction;
    float shoreline;
};

struct WaterWaveState
{
    float2 wind_dir;
    float wind_strength;
    float2 dir_large;
    float2 dir_small;
    float speed_large;
    float speed_small;
    float height_center;
    float height_x;
    float height_z;
    float crest;
    float3 normalW;
};

struct WaterOpticalState
{
    float3 view_dir;
    float3 light_dir;
    float3 half_dir;
    float fresnel;
    float specular;
    float shoreline_band;
    float shoreline_interaction;
    float highlight_intensity;
    float3 body_color;
    float3 reflection_tint;
    float3 shoreline_tint;
    float alpha;
};

float2 SafeNormalize(float2 v)
{
    float len_sq = dot(v, v);
    float inv_len = rsqrt(max(len_sq, 1.0e-6f));
    float2 normalized = v * inv_len;
    return (len_sq < 1.0e-6f) ? float2(1.0f, 0.0f) : normalized;
}

float2 DecodeWindDirection(float4 field_sample)
{
    return SafeNormalize(field_sample.xy * 2.0f - 1.0f);
}

float4 SampleSmoothedWind(float2 uv)
{
    const float2 offset = float2(0.0035f, 0.0035f);
    float4 center = flow_field_tex.Sample(samp, uv);
    float4 x_pos = flow_field_tex.Sample(samp, saturate(uv + float2(offset.x, 0.0f)));
    float4 x_neg = flow_field_tex.Sample(samp, saturate(uv - float2(offset.x, 0.0f)));
    float4 y_pos = flow_field_tex.Sample(samp, saturate(uv + float2(0.0f, offset.y)));
    float4 y_neg = flow_field_tex.Sample(samp, saturate(uv - float2(0.0f, offset.y)));
    return (center * 0.40f) + (x_pos + x_neg + y_pos + y_neg) * 0.15f;
}

float2 Rotate2D(float2 v, float angle_radians)
{
    float s = sin(angle_radians);
    float c = cos(angle_radians);
    return float2(v.x * c - v.y * s, v.x * s + v.y * c);
}

float SampleCoverage(float2 uv)
{
    return surface_water_tex.Sample(samp, saturate(uv)).a;
}

float SampleSceneDepth(float4 posH)
{
    int2 pixel = int2(posH.xy);
    return scene_depth_tex.Load(int3(pixel, 0)).r;
}

float WaveLayer(float2 uv, float2 direction, float scale, float speed, float phase_bias)
{
    float2 perp = float2(-direction.y, direction.x);
    float along = dot(uv, direction) * scale;
    float cross = dot(uv, perp) * (scale * 0.22f);
    float phase = along * 6.2831853f - time_seconds * speed + phase_bias;
    phase += sin(cross * 6.2831853f + phase_bias * 0.85f) * 0.28f;

    float primary = sin(phase);
    float secondary = sin(phase * 0.52f + cross * 1.15f + phase_bias * 0.45f) * 0.32f;
    return primary * 0.84f + secondary * 0.16f;
}

float ComputeWaveHeight(float2 uv, float2 dir_large, float2 dir_small, float speed_large, float speed_small)
{
    float large = WaveLayer(uv, dir_large, 2.9f, speed_large, 0.0f);
    float small = WaveLayer(uv, dir_small, 5.2f, speed_small, 2.1f);
    return large * 0.78f + small * 0.22f;
}

WaterMaskState ComputeMaskState(PS_INPUT ps_in)
{
    WaterMaskState state = (WaterMaskState)0;
    state.coverage = SampleCoverage(ps_in.uv);
    float scene_depth = SampleSceneDepth(ps_in.posH);
    float pixel_depth = saturate(ps_in.posH.z);
    state.depth_gap = scene_depth - pixel_depth;
    state.depth_fade = saturate(pow(max(state.depth_gap, 0.0f) * 38.0f, 0.54f));
    state.absorption = saturate(pow(max(state.depth_gap, 0.0f) * 14.0f, 0.78f));
    state.extinction = saturate(1.0f - exp2(-max(state.depth_gap, 0.0f) * 18.0f));
    state.shoreline = 1.0f - smoothstep(0.0025f, 0.020f, max(state.depth_gap, 0.0f));
    return state;
}

WaterWaveState ComputeWaveState(float2 uv)
{
    WaterWaveState state = (WaterWaveState)0;
    float4 wind_sample = SampleSmoothedWind(uv);
    state.wind_dir = DecodeWindDirection(wind_sample);
    state.wind_strength = saturate(wind_sample.z);

    float local_turbulence = sin(dot(uv, float2(1.2f, 0.9f)) * 6.2831853f) * 0.006f;
    state.dir_large = state.wind_dir;
    const float2 autonomous_small_dir = normalize(float2(0.34f, 0.94f));
    float2 wind_biased_small_dir = Rotate2D(state.wind_dir, 0.10f + local_turbulence);
    state.dir_small = SafeNormalize(lerp(autonomous_small_dir, wind_biased_small_dir, 0.22f + state.wind_strength * 0.10f));

    state.speed_large = 0.138f;
    state.speed_small = 0.086f;

    float2 ripple_step = float2(
        5.0f / max(surface_size_x, 1.0f),
        5.0f / max(surface_size_z, 1.0f));

    state.height_center = ComputeWaveHeight(uv, state.dir_large, state.dir_small, state.speed_large, state.speed_small);
    state.height_x = ComputeWaveHeight(uv + float2(ripple_step.x, 0.0f), state.dir_large, state.dir_small, state.speed_large, state.speed_small);
    state.height_z = ComputeWaveHeight(uv + float2(0.0f, ripple_step.y), state.dir_large, state.dir_small, state.speed_large, state.speed_small);
    state.crest = saturate(abs(state.height_x - state.height_center) + abs(state.height_z - state.height_center));

    state.normalW = normalize(float3(
        -(state.height_x - state.height_center) * 1.40f,
        1.0f,
        -(state.height_z - state.height_center) * 1.40f));

    return state;
}

float3 ComputeWaterBodyColor(WaterMaskState mask_state)
{
    float3 shallow_tint = diffuse_color.rgb * float3(0.98f, 1.00f, 0.98f);
    float3 mid_tint = diffuse_color.rgb * float3(0.78f, 0.88f, 1.02f);
    float3 deep_tint = diffuse_color.rgb * float3(0.58f, 0.72f, 0.94f);
    float3 body = lerp(shallow_tint, mid_tint, saturate(mask_state.absorption * 1.10f));
    body = lerp(body, deep_tint, saturate(mask_state.extinction));
    return body;
}

float ComputeWaterAlpha(WaterMaskState mask_state, WaterOpticalState optical_state)
{
    return saturate(
        mask_state.coverage * lerp(max(diffuse_color.a, 0.26f), 0.86f, saturate(mask_state.depth_fade * 0.72f + mask_state.extinction * 0.56f)) +
        optical_state.highlight_intensity * 0.22f +
        optical_state.shoreline_interaction * 0.08f);
}

WaterOpticalState ComputeOpticalState(
    PS_INPUT ps_in,
    float2 uv,
    WaterMaskState mask_state,
    WaterWaveState wave_state)
{
    WaterOpticalState state = (WaterOpticalState)0;
    state.view_dir = normalize(camera_position - ps_in.posW);
    state.light_dir = normalize(float3(-0.28f, 0.88f, 0.20f));
    state.half_dir = normalize(state.light_dir + state.view_dir);
    state.fresnel = pow(1.0f - saturate(dot(wave_state.normalW, state.view_dir)), max(fresnel_power, 1.0f));
    state.specular = pow(saturate(dot(wave_state.normalW, state.half_dir)), 44.0f);

    state.shoreline_band = 0.5f + 0.5f * sin(dot(uv, wave_state.dir_large) * 20.0f - time_seconds * wave_state.speed_large * 1.7f);
    state.shoreline_band = smoothstep(0.58f, 0.92f, state.shoreline_band);
    state.shoreline_interaction =
        mask_state.shoreline *
        state.shoreline_band *
        saturate(0.20f + wave_state.wind_strength * 0.80f);

    state.highlight_intensity =
        state.specular * (0.82f + highlight_strength * 1.35f) +
        state.fresnel * (0.10f + highlight_strength * 0.30f) +
        wave_state.crest * 0.12f;
    state.highlight_intensity *= (1.00f + wave_state.wind_strength * 0.12f);
    state.highlight_intensity += state.shoreline_interaction * (0.10f + highlight_strength * 0.16f);
    state.highlight_intensity *= mask_state.coverage * saturate(0.35f + mask_state.depth_fade * 0.85f);

    state.body_color = ComputeWaterBodyColor(mask_state);
    state.reflection_tint = lerp(float3(0.76f, 0.86f, 0.96f), float3(1.0f, 1.0f, 1.0f), state.specular);
    state.shoreline_tint = lerp(float3(0.72f, 0.82f, 0.88f), float3(0.95f, 0.97f, 1.0f), wave_state.wind_strength);
    state.alpha = ComputeWaterAlpha(mask_state, state);
    return state;
}

float3 ComposeWaterColor(WaterMaskState mask_state, WaterWaveState wave_state, WaterOpticalState optical_state)
{
    return
        optical_state.body_color * lerp(0.42f, 0.96f, saturate(mask_state.depth_fade * 0.68f + mask_state.extinction * 0.58f)) +
        optical_state.reflection_tint * (0.18f * optical_state.fresnel + 0.95f * optical_state.specular + 0.10f * wave_state.crest) +
        optical_state.shoreline_tint * optical_state.shoreline_interaction * 0.34f;
}

float4 main(PS_INPUT ps_in) : SV_TARGET
{
    WaterMaskState mask_state = ComputeMaskState(ps_in);
    if (mask_state.coverage <= 0.01f || mask_state.depth_gap <= 1.0e-4f)
    {
        discard;
    }

    float2 uv = saturate(ps_in.uv);
    WaterWaveState wave_state = ComputeWaveState(uv);
    WaterOpticalState optical_state = ComputeOpticalState(ps_in, uv, mask_state, wave_state);
    float3 color = ComposeWaterColor(mask_state, wave_state, optical_state);
    return float4(color, optical_state.alpha);
}
