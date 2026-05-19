Texture2D<float4> g_PreviousWindField : register(t0);
Texture2D<float4> g_ClimateField : register(t1);
Texture2D g_TerrainHeight : register(t2);
SamplerState g_SurfaceSampler : register(s0);
RWTexture2D<float4> g_Output : register(u0);

cbuffer CS_WIND_FIELD : register(b0)
{
    float g_TimeSeconds;
    float g_DeltaTimeSeconds;
    float g_WindDirectionX;
    float g_WindDirectionY;
    float g_WindStrength;
    float g_WindCrossInfluence;
    float g_NoiseScale;
    float g_PressureWindScale;
    float g_SourceBlendRate;
    float g_PropagationScale;
    float g_TerrainGuidanceScale;
    float g_StormCoupling;
    uint g_Width;
    uint g_Height;
    uint g_InitializeState;
    uint g_Padding0;
};

float Hash21(float2 p)
{
    p = frac(p * float2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return frac(p.x * p.y);
}

float ValueNoise(float2 p)
{
    float2 cell = floor(p);
    float2 local = frac(p);
    float2 smooth = local * local * (3.0 - 2.0 * local);

    float v00 = Hash21(cell + float2(0.0, 0.0));
    float v10 = Hash21(cell + float2(1.0, 0.0));
    float v01 = Hash21(cell + float2(0.0, 1.0));
    float v11 = Hash21(cell + float2(1.0, 1.0));

    return lerp(lerp(v00, v10, smooth.x), lerp(v01, v11, smooth.x), smooth.y);
}

float FBM(float2 p)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    float2 domain = p;

    [unroll]
    for (int octave = 0; octave < 4; ++octave)
    {
        value += ValueNoise(domain) * amplitude;
        domain = domain * 2.03f + float2(17.0f, 9.0f);
        amplitude *= 0.5f;
    }

    return value;
}

float2 SafeNormalize(float2 v)
{
    float length_sq = dot(v, v);
    float2 result = float2(1.0f, 0.0f);
    if (length_sq >= 1e-6f)
    {
        result = v * rsqrt(length_sq);
    }
    return result;
}

float3 DecodeWindSample(float4 encoded)
{
    const float2 dir = SafeNormalize(encoded.xy * 2.0f - 1.0f);
    return float3(dir, saturate(encoded.z));
}

float4 EncodeWindSample(float2 dir, float strength, float storminess)
{
    const float2 encoded_dir = SafeNormalize(dir) * 0.5f + 0.5f;
    return float4(encoded_dir, saturate(strength), saturate(storminess));
}

float3 SampleClimate(float2 uv)
{
    return g_ClimateField.SampleLevel(g_SurfaceSampler, saturate(uv), 0.0f).rgb;
}

float SampleTerrainHeight(float2 uv)
{
    return g_TerrainHeight.SampleLevel(g_SurfaceSampler, saturate(uv), 0.0f).r;
}

struct TerrainFlowState
{
    float2 slope_dir;
    float2 contour_dir;
    float slope_amount;
    float ridge_mask;
    float valley_mask;
    float channel_mask;
};

