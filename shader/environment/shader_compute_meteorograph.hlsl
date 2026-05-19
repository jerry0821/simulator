Texture2D<float4> g_PreviousMeteorograph : register(t0);
Texture2D g_TerrainHeight : register(t1);
SamplerState g_LinearSampler : register(s0);
RWTexture2D<float4> g_Output : register(u0);
RWTexture2D<float4> g_RainOut : register(u1);

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
    float g_HumidityAdvection;
    float g_TemperatureRelax;
    float g_RainCoupling;
    float g_RainMultiplier;
    float g_ForceRain;
    float g_HumidityCapacity;
    uint g_Width;
    uint g_Height;
    uint g_InitializeState;
    uint g_Padding0;
};

static const float kWindSourceIntensity = 1.8f;
static const float kHumidityDiffuseWeight = 0.01f;
static const float kRainFadeoff = 0.55f;

float2 SafeNormalize(float2 value)
{
    const float len_sq = dot(value, value);
    return len_sq > 1.0e-6f ? value * rsqrt(len_sq) : float2(1.0f, 0.0f);
}

int2 WrapCoord(int2 coord)
{
    const int width = int(g_Width);
    const int height = int(g_Height);
    coord.x = ((coord.x % width) + width) % width;
    coord.y = ((coord.y % height) + height) % height;
    return coord;
}

float2 WrapUv(float2 uv)
{
    return frac(uv + 1024.0f);
}

float4 SamplePreviousState(int2 coord)
{
    return g_PreviousMeteorograph.Load(int3(WrapCoord(coord), 0));
}

