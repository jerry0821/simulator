cbuffer EROSION_DELTA_CONSTANT_BUFFER : register(b0)
{
    float erosion_rate;
    float deposition_rate;
    float thermal_rate;
    float relaxation_rate;
    float max_erosion;
    float max_deposition;
    float thermal_threshold;
    float hydraulic_bias;
    uint width;
    uint height;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D<float2> g_WaterSurfaceHeight : register(t1);
Texture2D g_WaterVelocity : register(t2);
Texture2D g_WaterSediment : register(t3);
Texture2D g_PreviousErosionDelta : register(t4);
SamplerState g_ErosionSampler : register(s0);
RWTexture2D<float4> g_ErosionDelta : register(u0);

float2 SampleAccumulated(float2 uv)
{
    return g_PreviousErosionDelta.SampleLevel(g_ErosionSampler, saturate(uv), 0.0f).rg;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    float2 uv = (float2(dispatch_thread_id.xy) + 0.5f) / float2(width, height);
    float2 texel = 1.0f / float2(width, height);

    float terrain_center = g_TerrainHeight.SampleLevel(g_ErosionSampler, saturate(uv), 0.0f).r;
    float terrain_e = g_TerrainHeight.SampleLevel(g_ErosionSampler, saturate(uv + float2(texel.x, 0.0f)), 0.0f).r;
    float terrain_w = g_TerrainHeight.SampleLevel(g_ErosionSampler, saturate(uv + float2(-texel.x, 0.0f)), 0.0f).r;
    float terrain_n = g_TerrainHeight.SampleLevel(g_ErosionSampler, saturate(uv + float2(0.0f, texel.y)), 0.0f).r;
    float terrain_s = g_TerrainHeight.SampleLevel(g_ErosionSampler, saturate(uv + float2(0.0f, -texel.y)), 0.0f).r;

    float2 water_height = g_WaterSurfaceHeight.SampleLevel(g_ErosionSampler, saturate(uv), 0.0f);
    float water_amount = max(water_height.y - water_height.x, 0.0f);
    float standing_water = smoothstep(0.04f, 0.16f, water_amount);
    float4 velocity = g_WaterVelocity.SampleLevel(g_ErosionSampler, saturate(uv), 0.0f);
    float4 sediment = g_WaterSediment.SampleLevel(g_ErosionSampler, saturate(uv), 0.0f);
    float2 flow_vector = velocity.xy;
    float flow_strength = saturate(velocity.z);
    float transport_energy = saturate(velocity.w);
    float suspended_sediment = saturate(sediment.r);
    float deposition_tendency = saturate(sediment.g);
    float erosion_tendency = saturate(sediment.b);
    float sediment_capacity = saturate(sediment.a);

    float2 previous = SampleAccumulated(uv);

    float2 terrain_gradient = float2(terrain_e - terrain_w, terrain_n - terrain_s);
    float slope = saturate(length(terrain_gradient) * 0.20f);
    float basin_factor = saturate((terrain_center - min(min(terrain_e, terrain_w), min(terrain_n, terrain_s))) * 0.12f + 0.5f);
    float hydraulic_support =
        saturate((0.25f + water_amount * 0.75f) * (0.20f + slope * 0.80f) * (0.35f + transport_energy * 0.65f));
    float sediment_gap = sediment_capacity - suspended_sediment;
    float hydraulic_erosion_gain =
        max(sediment_gap, 0.0f) *
        erosion_rate *
        hydraulic_support *
        (hydraulic_bias + erosion_tendency * (1.0f - hydraulic_bias));
    float hydraulic_deposition_gain =
        max(-sediment_gap, 0.0f) *
        deposition_rate *
        (0.45f + standing_water * 0.55f) *
        (0.35f + deposition_tendency * 0.65f) *
        (1.0f - slope * 0.45f);

    float neighbor_height = (terrain_e + terrain_w + terrain_n + terrain_s) * 0.25f;
    float thermal_drive = max(terrain_center - neighbor_height, 0.0f);
    float underwater_factor = lerp(1.0f, 0.42f, standing_water);
    float thermal_excess = max(slope - thermal_threshold * underwater_factor, 0.0f);
    float thermal_erosion_gain =
        thermal_drive *
        thermal_rate *
        thermal_excess *
        (0.35f + water_amount * 0.25f + transport_energy * 0.20f);
    float thermal_deposition_gain =
        thermal_erosion_gain *
        (0.18f + basin_factor * 0.22f);

    float instantaneous_erosion =
        hydraulic_erosion_gain +
        thermal_erosion_gain -
        hydraulic_deposition_gain * 0.04f;
    float instantaneous_deposition =
        hydraulic_deposition_gain +
        thermal_deposition_gain -
        hydraulic_erosion_gain * 0.02f;

    float erosion =
        max(
            lerp(previous.x * 0.82f, instantaneous_erosion, 0.28f) -
            relaxation_rate * 0.45f,
            0.0f);
    float deposition =
        max(
            lerp(previous.y * 0.86f, instantaneous_deposition, 0.24f) -
            relaxation_rate * 0.22f,
            0.0f);

    erosion = min(erosion, max_erosion);
    deposition = min(deposition, max_deposition);

    float signed_delta = deposition - erosion;
    float flow_preview = saturate(max(transport_energy, flow_strength));

    g_ErosionDelta[dispatch_thread_id.xy] = float4(erosion, deposition, signed_delta, flow_preview);
}