TerrainFlowState ComputeTerrainFlow(float2 uv, float2 reference_dir)
{
    TerrainFlowState state = (TerrainFlowState)0;

    uint terrain_width = 0u;
    uint terrain_height = 0u;
    g_TerrainHeight.GetDimensions(terrain_width, terrain_height);
    float2 texel = float2(
        1.0f / max(float(terrain_width), 1.0f),
        1.0f / max(float(terrain_height), 1.0f));

    float height_center = SampleTerrainHeight(uv);
    float height_x_pos = SampleTerrainHeight(uv + float2(texel.x, 0.0f));
    float height_x_neg = SampleTerrainHeight(uv - float2(texel.x, 0.0f));
    float height_y_pos = SampleTerrainHeight(uv + float2(0.0f, texel.y));
    float height_y_neg = SampleTerrainHeight(uv - float2(0.0f, texel.y));

    float2 height_gradient = float2(
        height_x_pos - height_x_neg,
        height_y_pos - height_y_neg);
    float relief = max(max(height_x_pos, height_x_neg), max(height_y_pos, height_y_neg)) -
        min(min(height_x_pos, height_x_neg), min(height_y_pos, height_y_neg));
    float laplacian =
        (height_x_pos + height_x_neg + height_y_pos + height_y_neg) -
        height_center * 4.0f;

    state.slope_amount = saturate(length(height_gradient) * 0.22f + relief * 0.08f);
    state.ridge_mask = saturate((-laplacian) * 0.18f + relief * 0.05f);
    state.valley_mask = saturate(laplacian * 0.14f + relief * 0.04f);
    state.slope_dir = SafeNormalize(-height_gradient);
    state.contour_dir = float2(-state.slope_dir.y, state.slope_dir.x);
    if (dot(state.contour_dir, reference_dir) < 0.0f)
    {
        state.contour_dir *= -1.0f;
    }
    state.channel_mask = saturate(state.slope_amount * 0.55f + state.valley_mask * 0.65f);
    return state;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= g_Width || dispatch_thread_id.y >= g_Height)
    {
        return;
    }

    float2 uv = (dispatch_thread_id.xy + 0.5) / float2(g_Width, g_Height);
    const float2 texel = 1.0f / float2(g_Width, g_Height);
    const float wind_dt = max(min(g_DeltaTimeSeconds, 1.0f / 30.0f), 1.0e-4f);
    float2 wind_dir = SafeNormalize(float2(g_WindDirectionX, g_WindDirectionY));
    float2 cross_dir = float2(-wind_dir.y, wind_dir.x);
    const float3 climate_center = SampleClimate(uv);
    const float3 climate_right = SampleClimate(uv + float2(texel.x, 0.0f));
    const float3 climate_left = SampleClimate(uv - float2(texel.x, 0.0f));
    const float3 climate_up = SampleClimate(uv + float2(0.0f, texel.y));
    const float3 climate_down = SampleClimate(uv - float2(0.0f, texel.y));
    const float storminess = saturate(climate_center.y * (0.62f + climate_center.z * 0.38f));

    float2 domain = uv * max(g_NoiseScale, 1.0f);
    float2 drift = wind_dir * g_TimeSeconds * 0.0055f + cross_dir * sin(g_TimeSeconds * 0.0035f) * 0.020f;
    float2 noise_dir = SafeNormalize(float2(
        FBM(domain * 0.22f + drift * 8.0f + float2(13.0f, -7.0f)) - 0.5f,
        FBM(domain * 0.22f - drift * 7.0f + float2(-11.0f, 17.0f)) - 0.5f));

    TerrainFlowState terrain_flow = ComputeTerrainFlow(uv, wind_dir);
    float wind_against_slope = saturate(dot(wind_dir, -terrain_flow.slope_dir));
    float wind_along_channel = saturate(abs(dot(wind_dir, terrain_flow.contour_dir)));
    float2 terrain_guidance =
        terrain_flow.contour_dir * terrain_flow.channel_mask * (0.20f + g_WindCrossInfluence * 0.16f) +
        terrain_flow.slope_dir * (terrain_flow.ridge_mask * 0.05f - terrain_flow.valley_mask * 0.08f);

    const float jet_band_north = exp(-pow((uv.y - 0.22f) / 0.14f, 2.0f));
    const float jet_band_mid = exp(-pow((uv.y - 0.52f) / 0.20f, 2.0f));
    const float jet_band_south = exp(-pow((uv.y - 0.80f) / 0.16f, 2.0f));
    const float2 bias_flow =
        wind_dir * (0.42f + jet_band_north * 0.26f + jet_band_mid * 0.10f - jet_band_south * 0.05f) +
        cross_dir * ((jet_band_north - jet_band_south) * 0.06f + (jet_band_mid - 0.35f) * 0.02f);

    float2 source_dir = SafeNormalize(
        lerp(wind_dir, noise_dir, 0.30f + storminess * 0.32f) +
        bias_flow * 0.22f +
        terrain_guidance * g_TerrainGuidanceScale);
    float source_strength = saturate(
        g_WindStrength *
        (0.72f + storminess * g_StormCoupling + climate_center.x * 0.10f) +
        terrain_flow.ridge_mask * 0.06f -
        terrain_flow.valley_mask * 0.04f);

    if (g_InitializeState != 0u)
    {
        g_Output[dispatch_thread_id.xy] = EncodeWindSample(source_dir, source_strength, storminess);
        return;
    }

    const float3 previous_center = DecodeWindSample(g_PreviousWindField.Load(int3(dispatch_thread_id.xy, 0)));
    const float3 previous_right = DecodeWindSample(g_PreviousWindField.Load(int3(clamp(int2(dispatch_thread_id.xy) + int2(1, 0), int2(0, 0), int2(int(g_Width) - 1, int(g_Height) - 1)), 0)));
    const float3 previous_left = DecodeWindSample(g_PreviousWindField.Load(int3(clamp(int2(dispatch_thread_id.xy) + int2(-1, 0), int2(0, 0), int2(int(g_Width) - 1, int(g_Height) - 1)), 0)));
    const float3 previous_up = DecodeWindSample(g_PreviousWindField.Load(int3(clamp(int2(dispatch_thread_id.xy) + int2(0, 1), int2(0, 0), int2(int(g_Width) - 1, int(g_Height) - 1)), 0)));
    const float3 previous_down = DecodeWindSample(g_PreviousWindField.Load(int3(clamp(int2(dispatch_thread_id.xy) + int2(0, -1), int2(0, 0), int2(int(g_Width) - 1, int(g_Height) - 1)), 0)));

    float2 previous_wind = previous_center.xy * previous_center.z;
    float2 wind_right = previous_right.xy * previous_right.z;
    float2 wind_left = previous_left.xy * previous_left.z;
    float2 wind_up = previous_up.xy * previous_up.z;
    float2 wind_down = previous_down.xy * previous_down.z;

    float2 neighbor_transport = float2(
        (-wind_right.x + wind_left.x),
        (-wind_up.y + wind_down.y));
    float2 pressure_wind = clamp(
        float2(
            climate_right.x - climate_left.x,
            climate_up.x - climate_down.x) * (-g_PressureWindScale),
        -g_PressureWindScale.xx,
        g_PressureWindScale.xx);
    float2 humidity_pull =
        float2(
            climate_right.y - climate_left.y,
            climate_up.y - climate_down.y) *
        (0.018f + climate_center.z * 0.026f);

    previous_wind = lerp(
        previous_wind,
        source_dir * source_strength,
        saturate(wind_dt * g_SourceBlendRate * (0.65f + storminess * 0.55f)));
    previous_wind +=
        (neighbor_transport * g_PropagationScale +
         pressure_wind +
         humidity_pull +
         terrain_guidance * (g_TerrainGuidanceScale * (0.55f + wind_along_channel * 0.45f))) * wind_dt;

    const float max_strength =
        saturate(0.30f + g_WindStrength * 1.45f + storminess * 0.24f + wind_against_slope * 0.06f);
    float strength = min(length(previous_wind), max(max_strength, 0.02f));
    float2 local_dir = strength > 1.0e-5f ? previous_wind / strength : source_dir;
    strength = saturate(strength);

    g_Output[dispatch_thread_id.xy] = EncodeWindSample(local_dir, strength, storminess);
}
