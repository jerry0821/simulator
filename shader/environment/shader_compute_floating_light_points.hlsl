struct FloatingLightSeed
{
    float4 base_position_size;
    float4 params;
};

struct InstanceData
{
    float4 world0;
    float4 world1;
    float4 world2;
    float4 world3;
    float4 color;
};

cbuffer CS_FLOATING_LIGHT_CONSTANT_BUFFER : register(b0)
{
    float time_seconds;
    float world_min_x;
    float world_max_x;
    float world_min_z;
    float world_max_z;
    uint seed_count;
    float padding0;
    float padding1;
    float padding2;
    float4 camera_position;
    float4 camera_forward;
    float4 camera_right;
    float4 camera_up;
};

StructuredBuffer<FloatingLightSeed> g_LightSeeds : register(t0);
Texture2D g_TerrainHeight : register(t1);
Texture2D<float4> g_Meteorograph : register(t2);
RWStructuredBuffer<InstanceData> g_InstanceData : register(u0);

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

float4 SampleSmoothedWind(float2 world_xz)
{
    uint width = 0;
    uint height = 0;
    g_Meteorograph.GetDimensions(width, height);

    float2 uv = float2(
        saturate((world_xz.x - world_min_x) / max(world_max_x - world_min_x, 1.0e-4f)),
        saturate((world_xz.y - world_min_z) / max(world_max_z - world_min_z, 1.0e-4f)));
    int2 center = int2(uv * float2(max(int(width) - 1, 0), max(int(height) - 1, 0)) + 0.5f.xx);
    int2 max_coord = int2(max(int(width) - 1, 0), max(int(height) - 1, 0));

    int2 xp = min(center + int2(1, 0), max_coord);
    int2 xm = max(center - int2(1, 0), int2(0, 0));
    int2 yp = min(center + int2(0, 1), max_coord);
    int2 ym = max(center - int2(0, 1), int2(0, 0));

    float4 wind_center = g_Meteorograph.Load(int3(center, 0));
    float4 wind_xp = g_Meteorograph.Load(int3(xp, 0));
    float4 wind_xm = g_Meteorograph.Load(int3(xm, 0));
    float4 wind_yp = g_Meteorograph.Load(int3(yp, 0));
    float4 wind_ym = g_Meteorograph.Load(int3(ym, 0));
    return wind_center * 0.40f + (wind_xp + wind_xm + wind_yp + wind_ym) * 0.15f;
}

float SampleTerrainHeightWorld(float2 world_xz)
{
    uint width = 0;
    uint height = 0;
    g_TerrainHeight.GetDimensions(width, height);

    float2 uv = float2(
        saturate(world_xz.x / 512.0f + 0.5f),
        saturate(world_xz.y / 512.0f + 0.5f));
    int2 coord = int2(uv * float2(max(int(width) - 1, 0), max(int(height) - 1, 0)) + 0.5f.xx);
    return g_TerrainHeight.Load(int3(coord, 0)).r;
}

float3 DecodeWindDirection(float4 wind_sample)
{
    float2 dir = wind_sample.xy;
    float len_sq = dot(dir, dir);
    if (len_sq < 1.0e-6f)
    {
        return float3(1.0f, 0.0f, 0.0f);
    }
    dir *= rsqrt(len_sq);
    return float3(dir.x, 0.0f, dir.y);
}

