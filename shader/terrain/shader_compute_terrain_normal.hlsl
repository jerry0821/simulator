cbuffer TERRAIN_NORMAL_CONSTANT_BUFFER : register(b0)
{
    float field_width;
    float field_depth;
    float time_seconds;
    float wind_ripple_strength;
    uint width;
    uint height;
    float wind_ripple_frequency;
    float wind_ripple_speed;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D<float4> g_WaterVelocity : register(t1);
SamplerState g_TerrainSampler : register(s0);
RWTexture2D<float4> g_TerrainNormal : register(u0);

float2 SampleTerrainWaterHeight(int2 coord)
{
    coord.x = clamp(coord.x, 0, int(width) - 1);
    coord.y = clamp(coord.y, 0, int(height) - 1);
    const float4 sample_value = g_TerrainHeight.Load(int3(coord, 0));
    const float terrain_height = sample_value.x;
    const float water_surface_height = max(sample_value.y, terrain_height);
    return float2(terrain_height, water_surface_height);
}

float2 EncodeUpNormal(float3 normal)
{
    return normal.xz;
}

float2 SafeNormalize2(float2 value)
{
    const float len_sq = dot(value, value);
    return len_sq > 1.0e-6f ? value * rsqrt(len_sq) : float2(1.0f, 0.0f);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const int2 coord = int2(dispatch_thread_id.xy);
    const float2 center_height = SampleTerrainWaterHeight(coord);
    const float2 left_height = SampleTerrainWaterHeight(coord + int2(-1, 0));
    const float2 right_height = SampleTerrainWaterHeight(coord + int2(1, 0));
    const float2 up_height = SampleTerrainWaterHeight(coord + int2(0, -1));
    const float2 down_height = SampleTerrainWaterHeight(coord + int2(0, 1));
    const float2 left_far_height = SampleTerrainWaterHeight(coord + int2(-2, 0));
    const float2 right_far_height = SampleTerrainWaterHeight(coord + int2(2, 0));
    const float2 up_far_height = SampleTerrainWaterHeight(coord + int2(0, -2));
    const float2 down_far_height = SampleTerrainWaterHeight(coord + int2(0, 2));

    const float cell_size_x = field_width / max(float(width - 1), 1.0f);
    const float cell_size_z = field_depth / max(float(height - 1), 1.0f);

    const float3 terrain_tangent_x = float3(cell_size_x * 2.0f, right_height.x - left_height.x, 0.0f);
    const float3 terrain_tangent_z = float3(0.0f, down_height.x - up_height.x, cell_size_z * 2.0f);
    const float3 terrain_normal = normalize(cross(terrain_tangent_z, terrain_tangent_x));

    const float water_left =
        (left_far_height.y + left_height.y * 2.0f + center_height.y) * 0.25f;
    const float water_right =
        (right_far_height.y + right_height.y * 2.0f + center_height.y) * 0.25f;
    const float water_up =
        (up_far_height.y + up_height.y * 2.0f + center_height.y) * 0.25f;
    const float water_down =
        (down_far_height.y + down_height.y * 2.0f + center_height.y) * 0.25f;
    const float3 water_tangent_x = float3(cell_size_x * 2.0f, water_right - water_left, 0.0f);
    const float3 water_tangent_z = float3(0.0f, water_down - water_up, cell_size_z * 2.0f);
    float3 water_normal = normalize(cross(water_tangent_z, water_tangent_x));

    const float water_depth = max(center_height.y - center_height.x, 0.0f);
    const float local_wave_energy = max(abs(water_right - water_left), abs(water_down - water_up));
    const float shallow_stability = 1.0f - smoothstep(0.03f, 0.12f, water_depth);
    const float micro_ripple_stability = 1.0f - smoothstep(0.0015f, 0.012f, local_wave_energy);
    const float terrain_blend = saturate(max(shallow_stability, micro_ripple_stability * 0.65f));
    water_normal = normalize(lerp(water_normal, terrain_normal, terrain_blend));

    const float4 velocity_sample = g_WaterVelocity.Load(int3(coord, 0));
    const float2 flow_velocity = velocity_sample.xy;
    const float water_speed = saturate(velocity_sample.z);
    const float transport_energy = saturate(velocity_sample.w);
    const float flow_presence = saturate(max(water_speed, transport_energy * 0.90f));
    const float ripple_visibility =
        smoothstep(0.006f, 0.08f, water_depth) *
        smoothstep(0.004f, 0.08f, flow_presence);
    if (ripple_visibility > 1.0e-4f)
    {
        const float2 world_pos = float2(
            ((float(coord.x) + 0.5f) / max(float(width), 1.0f) - 0.5f) * field_width,
            ((float(coord.y) + 0.5f) / max(float(height), 1.0f) - 0.5f) * field_depth);
        const float2 ripple_dir = SafeNormalize2(flow_velocity);
        const float2 ripple_cross = float2(-ripple_dir.y, ripple_dir.x);
        const float along = dot(world_pos, ripple_dir);
        const float across = dot(world_pos, ripple_cross);
        const float phase_a =
            along * wind_ripple_frequency -
            time_seconds * wind_ripple_speed * (0.35f + flow_presence * 0.85f);
        const float phase_b =
            (along * 0.62f + across * 0.28f) * (wind_ripple_frequency * 1.7f) -
            time_seconds * (wind_ripple_speed * 1.32f) * (0.25f + flow_presence * 0.65f);
        const float ripple_amp = wind_ripple_strength * ripple_visibility;
        const float2 ripple_slope =
            ripple_dir * (cos(phase_a) * ripple_amp * 0.58f + cos(phase_b) * ripple_amp * 0.28f) +
            ripple_cross * (sin(phase_b) * ripple_amp * 0.14f);
        water_normal = normalize(float3(
            water_normal.x - ripple_slope.x,
            max(water_normal.y, 0.35f),
            water_normal.z - ripple_slope.y));
    }

    g_TerrainNormal[dispatch_thread_id.xy] = float4(
        EncodeUpNormal(terrain_normal),
        EncodeUpNormal(water_normal));
}
