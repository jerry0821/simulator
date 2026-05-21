Texture2D<float4> g_PreviousMeteorograph : register(t0);
Texture2D g_TerrainHeight : register(t1);
SamplerState g_LinearSampler : register(s0);
RWTexture2D<float4> g_Output : register(u0);
RWTexture2D<float> g_RainOut : register(u1);
RWTexture2D<float4> g_PreviewOut : register(u2);

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
static const float kAbsoluteZeroTemperature = -273.15f;
static const float kPolarTemperature = -10.0f;
static const float kEquatorialTemperature = 30.0f;
static const float kEnvironmentalLapseRate = -0.325f;
static const float kEquatorialEvaporationRate = 1.2e-4f;
static const float kBaseAirConvection = 0.2f;
static const float kSoilEvaporationFactor = 0.2f;
static const float kInvMaxWaterSoilDiffuseDistance = 1.0f;

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

float NormalizePreviewTemperature(float temperature_celsius)
{
    return saturate((temperature_celsius - kPolarTemperature) / max(kEquatorialTemperature - kPolarTemperature, 1.0e-4f));
}

float ComputeSurfaceTemperature(float2 uv, float terrain_height)
{
    const float latitude = abs(uv.y * 2.0f - 1.0f);
    const float sunlight_ratio = saturate(1.0f - latitude);
    const float sea_level_temperature = lerp(kPolarTemperature, kEquatorialTemperature, sunlight_ratio);
    return max(sea_level_temperature + terrain_height * kEnvironmentalLapseRate, kAbsoluteZeroTemperature);
}

float SoilMoisture(float delta_water_height)
{
    return kSoilEvaporationFactor -
        clamp(-delta_water_height * (kInvMaxWaterSoilDiffuseDistance * kSoilEvaporationFactor), 0.0f, kSoilEvaporationFactor);
}

float ComputeEvaporationSource(float2 wind_velocity, float humidity, float temperature_celsius, float soil_moisture)
{
    const float wind_speed = length(wind_velocity);
    const float clamped_temperature = clamp(temperature_celsius, 0.0f, 100.0f);
    const float vapor_pressure =
        610.78f * exp((17.27f * clamped_temperature) / (clamped_temperature + 237.3f));
    const float vapor_pressure_deficit = max(vapor_pressure - humidity * vapor_pressure, 0.0f);
    return
        kEquatorialEvaporationRate *
        max(wind_speed, kBaseAirConvection) *
        vapor_pressure_deficit *
        soil_moisture *
        g_RainCoupling;
}

float4 BuildAtmospherePreview(float rain_amount, float humidity, float temperature_celsius)
{
    const float temperature_preview = NormalizePreviewTemperature(temperature_celsius);
    const float humidity_preview = saturate(humidity);
    const float rainfall_preview = saturate(rain_amount * 28.0f);

    float3 preview_color = lerp(
        float3(0.28f, 0.20f, 0.10f),
        float3(0.94f, 0.36f, 0.10f),
        temperature_preview);
    preview_color = lerp(
        preview_color,
        float3(0.76f, 0.96f, 0.26f),
        sqrt(humidity_preview) * 0.42f);
    preview_color = lerp(
        preview_color,
        1.0f.xxx,
        rainfall_preview);

    return float4(preview_color, saturate(rain_amount));
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
    const float target_temperature = ComputeSurfaceTemperature(uv, terrain_height);

    const float2 domain = uv * max(g_NoiseScale, 1.0f);
    const float2 drift =
        prevailing_wind * g_TimeSeconds * 0.018f +
        cross_dir * sin(g_TimeSeconds * 0.0018f) * 0.010f;
    const float2 random_wind_source = SafeNormalize(float2(
        FBM(domain * 0.55f + drift * 3.1f + float2(7.1f, -4.2f)) - 0.5f,
        FBM(domain * 0.55f + drift.yx * float2(-2.3f, 2.0f) + float2(-5.4f, 9.7f)) - 0.5f));
    const float2 source_dir = SafeNormalize(
        lerp(prevailing_wind, random_wind_source, 0.03f + g_WindCrossInfluence * 0.04f));
    const float source_strength =
        max(g_WindStrength, 1.0e-4f) * (0.98f + water_depth * 0.02f);

    if (g_InitializeState != 0u)
    {
        const float humidity_seed = saturate(
            0.38f +
            FBM(domain * 0.40f + float2(13.7f, -8.1f)) * 0.26f +
            water_depth * 0.18f);
        g_Output[coord] = float4(source_dir * source_strength, humidity_seed, target_temperature);
        g_RainOut[coord] = 0.0f;
        g_PreviewOut[coord] = BuildAtmospherePreview(0.0f, humidity_seed, target_temperature);
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

    const float divergence =
        ((previous_right.x - previous_left.x) + (previous_up.y - previous_down.y)) * 0.5f;
    const float convergence =
        saturate(-divergence * (2.0f + g_PropagationScale * 2.0f));

    const float2 humiture_flow_t = -previous_up.y * previous_up.zw;
    const float2 humiture_flow_b = previous_down.y * previous_down.zw;
    const float2 humiture_flow_r = -previous_right.x * previous_right.zw;
    const float2 humiture_flow_l = previous_left.x * previous_left.zw;
    float2 humiture = max(
        previous_center.zw + (humiture_flow_t + humiture_flow_b + humiture_flow_r + humiture_flow_l) * dt,
        float2(0.0f, kAbsoluteZeroTemperature));

    const float diffuse_weight = min(max(g_HumidityAdvection, kHumidityDiffuseWeight) * dt, 0.24f);
    const float concentrated_weight = max(1.0f - diffuse_weight * 4.0f, 0.0f);
    humiture =
        humiture * concentrated_weight +
        (previous_up.zw + previous_down.zw + previous_right.zw + previous_left.zw) * diffuse_weight;

    float humidity = max(humiture.x, 0.0f);
    const float soil_moisture = SoilMoisture(water_depth);
    humidity += ComputeEvaporationSource(wind_velocity, humidity, humiture.y, soil_moisture) * dt;
    humidity += convergence * (0.06f + humidity * 0.18f) * dt;

    float rain_amount = max(humidity - g_HumidityCapacity, 0.0f) * kRainFadeoff * dt;
    humidity = saturate(humidity - rain_amount);

    float temperature = lerp(humiture.y, target_temperature, saturate(dt * g_TemperatureRelax));
    temperature = lerp(
        temperature,
        (previous_up.w + previous_down.w + previous_right.w + previous_left.w) * 0.25f,
        saturate(dt * 0.05f));
    temperature -= convergence * 0.8f * dt;
    temperature = max(temperature, kAbsoluteZeroTemperature);

    g_Output[coord] = float4(wind_velocity, humidity, temperature);
    g_RainOut[coord] = saturate(rain_amount);
    g_PreviewOut[coord] = BuildAtmospherePreview(rain_amount, humidity, temperature);
}
