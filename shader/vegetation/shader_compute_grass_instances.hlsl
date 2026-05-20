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
    float4 data; // x = world_x, y = ground_y, z = world_z, w = scale_hint (<= 0 means invalid)
};

struct GrassInstance
{
    float4 world0;
    float4 world1;
    float4 world2;
    float4 world3;
    float4 lodColor;
};

StructuredBuffer<GrassSeed> g_Seeds : register(t0);
AppendStructuredBuffer<GrassInstance> g_Instances : register(u0);

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

[numthreads(64, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    const uint candidate_count = grid_cols * grid_rows;
    const uint seed_index = dispatch_thread_id.x;
    if (seed_index >= candidate_count)
    {
        return;
    }

    const GrassSeed seed = g_Seeds[seed_index];
    if (seed.data.w <= 0.0f)
    {
        return;
    }

    const float2 world_xz = float2(seed.data.x, seed.data.z);
    const float ground_y = seed.data.y;
    const float scale_hint = max(seed.data.w, 0.001f);

    float2 camera_xz = float2(camera_x, camera_z);
    float distance_to_camera = distance(world_xz, camera_xz);
    if (distance_to_camera >= lod_max_distance)
    {
        return;
    }

    const uint col = seed_index % max(grid_cols, 1u);
    const uint row = seed_index / max(grid_cols, 1u);
    const float2 grid_pos = float2((float)col, (float)row);

    const bool use_billboard_lod = distance_to_camera >= lod_full_distance;
    float lod1_t = 0.0f;
    if (use_billboard_lod)
    {
        lod1_t = saturate((distance_to_camera - lod_full_distance) / max(lod_max_distance - lod_full_distance, 1.0f));
        float keep_probability = lerp(1.0f, far_keep_probability, lod1_t);
        float lod_hash = Hash21(grid_pos + float2(53.0f, 71.0f));
        if (lod_hash > keep_probability)
        {
            return;
        }
    }

    const float angle_offsets[3] = {
        0.0f,
        1.04719755f,
        2.09439510f
    };

    const float scale_noise = Hash21(world_xz * 0.18f + float2(41.0f, -17.0f));
    const float scale_jitter = 0.78f + scale_noise * 0.52f;
    const float lod_scale = use_billboard_lod ? lerp(0.92f, 0.52f, lod1_t) : 1.0f;
    const float yaw_offset = Hash21(grid_pos + float2(29.0f, 31.0f)) * 3.14159265f;

    const float sx = quad_scale_x * scale_jitter * lod_scale * scale_hint;
    const float sy = quad_scale_y * scale_jitter * lod_scale * scale_hint;

    const float tx = world_xz.x;
    // Temporary grounding override for tuning: push the whole grass clump downward.
    const float visual_root_pivot = 0.42f;
    const float ty = ground_y + sy * visual_root_pivot - 0.5f;
    const float tz = world_xz.y;

    const uint quad_count = use_billboard_lod ? 1u : max(quads_per_seed, 1u);
    const float billboard_angle = atan2(camera_xz.x - tx, camera_xz.y - tz);

    [unroll]
    for (uint quad_index = 0; quad_index < 3; ++quad_index)
    {
        if (quad_index >= quad_count)
        {
            break;
        }

        const float angle = yaw_offset + angle_offsets[quad_index];
        const float final_angle = use_billboard_lod ? billboard_angle : angle;
        const float s = sin(final_angle);
        const float c = cos(final_angle);

        GrassInstance instance_data;
        instance_data.world0 = float4(sx * c, 0.0f, s, tx);
        instance_data.world1 = float4(0.0f, sy, 0.0f, ty);
        instance_data.world2 = float4(-sx * s, 0.0f, c, tz);
        instance_data.world3 = float4(0.0f, 0.0f, 0.0f, 1.0f);
        instance_data.lodColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
        g_Instances.Append(instance_data);
    }
}
