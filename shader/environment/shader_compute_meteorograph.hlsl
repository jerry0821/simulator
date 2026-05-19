Texture2D<float4> g_PreviousMeteorograph : register(t0);
Texture2D<float3> g_ClimateField : register(t1);
Texture2D g_TerrainHeight : register(t2);
SamplerState g_LinearSampler : register(s0);
RWTexture2D<float4> g_Output : register(u0);

cbuffer CS_METEOROGRAPH : register(b0)
{
    float g_TimeSeconds;
    float g_DeltaTimeSeconds;
    float g_WindDirectionX;
    float g_WindDirectionY;
    float g_WindStrength;
    float g_WindCrossInfluence;
    float g_NoiseScale;
    float g_PressureScale;
    float g_SourceBlendRate;
    float g_PropagationScale;
    float g_TerrainGuidanceScale;
    float g_StormCoupling;
    float g_HumidityAdvection;
    float g_TemperatureRelax;
    float g_RainCoupling;
    float g_Padding0;
    uint g_Width;
    uint g_Height;
    uint g_InitializeState;
    uint g_Padding1;
};

float2 SafeNormalize(float2 value)
{
    const float len_sq = dot(value, value);
    return len_sq > 1.0e-6f ? value * rsqrt(len_sq) : float2(1.0f, 0.0f);
}

int2 ClampCoord(int2 coord)
{
    return clamp(coord, int2(0, 0), int2(int(g_Width) - 1, int(g_Height) - 1));
}

float4 SamplePreviousState(int2 coord)
{
    return g_PreviousMeteorograph.Load(int3(ClampCoord(coord), 0));
}

float3 SampleClimate(float2 uv)
{
    return g_ClimateField.SampleLevel(g_LinearSampler, saturate(uv), 0.0f);
}

float SampleTerrainHeight(float2 uv)
{
    return g_TerrainHeight.SampleLevel(g_LinearSampler, saturate(uv), 0.0f).r;
}

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 345.45f));
    p += dot(p, p + 34.345f);
    return frac(p.x * p.y);
}

