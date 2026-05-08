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
    float shore_gradient;
    float2 shore_normal;
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
    float ripple_energy;
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
    float windward_shore;
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

float2 SurfaceTexelStep()
{
    return float2(
        1.25f / max(surface_size_x, 1.0f),
        1.25f / max(surface_size_z, 1.0f));
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

float CapillaryRippleLayer(float2 uv, float2 direction, float frequency, float speed, float phase_bias)
{
    float2 perp = float2(-direction.y, direction.x);
    float along = dot(uv, direction) * frequency;
    float cross = dot(uv, perp) * (frequency * 0.48f);
    float phase = along * 6.2831853f - time_seconds * speed + phase_bias;

    float interference_a = sin((along * 1.92f + cross * 0.63f) * 6.2831853f + phase_bias * 1.65f);
    float interference_b = sin((along * 2.87f - cross * 1.18f) * 6.2831853f - time_seconds * (speed * 0.38f) + phase_bias * 0.72f);
    float pulse = sin(phase + interference_a * 0.58f + interference_b * 0.24f);
    float streaks = sin((along - cross * 0.44f) * 12.5663706f - time_seconds * (speed * 1.42f) + phase_bias * 0.38f);
    float cross_chop = sin((cross * 1.76f + along * 0.36f) * 6.2831853f - time_seconds * (speed * 0.94f) + phase_bias * 1.13f);
    return pulse * 0.52f + streaks * 0.30f + cross_chop * 0.18f;
}

float ComputeWaveHeight(
    float2 uv,
    float2 dir_large,
    float2 dir_small,
    float speed_large,
    float speed_small,
    float wind_strength)
{
    float large = WaveLayer(uv, dir_large, 2.9f, speed_large, 0.0f);
    float small = WaveLayer(uv, dir_small, 5.2f, speed_small, 2.1f);

    float micro_primary = CapillaryRippleLayer(uv, dir_large, 13.5f, speed_large * 2.20f, 1.4f);
    float micro_cross = CapillaryRippleLayer(uv, dir_small, 17.0f, speed_small * 2.95f, 3.2f);
    float micro_weight = lerp(0.12f, 0.24f, wind_strength);
    float micro = (micro_primary * 0.58f + micro_cross * 0.42f) * micro_weight;

    return large * 0.68f + small * 0.16f + micro;
}

WaterMaskState ComputeMaskState(PS_INPUT ps_in)
{
    WaterMaskState state = (WaterMaskState)0;
    state.coverage = SampleCoverage(ps_in.uv);
    float2 shore_step = SurfaceTexelStep();
    float coverage_x_pos = SampleCoverage(ps_in.uv + float2(shore_step.x, 0.0f));
    float coverage_x_neg = SampleCoverage(ps_in.uv - float2(shore_step.x, 0.0f));
    float coverage_y_pos = SampleCoverage(ps_in.uv + float2(0.0f, shore_step.y));
    float coverage_y_neg = SampleCoverage(ps_in.uv - float2(0.0f, shore_step.y));
    float2 coverage_gradient = float2(
        coverage_x_pos - coverage_x_neg,
        coverage_y_pos - coverage_y_neg);

    state.shore_gradient = saturate(length(coverage_gradient) * 4.8f);
    state.shore_normal = SafeNormalize(coverage_gradient);
    float scene_depth = SampleSceneDepth(ps_in.posH);
    float pixel_depth = saturate(ps_in.posH.z);
    state.depth_gap = scene_depth - pixel_depth;
    state.depth_fade = saturate(pow(max(state.depth_gap, 0.0f) * 38.0f, 0.54f));
    state.absorption = saturate(pow(max(state.depth_gap, 0.0f) * 14.0f, 0.78f));
    state.extinction = saturate(1.0f - exp2(-max(state.depth_gap, 0.0f) * 18.0f));
    float edge_shallow = 1.0f - smoothstep(0.0025f, 0.020f, max(state.depth_gap, 0.0f));
    float edge_coverage = 1.0f - smoothstep(0.18f, 0.72f, state.coverage);
    state.shoreline = saturate(max(edge_shallow, state.shore_gradient * edge_coverage));
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
        2.5f / max(surface_size_x, 1.0f),
        2.5f / max(surface_size_z, 1.0f));

    state.height_center = ComputeWaveHeight(uv, state.dir_large, state.dir_small, state.speed_large, state.speed_small, state.wind_strength);
    state.height_x = ComputeWaveHeight(uv + float2(ripple_step.x, 0.0f), state.dir_large, state.dir_small, state.speed_large, state.speed_small, state.wind_strength);
    state.height_z = ComputeWaveHeight(uv + float2(0.0f, ripple_step.y), state.dir_large, state.dir_small, state.speed_large, state.speed_small, state.wind_strength);
    state.ripple_energy = saturate((abs(state.height_x - state.height_center) + abs(state.height_z - state.height_center)) * 1.65f);
    state.crest = saturate(state.ripple_energy * (0.72f + state.wind_strength * 0.46f));

    state.normalW = normalize(float3(
        -(state.height_x - state.height_center) * 2.05f,
        1.0f,
        -(state.height_z - state.height_center) * 2.05f));

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

    float wind_to_shore = saturate(dot(-wave_state.wind_dir, mask_state.shore_normal));
    float2 shoreline_tangent = float2(-mask_state.shore_normal.y, mask_state.shore_normal.x);
    float shoreline_wave_phase =
        dot(uv, -mask_state.shore_normal) * 92.0f -
        time_seconds * (1.65f + wave_state.wind_strength * 2.35f);
    shoreline_wave_phase += sin(dot(uv, shoreline_tangent) * 44.0f + time_seconds * 1.05f) * 0.32f;

    state.shoreline_band = 0.5f + 0.5f * sin(shoreline_wave_phase);
    state.shoreline_band = smoothstep(0.52f, 0.98f, state.shoreline_band);
    float ambient_shore =
        mask_state.shoreline *
        saturate(0.08f + wave_state.wind_strength * 0.20f + mask_state.shore_gradient * 0.18f);
    state.windward_shore =
        mask_state.shoreline *
        mask_state.shore_gradient *
        wind_to_shore *
        (0.34f + wave_state.wind_strength * 1.05f);
    state.shoreline_interaction = saturate(
        ambient_shore +
        state.windward_shore * (0.38f + state.shoreline_band * 1.05f));

    state.highlight_intensity =
        state.specular * (0.82f + highlight_strength * 1.35f) +
        state.fresnel * (0.10f + highlight_strength * 0.30f) +
        wave_state.crest * 0.12f;
    state.highlight_intensity *= (1.00f + wave_state.wind_strength * 0.12f + wave_state.ripple_energy * 0.18f);
    state.highlight_intensity += state.shoreline_interaction * (0.18f + highlight_strength * 0.24f + state.shoreline_band * 0.10f);
    state.highlight_intensity *= mask_state.coverage * saturate(0.35f + mask_state.depth_fade * 0.85f);

    state.body_color = ComputeWaterBodyColor(mask_state);
    state.reflection_tint = lerp(float3(0.76f, 0.86f, 0.96f), float3(1.0f, 1.0f, 1.0f), state.specular);
    state.shoreline_tint = lerp(
        float3(0.72f, 0.82f, 0.88f),
        float3(0.98f, 0.99f, 1.0f),
        saturate(wave_state.wind_strength * 0.55f + state.windward_shore * 1.20f));
    state.alpha = ComputeWaterAlpha(mask_state, state);
    return state;
}

float3 ComposeWaterColor(WaterMaskState mask_state, WaterWaveState wave_state, WaterOpticalState optical_state)
{
    return
        optical_state.body_color * lerp(0.42f, 0.96f, saturate(mask_state.depth_fade * 0.68f + mask_state.extinction * 0.58f)) +
        optical_state.reflection_tint * (0.18f * optical_state.fresnel + 0.95f * optical_state.specular + 0.14f * wave_state.crest + 0.08f * wave_state.ripple_energy) +
        optical_state.shoreline_tint * optical_state.shoreline_interaction * (0.34f + optical_state.shoreline_band * 0.24f + optical_state.windward_shore * 0.32f);
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
