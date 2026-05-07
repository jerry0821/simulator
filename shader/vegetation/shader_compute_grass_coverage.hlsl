cbuffer GRASS_INSTANCE_CONSTANT_BUFFER : register(b0)
{
    float4x4 view_proj;
    float quad_scale_x;
    float quad_scale_y;
    float world_min_x;
    float world_min_z;
    float spacing;
    float sample_offset;
    float water_height;
    float camera_x;
    float camera_z;
    float lod_full_distance;
    float lod_max_distance;
    float far_keep_probability;
    uint grid_cols;
    uint grid_rows;
    uint quads_per_seed;
    uint padding0;
};

struct GrassSeed
{
    float4 data; // x = world_x, y = ground_y, z = world_z, w = valid
};

Texture2D g_TerrainHeight : register(t0);
Texture2D g_TerrainVegetationSuitability : register(t1);
Texture2D g_TerrainClassification : register(t2);
SamplerState g_HeightSampler : register(s0);
RWStructuredBuffer<GrassSeed> g_Seeds : register(u0);

float frac1(float v)
{
    return v - floor(v);
}

float Hash21(float2 p)
{
    p = frac(p * float2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return frac(p.x * p.y);
}

float Smoothstep(float v)
{
    float t = saturate(v);
    return t * t * (3.0 - 2.0 * t);
}

float RemapClamped(float value, float in_min, float in_max)
{
    return saturate((value - in_min) / max(in_max - in_min, 1.0e-4));
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

float Fbm(float2 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    float2 domain = p;

    [unroll]
    for (int octave = 0; octave < 4; ++octave)
    {
        value += ValueNoise(domain) * amplitude;
        domain = domain * 2.03 + float2(17.0, 9.0);
        amplitude *= 0.5;
    }

    return value;
}

float SampleTerrainHeight(float2 world_xz)
{
    const float field_width = 512.0f;
    const float field_depth = 512.0f;
    float2 uv = float2(
        saturate((world_xz.x + field_width * 0.5f) / field_width),
        saturate((world_xz.y + field_depth * 0.5f) / field_depth));
    return g_TerrainHeight.SampleLevel(g_HeightSampler, uv, 0.0f).r;
}

float GenerateTerrainHeight(float2 world_xz)
{
    return SampleTerrainHeight(world_xz);
}

float GenerateSmoothedTerrainHeight(float2 world_xz)
{
    return SampleTerrainHeight(world_xz);
}

float SampleVegetationSuitability(float2 world_xz)
{
    const float field_width = 512.0f;
    const float field_depth = 512.0f;
    float2 uv = float2(
        saturate((world_xz.x + field_width * 0.5f) / field_width),
        saturate((world_xz.y + field_depth * 0.5f) / field_depth));
    return g_TerrainVegetationSuitability.SampleLevel(g_HeightSampler, uv, 0.0f).r;
}

float4 SampleTerrainClassification(float2 world_xz)
{
    const float field_width = 512.0f;
    const float field_depth = 512.0f;
    float2 uv = float2(
        saturate((world_xz.x + field_width * 0.5f) / field_width),
        saturate((world_xz.y + field_depth * 0.5f) / field_depth));
    return g_TerrainClassification.SampleLevel(g_HeightSampler, uv, 0.0f);
}

float EstimateNormalY(float2 world_xz)
{
    float left_height = GenerateSmoothedTerrainHeight(world_xz + float2(-sample_offset, 0.0f));
    float right_height = GenerateSmoothedTerrainHeight(world_xz + float2(sample_offset, 0.0f));
    float up_height = GenerateSmoothedTerrainHeight(world_xz + float2(0.0f, -sample_offset));
    float down_height = GenerateSmoothedTerrainHeight(world_xz + float2(0.0f, sample_offset));

    float3 tangent = float3(sample_offset * 2.0f, right_height - left_height, 0.0f);
    float3 bitangent = float3(0.0f, down_height - up_height, sample_offset * 2.0f);
    float3 normal = normalize(cross(bitangent, tangent));
    return saturate(normal.y);
}

[numthreads(64, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    const uint candidate_count = grid_cols * grid_rows;
    const uint seed_index = dispatch_thread_id.x;
    if (seed_index >= candidate_count)
    {
        return;
    }

    const uint col = seed_index % max(grid_cols, 1u);
    const uint row = seed_index / max(grid_cols, 1u);
    const float2 grid_pos = float2((float)col, (float)row);
    float2 jitter = float2(
        Hash21(grid_pos + float2(7.3f, 11.9f)) - 0.5f,
        Hash21(grid_pos + float2(13.7f, 5.1f)) - 0.5f);
    jitter *= float2(spacing * 0.62f, spacing * 0.58f);

    float2 world_xz = float2(
        world_min_x + (float)col * spacing,
        world_min_z + (float)row * spacing) + jitter;

    float ground_y = GenerateSmoothedTerrainHeight(world_xz);
    float vegetation_suitability = SampleVegetationSuitability(world_xz);
    float4 classification = SampleTerrainClassification(world_xz);
    float wetness = classification.g;
    float erosion = classification.a;

    float normal_y = EstimateNormalY(world_xz);
    float geometric_confirmation = smoothstep(0.48f, 0.76f, normal_y);
    float classified_coverage =
        vegetation_suitability *
        lerp(0.92f, 1.0f, geometric_confirmation) *
        lerp(1.0f, 0.96f, smoothstep(0.86f, 0.99f, wetness)) *
        lerp(1.0f, 0.97f, erosion);
    bool valid = (wetness < 0.992f) && (classified_coverage > 0.024f);

    GrassSeed seed;
    seed.data = valid ? float4(world_xz.x, ground_y, world_xz.y, 1.0f) : float4(0.0f, 0.0f, 0.0f, 0.0f);
    g_Seeds[seed_index] = seed;
}