float ValueNoise(float2 p)
{
    const float2 cell = floor(p);
    const float2 local = frac(p);
    const float2 smooth = local * local * (3.0f - 2.0f * local);

    const float v00 = Hash21(cell + float2(0.0f, 0.0f));
    const float v10 = Hash21(cell + float2(1.0f, 0.0f));
    const float v01 = Hash21(cell + float2(0.0f, 1.0f));
    const float v11 = Hash21(cell + float2(1.0f, 1.0f));

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
    const float2 texel = float2(
        1.0f / max(float(terrain_width), 1.0f),
        1.0f / max(float(terrain_height), 1.0f));

    const float height_center = SampleTerrainHeight(uv);
    const float height_x_pos = SampleTerrainHeight(uv + float2(texel.x, 0.0f));
    const float height_x_neg = SampleTerrainHeight(uv - float2(texel.x, 0.0f));
    const float height_y_pos = SampleTerrainHeight(uv + float2(0.0f, texel.y));
    const float height_y_neg = SampleTerrainHeight(uv - float2(0.0f, texel.y));

    const float2 height_gradient = float2(
        height_x_pos - height_x_neg,
        height_y_pos - height_y_neg);
    const float relief =
        max(max(height_x_pos, height_x_neg), max(height_y_pos, height_y_neg)) -
        min(min(height_x_pos, height_x_neg), min(height_y_pos, height_y_neg));
    const float laplacian =
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

    const int2 coord = int2(dispatch_thread_id.xy);
    const float2 uv = (dispatch_thread_id.xy + 0.5f) / float2(g_Width, g_Height);
    const float2 texel = 1.0f / float2(g_Width, g_Height);
    const float dt = max(min(g_DeltaTimeSeconds, 1.0f / 30.0f), 1.0e-4f);

    const float2 prevailing_wind = SafeNormalize(float2(g_WindDirectionX, g_WindDirectionY));
    const float2 cross_dir = float2(-prevailing_wind.y, prevailing_wind.x);
    const float3 climate_center = SampleClimate(uv);
    const float3 climate_right = SampleClimate(uv + float2(texel.x, 0.0f));
    const float3 climate_left = SampleClimate(uv - float2(texel.x, 0.0f));
    const float3 climate_up = SampleClimate(uv + float2(0.0f, texel.y));
    const float3 climate_down = SampleClimate(uv - float2(0.0f, texel.y));
    const float storminess = saturate(climate_center.y * (0.62f + climate_center.z * 0.38f));

    const float2 domain = uv * max(g_NoiseScale, 1.0f);
    const float2 drift =
        prevailing_wind * g_TimeSeconds * 0.0055f +
        cross_dir * sin(g_TimeSeconds * 0.0035f) * 0.020f;
    const float2 noise_dir = SafeNormalize(float2(
        FBM(domain * 0.22f + drift * 8.0f + float2(13.0f, -7.0f)) - 0.5f,
        FBM(domain * 0.22f - drift * 7.0f + float2(-11.0f, 17.0f)) - 0.5f));

    const TerrainFlowState terrain_flow = ComputeTerrainFlow(uv, prevailing_wind);
    const float wind_against_slope = saturate(dot(prevailing_wind, -terrain_flow.slope_dir));
    const float wind_along_channel = saturate(abs(dot(prevailing_wind, terrain_flow.contour_dir)));
    const float2 terrain_guidance =
        terrain_flow.contour_dir * terrain_flow.channel_mask * (0.20f + g_WindCrossInfluence * 0.16f) +
        terrain_flow.slope_dir * (terrain_flow.ridge_mask * 0.05f - terrain_flow.valley_mask * 0.08f);

    const float jet_band_north = exp(-pow((uv.y - 0.22f) / 0.14f, 2.0f));
    const float jet_band_mid = exp(-pow((uv.y - 0.52f) / 0.20f, 2.0f));
    const float jet_band_south = exp(-pow((uv.y - 0.80f) / 0.16f, 2.0f));
    const float2 bias_flow =
        prevailing_wind * (0.42f + jet_band_north * 0.26f + jet_band_mid * 0.10f - jet_band_south * 0.05f) +
        cross_dir * ((jet_band_north - jet_band_south) * 0.06f + (jet_band_mid - 0.35f) * 0.02f);

    const float2 source_dir = SafeNormalize(
        lerp(prevailing_wind, noise_dir, 0.30f + storminess * 0.32f) +
        bias_flow * 0.22f +
        terrain_guidance * g_TerrainGuidanceScale);
    const float source_strength = saturate(
        g_WindStrength *
        (0.72f + storminess * g_StormCoupling + climate_center.x * 0.10f) +
        terrain_flow.ridge_mask * 0.06f -
        terrain_flow.valley_mask * 0.04f);

    const float climate_humidity =
        saturate(climate_center.y * (0.72f + climate_center.z * g_RainCoupling));
    const float climate_temperature =
        saturate(climate_center.x - climate_center.z * 0.10f + source_strength * 0.03f);

    if (g_InitializeState != 0u)
    {
        g_Output[coord] = float4(source_dir * source_strength, climate_humidity, climate_temperature);
        return;
    }

    const float4 previous_center = SamplePreviousState(coord);
    const float4 previous_right = SamplePreviousState(coord + int2(1, 0));
    const float4 previous_left = SamplePreviousState(coord + int2(-1, 0));
    const float4 previous_up = SamplePreviousState(coord + int2(0, 1));
    const float4 previous_down = SamplePreviousState(coord + int2(0, -1));

    float2 wind_velocity = previous_center.xy;
    const float2 neighbor_wind =
        (previous_right.xy + previous_left.xy + previous_up.xy + previous_down.xy) * 0.25f;
    const float2 neighbor_transport = float2(
        -previous_right.x + previous_left.x,
        -previous_up.y + previous_down.y);
    const float2 pressure_wind = clamp(
        float2(
            climate_right.x - climate_left.x,
            climate_up.x - climate_down.x) * (-g_PressureScale),
        -g_PressureScale.xx,
        g_PressureScale.xx);
    const float2 humidity_push =
        float2(
            climate_right.y - climate_left.y,
            climate_up.y - climate_down.y) *
        (0.030f + climate_center.z * 0.042f);

    wind_velocity = lerp(wind_velocity, neighbor_wind, saturate(dt * g_PropagationScale * 0.55f));
    wind_velocity = lerp(
        wind_velocity,
        source_dir * source_strength,
        saturate(dt * g_SourceBlendRate * (0.75f + storminess * 0.45f)));
    wind_velocity +=
        (neighbor_transport * g_PropagationScale +
         pressure_wind +
         humidity_push +
         terrain_guidance * (g_TerrainGuidanceScale * (0.55f + wind_along_channel * 0.45f))) * dt;

    const float max_strength =
        saturate(0.18f + g_WindStrength * 1.32f + storminess * 0.22f + wind_against_slope * 0.05f);
    const float wind_length = length(wind_velocity);
    if (wind_length > max(max_strength, 1.0e-4f))
    {
        wind_velocity *= max_strength / wind_length;
    }

    const float humidity_neighbors =
        (previous_right.z + previous_left.z + previous_up.z + previous_down.z) * 0.25f;
    float humidity = lerp(
        previous_center.z,
        climate_humidity,
        saturate(dt * (0.34f + climate_center.z * 0.28f)));
    humidity = lerp(humidity, humidity_neighbors, saturate(dt * g_HumidityAdvection));
    humidity = saturate(
        humidity +
        climate_center.z * dt * 0.14f -
        climate_center.x * dt * 0.04f +
        saturate(length(wind_velocity)) * dt * 0.03f);

    const float temperature_neighbors =
        (previous_right.w + previous_left.w + previous_up.w + previous_down.w) * 0.25f;
    float temperature = lerp(
        previous_center.w,
        climate_temperature,
        saturate(dt * g_TemperatureRelax));
    temperature = lerp(temperature, temperature_neighbors, saturate(dt * 0.12f));
    temperature = saturate(temperature - climate_center.z * dt * 0.05f);

    g_Output[coord] = float4(wind_velocity, humidity, temperature);
}
