cbuffer TERRAIN_CLASSIFICATION_CONSTANT_BUFFER : register(b0)
{
    float grass_slope_min;
    float grass_slope_max;
    float grass_noise_strength;
    float grass_height_start;
    float grass_height_end;
    float rock_slope_start;
    float rock_slope_end;
    float rock_height_start;
    float rock_height_end;
    float stone_noise_scale;
    float shoreline_offset_start;
    float shoreline_offset_end;
    float lowland_height_start;
    float lowland_height_end;
    float grass_coverage_min;
    float water_height;
    float field_width;
    float field_depth;
    float sample_offset;
    float wetness_gain;
    uint width;
    uint height;
    uint padding0;
    uint padding1;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D g_TerrainNormal : register(t1);
Texture2D g_WaterInteractionData : register(t2);
Texture2D g_ErosionDelta : register(t3);
Texture2D<float4> g_Meteorograph : register(t4);
SamplerState g_ClassificationSampler : register(s0);
RWTexture2D<float> g_TerrainVegetationSuitability : register(u0);

static const float kPolarTemperature = -10.0f;
static const float kEquatorialTemperature = 30.0f;

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 345.45f));
    p += dot(p, p + 34.345f);
    return frac(p.x * p.y);
}

float ValueNoise(float2 p)
{
    float2 cell = floor(p);
    float2 local = frac(p);
    float2 smooth = local * local * (3.0f - 2.0f * local);

    float v00 = Hash21(cell);
    float v10 = Hash21(cell + float2(1.0f, 0.0f));
    float v01 = Hash21(cell + float2(0.0f, 1.0f));
    float v11 = Hash21(cell + float2(1.0f, 1.0f));

    float vx0 = lerp(v00, v10, smooth.x);
    float vx1 = lerp(v01, v11, smooth.x);
    return lerp(vx0, vx1, smooth.y);
}

float Fbm(float2 p)
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

float2 WorldToUv(float2 world_xz)
{
    return float2(
        saturate((world_xz.x + field_width * 0.5f) / field_width),
        saturate((world_xz.y + field_depth * 0.5f) / field_depth));
}

float TerrainNormalYFromPacked(float2 encoded)
{
    const float2 xz = clamp(encoded, -1.0f.xx, 1.0f.xx);
    return sqrt(saturate(1.0f - dot(xz, xz)));
}

float SampleTerrainHeight(float2 world_xz)
{
    return g_TerrainHeight.SampleLevel(g_ClassificationSampler, WorldToUv(world_xz), 0.0f).r;
}

float4 SampleWaterInteraction(float2 world_xz)
{
    return g_WaterInteractionData.SampleLevel(g_ClassificationSampler, WorldToUv(world_xz), 0.0f);
}

float4 SampleErosionDelta(float2 world_xz)
{
    return g_ErosionDelta.SampleLevel(g_ClassificationSampler, WorldToUv(world_xz), 0.0f);
}

float4 SampleMeteorograph(float2 world_xz)
{
    return g_Meteorograph.SampleLevel(g_ClassificationSampler, WorldToUv(world_xz), 0.0f);
}

float NormalizeClimateTemperature(float temperature_celsius)
{
    return saturate((temperature_celsius - kPolarTemperature) / max(kEquatorialTemperature - kPolarTemperature, 1.0e-4f));
}

float SampleNormalY(float2 world_xz)
{
    float4 normal_sample = g_TerrainNormal.SampleLevel(g_ClassificationSampler, WorldToUv(world_xz), 0.0f);
    return saturate(TerrainNormalYFromPacked(normal_sample.xy));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    float2 uv = (float2(dispatch_thread_id.xy) + 0.5f) / float2(width, height);
    float2 world_xz = float2(
        uv.x * field_width - field_width * 0.5f,
        uv.y * field_depth - field_depth * 0.5f);

    float terrain_height = SampleTerrainHeight(world_xz);
    float4 water_interaction = SampleWaterInteraction(world_xz);
    float4 erosion_delta = SampleErosionDelta(world_xz);
    float4 meteorograph = SampleMeteorograph(world_xz);
    float normal_y = SampleNormalY(world_xz);
    float surface_wetness = water_interaction.r;
    float shoreline_wetness = water_interaction.g;
    float pooled_wetness = water_interaction.b;
    float retained_moisture = water_interaction.a;

    float grass_flatness = saturate((normal_y - grass_slope_min) / max(grass_slope_max - grass_slope_min, 1.0e-5f));
    float lowland = 1.0f - smoothstep(lowland_height_end, lowland_height_end + 30.0f, terrain_height);
    float above_water = smoothstep(
        water_height + max(shoreline_offset_start * 0.18f, 0.08f),
        water_height + max(shoreline_offset_start + 0.95f, 1.15f),
        terrain_height);

    float macro_noise = Fbm(world_xz * 0.0125f + float2(37.0f, -21.0f));
    float patch_noise = ValueNoise(world_xz * 0.050f + float2(-11.0f, 17.0f));
    float detail_noise = ValueNoise(world_xz * 0.110f + float2(23.0f, -31.0f));
    float macro_mask = smoothstep(0.40f, 0.72f, macro_noise);
    float patch_mask = smoothstep(
        0.44f,
        0.72f,
        patch_noise * 0.7f + detail_noise * 0.3f + (macro_noise - 0.5f) * grass_noise_strength);

    float erosion_mask = saturate(erosion_delta.r * 1.85f + max(-erosion_delta.b, 0.0f) * 0.65f);
    float vegetation_noise = smoothstep(0.22f, 0.70f, macro_noise * 0.65f + patch_noise * 0.35f);
    float climate_humidity = saturate(meteorograph.z);
    float climate_temperature = NormalizeClimateTemperature(meteorograph.w);
    float climate_mildness = saturate(1.0f - abs(climate_temperature - 0.52f) * 1.35f);
    float moisture_support = lerp(
        0.92f,
        1.0f,
        saturate(retained_moisture * 0.82f + climate_humidity * 0.18f));
    float vegetation_suitability =
        grass_flatness *
        above_water *
        lerp(0.72f, 1.0f, lowland) *
        lerp(0.42f, 1.0f, macro_mask) *
        lerp(0.68f, 1.0f, vegetation_noise) *
        moisture_support *
        lerp(1.0f, 0.82f, max(surface_wetness, pooled_wetness * 0.92f)) *
        lerp(1.0f, 0.82f, 1.0f - climate_mildness) *
        lerp(1.0f, 0.96f, erosion_mask);
    vegetation_suitability = saturate(vegetation_suitability);
    g_TerrainVegetationSuitability[dispatch_thread_id.xy] = vegetation_suitability;
}
