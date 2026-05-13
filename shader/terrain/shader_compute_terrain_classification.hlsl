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
Texture2D g_ClimateField : register(t4);
SamplerState g_ClassificationSampler : register(s0);
RWTexture2D<float4> g_TerrainSurfaceData : register(u0);
RWTexture2D<float> g_TerrainVegetationSuitability : register(u1);

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

float3 SampleClimate(float2 world_xz)
{
    return g_ClimateField.SampleLevel(g_ClassificationSampler, WorldToUv(world_xz), 0.0f).rgb;
}

float SampleNormalY(float2 world_xz)
{
    float4 normal_sample = g_TerrainNormal.SampleLevel(g_ClassificationSampler, WorldToUv(world_xz), 0.0f);
    return saturate(normal_sample.w);
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
    float3 climate = SampleClimate(world_xz);
    float normal_y = SampleNormalY(world_xz);
    float surface_wetness = water_interaction.r;
    float shoreline_wetness = water_interaction.g;
    float pooled_wetness = water_interaction.b;
    float retained_moisture = water_interaction.a;

    float grass_flatness = saturate((normal_y - grass_slope_min) / max(grass_slope_max - grass_slope_min, 1.0e-5f));
    float lowland = 1.0f - smoothstep(lowland_height_end, lowland_height_end + 30.0f, terrain_height);
    float shoreline = smoothstep(
        water_height + shoreline_offset_start,
        water_height + shoreline_offset_end,
        terrain_height);
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

    float flood_penalty = 1.0f - smoothstep(0.76f, 0.98f, max(surface_wetness, pooled_wetness * 0.92f));
    float erosion_mask = saturate(erosion_delta.r * 1.85f + max(-erosion_delta.b, 0.0f) * 0.65f);
    float wetness = saturate(
        max(shoreline * (0.60f + shoreline_wetness * 0.40f), retained_moisture * 0.82f) * wetness_gain +
        surface_wetness * 0.24f +
        pooled_wetness * 0.18f +
        climate.g * 0.10f);
    float surface_grass_coverage =
        grass_flatness *
        shoreline *
        lerp(0.72f, 1.0f, lowland) *
        lerp(0.42f, 1.0f, macro_mask) *
        patch_mask *
        flood_penalty *
        lerp(0.88f, 1.12f, retained_moisture) *
        lerp(1.0f, 0.78f, erosion_mask);
    float vegetation_noise = smoothstep(0.22f, 0.70f, macro_noise * 0.65f + patch_noise * 0.35f);
    float moisture_support = lerp(0.92f, 1.0f, saturate(retained_moisture * 0.82f + climate.g * 0.18f));
    float vegetation_suitability =
        grass_flatness *
        above_water *
        lerp(0.68f, 1.0f, vegetation_noise) *
        moisture_support *
        lerp(1.0f, 0.96f, erosion_mask);
    vegetation_suitability = saturate(vegetation_suitability);
    surface_grass_coverage = saturate(surface_grass_coverage);

    float cliff_slope_mask = 1.0f - smoothstep(rock_slope_start, rock_slope_end, normal_y);
    float cliff_height_mask = smoothstep(rock_height_start, rock_height_end, terrain_height);
    float rock_mask = saturate(cliff_slope_mask * 1.15f + cliff_height_mask * 0.30f);
    rock_mask *= lerp(1.06f, 0.88f, climate.g);
    rock_mask = saturate(pow(rock_mask, 1.8f));

    float slope_amount = saturate(1.0f - normal_y);
    float surface_slope = smoothstep(1.0f - rock_slope_end, 1.0f - rock_slope_start, slope_amount);
    float beach_mask = saturate(max(shoreline_wetness, shoreline * (1.0f - pooled_wetness * 0.25f)));
    float humidity = saturate(retained_moisture * 0.72f + wetness * 0.18f + climate.g * 0.10f);
    float roughness = saturate(lerp(0.24f, 0.92f, max(erosion_mask, surface_slope * 0.65f)));

    g_TerrainSurfaceData[dispatch_thread_id.xy] = float4(surface_slope, beach_mask, humidity, roughness);
    g_TerrainVegetationSuitability[dispatch_thread_id.xy] = vegetation_suitability;
}