float4 DustPalette(float seed)
{
    const float4 a = float4(0.88f, 0.60f, 1.00f, 1.0f);
    const float4 b = float4(0.62f, 0.72f, 1.00f, 1.0f);
    const float4 c = float4(0.98f, 0.72f, 0.90f, 1.0f);
    const float4 d = float4(0.72f, 0.88f, 1.00f, 1.0f);
    const float4 e = float4(1.00f, 0.84f, 0.64f, 1.0f);

    float wrapped = frac(seed);
    float scaled = wrapped * 5.0f;
    float bucket = floor(scaled);
    float t = frac(scaled);

    if (bucket < 1.0f) return lerp(a, b, t);
    if (bucket < 2.0f) return lerp(b, c, t);
    if (bucket < 3.0f) return lerp(c, d, t);
    if (bucket < 4.0f) return lerp(d, e, t);
    return lerp(e, a, t);
}

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (index >= seed_count)
    {
        return;
    }

    const FloatingLightSeed seed = g_LightSeeds[index];
    const float3 base_position = seed.base_position_size.xyz;
    const float size = seed.base_position_size.w;
    const float phase = seed.params.x;
    const float bob_amplitude = seed.params.y;
    const float drift_amplitude = seed.params.z;

    const float lateral = base_position.x * 0.18f;
    const float vertical_seed = (base_position.y - 0.55f) * 0.24f;
    const float forward_distance = 2.6f + base_position.z * 0.18f;
    const float3 anchor_position =
        camera_position.xyz +
        camera_right.xyz * lateral +
        camera_up.xyz * vertical_seed +
        camera_forward.xyz * forward_distance;
    const float2 anchor_world_xz = anchor_position.xz;
    const float4 wind_sample = SampleSmoothedWind(anchor_world_xz);
    const float3 wind_dir3 = DecodeWindDirection(wind_sample);
    const float2 wind_dir = normalize(wind_dir3.xz + 1.0e-6f.xx);
    const float wind_strength = saturate(length(wind_sample.xy));
    const float2 cross_dir = float2(-wind_dir.y, wind_dir.x);

    const float stream_noise =
        ValueNoise(anchor_world_xz * 0.036f + float2(phase * 0.13f, phase * 0.09f));
    const float stream_bias = lerp(0.76f, 1.42f, stream_noise);
    const float sweep = sin(time_seconds * (0.44f + wind_strength * 0.22f) + phase) * 0.5f + 0.5f;
    const float eddy = sin(time_seconds * (0.31f + wind_strength * 0.12f) + phase * 1.7f);
    const float gust = 0.55f + wind_strength * 1.35f;
    const float2 drift =
        wind_dir *
        drift_amplitude *
        stream_bias *
        gust *
        (0.48f + sweep * 0.72f);
    const float2 cross =
        cross_dir *
        drift_amplitude *
        eddy *
        (0.12f + wind_strength * 0.24f);

    const float vertical =
        sin(time_seconds * 0.88f + phase) * bob_amplitude +
        cos(time_seconds * 0.43f + phase * 1.9f) * bob_amplitude * 0.42f +
        sweep * 0.18f;
    const float pulse_scale = 1.0f + 0.16f * sin(time_seconds * 0.92f + phase * 2.3f);

    const float3 forward = normalize(camera_forward.xyz + 1.0e-6f.xxx);
    const float3 right = normalize(camera_right.xyz + 1.0e-6f.xxx);
    const float3 up = normalize(camera_up.xyz + 1.0e-6f.xxx);

    const float3 final_position =
        float3(
            anchor_position.x + drift.x + cross.x,
            anchor_position.y + vertical,
            anchor_position.z + drift.y + cross.y);
    const float terrain_height = SampleTerrainHeightWorld(final_position.xz);
    const float floating_height = terrain_height + 0.45f + stream_noise * 0.35f + wind_strength * 0.12f;
    const float3 lifted_position = float3(
        final_position.x,
        max(final_position.y, floating_height),
        final_position.z);
    const float final_scale = size * pulse_scale * 1.55f;

    const float3 billboard_right = right;
    const float3 billboard_up = up;

    InstanceData instance;
    instance.world0 = float4(billboard_right.x * final_scale, billboard_up.x * final_scale, 0.0f, lifted_position.x);
    instance.world1 = float4(billboard_right.y * final_scale, billboard_up.y * final_scale, 0.0f, lifted_position.y);
    instance.world2 = float4(billboard_right.z * final_scale, billboard_up.z * final_scale, 0.0f, lifted_position.z);
    instance.world3 = float4(0.0f, 0.0f, 0.0f, 1.0f);
    const float color_seed = frac(phase * 0.15915494f + base_position.x * 0.023f + base_position.z * 0.019f);
    float4 tint = DustPalette(color_seed);
    const float color_pulse = 0.88f + 0.22f * sin(time_seconds * 1.10f + phase * 0.75f);
    tint.rgb *= color_pulse * 1.95f;
    tint.a = (0.76f + wind_strength * 0.18f) * (0.98f + sweep * 0.42f);
    instance.color = tint;
    g_InstanceData[index] = instance;
}