float4 SampleTerrainState(float2 uv)
{
    return g_TerrainHeight.SampleLevel(g_LinearSampler, WrapUv(uv), 0.0f);
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

float ComputeSurfaceTemperature(float2 uv, float terrain_height, float water_depth)
{
    const float latitude = abs(uv.y * 2.0f - 1.0f);
    const float elevation_cooling = saturate(max(terrain_height, 0.0f) * 0.0125f);
    const float marine_moderation = saturate(water_depth * 0.18f);
    const float diurnal =
        sin(g_TimeSeconds * 0.026f + uv.x * 6.2831853f + uv.y * 2.6f) * 0.035f;

    return saturate(
        0.72f -
        latitude * 0.24f -
        elevation_cooling * 0.18f +
        marine_moderation * 0.05f +
        diurnal);
}

float ComputeEvaporationSource(float2 wind_velocity, float humidity, float water_depth)
{
    const float wind_strength = saturate(length(wind_velocity));
    const float exposed_water = saturate(water_depth * 0.95f);
    const float dry_air = 1.0f - saturate(humidity);
    return dry_air * exposed_water * (0.045f + wind_strength * 0.065f) * g_RainCoupling;
}

float4 BuildRainState(float rain_amount, float humidity, float water_depth)
{
    const float wet_hint =
        saturate(rain_amount * 0.68f + humidity * 0.22f + saturate(water_depth) * 0.24f);
    return float4(saturate(rain_amount), wet_hint, saturate(humidity), 1.0f);
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
    const float dt = max(min(g_DeltaTimeSeconds, 1.0f / 60.0f), 1.0e-4f);

    const float2 prevailing_wind = SafeNormalize(float2(g_WindDirectionX, g_WindDirectionY));
    const float2 cross_dir = float2(-prevailing_wind.y, prevailing_wind.x);
    const float4 terrain_state = SampleTerrainState(uv);
    const float terrain_height = terrain_state.x;
    const float water_surface = max(terrain_state.y, terrain_height);
    const float water_depth = max(water_surface - terrain_height, 0.0f);
    const float target_temperature = ComputeSurfaceTemperature(uv, terrain_height, water_depth);

    const float2 domain = uv * max(g_NoiseScale, 1.0f);
    const float2 drift =
        prevailing_wind * g_TimeSeconds * 0.018f +
        cross_dir * sin(g_TimeSeconds * 0.0018f) * 0.010f;
    const float2 random_wind_source = SafeNormalize(float2(
        FBM(domain * 0.55f + drift * 3.1f + float2(7.1f, -4.2f)) - 0.5f,
        FBM(domain * 0.55f + drift.yx * float2(-2.3f, 2.0f) + float2(-5.4f, 9.7f)) - 0.5f));
    const float2 source_dir = SafeNormalize(
        lerp(prevailing_wind, random_wind_source, 0.10f + g_WindCrossInfluence * 0.12f));
    const float source_strength =
        max(g_WindStrength, 1.0e-4f) * (0.95f + water_depth * 0.05f);

    if (g_InitializeState != 0u)
    {
        const float humidity_seed = saturate(
            0.38f +
            FBM(domain * 0.40f + float2(13.7f, -8.1f)) * 0.26f +
            water_depth * 0.18f);
        g_Output[coord] = float4(source_dir * source_strength, humidity_seed, target_temperature);
        g_RainOut[coord] = BuildRainState(0.0f, humidity_seed, water_depth);
        return;
    }

    const float4 previous_center = SamplePreviousState(coord);
    const float4 previous_right = SamplePreviousState(coord + int2(1, 0));
    const float4 previous_left = SamplePreviousState(coord + int2(-1, 0));
    const float4 previous_up = SamplePreviousState(coord + int2(0, 1));
    const float4 previous_down = SamplePreviousState(coord + int2(0, -1));

    const float2 neighbor_transport = float2(
        -previous_right.x + previous_left.x,
        -previous_up.y + previous_down.y);
    const float2 pressure_wind = clamp(
        float2(
            (previous_right.w - previous_center.w) + (previous_left.w - previous_center.w),
            (previous_up.w - previous_center.w) + (previous_down.w - previous_center.w)) * (-g_PressureScale),
        -g_PressureScale.xx,
        g_PressureScale.xx);

    float2 wind_velocity = lerp(
        previous_center.xy,
        source_dir * max(source_strength, kWindSourceIntensity * g_WindStrength),
        saturate(dt * g_SourceBlendRate));
    wind_velocity += (neighbor_transport * g_PropagationScale + pressure_wind) * dt;

    const float max_strength = max(g_WindStrength * 1.65f + 0.04f, 1.0e-4f);
    const float wind_length = length(wind_velocity);
    if (wind_length > max_strength)
    {
        wind_velocity *= max_strength / wind_length;
    }

    const float2 humiture_flow_t = -previous_up.y * previous_up.zw;
    const float2 humiture_flow_b = previous_down.y * previous_down.zw;
    const float2 humiture_flow_r = -previous_right.x * previous_right.zw;
    const float2 humiture_flow_l = previous_left.x * previous_left.zw;
    float2 humiture = max(
        previous_center.zw + (humiture_flow_t + humiture_flow_b + humiture_flow_r + humiture_flow_l) * dt,
        float2(0.0f, 0.0f));

    const float diffuse_weight = min(max(g_HumidityAdvection, kHumidityDiffuseWeight) * dt, 0.24f);
    const float concentrated_weight = max(1.0f - diffuse_weight * 4.0f, 0.0f);
    humiture =
        humiture * concentrated_weight +
        (previous_up.zw + previous_down.zw + previous_right.zw + previous_left.zw) * diffuse_weight;

    float humidity = max(humiture.x, 0.0f);
    humidity += ComputeEvaporationSource(wind_velocity, humidity, water_depth) * dt;

    float rain_amount = max(humidity - g_HumidityCapacity, 0.0f) * kRainFadeoff * dt;
    rain_amount *= max(g_RainMultiplier, 0.0f);
    rain_amount = max(rain_amount, saturate(g_ForceRain * humidity) * 0.12f);
    rain_amount = saturate(rain_amount);
    humidity = saturate(humidity - rain_amount);

    float temperature = lerp(humiture.y, target_temperature, saturate(dt * g_TemperatureRelax));
    temperature = lerp(
        temperature,
        (previous_up.w + previous_down.w + previous_right.w + previous_left.w) * 0.25f,
        saturate(dt * 0.05f));
    temperature = saturate(temperature);

    g_Output[coord] = float4(wind_velocity, humidity, temperature);
    g_RainOut[coord] = BuildRainState(rain_amount, humidity, water_depth);
}
