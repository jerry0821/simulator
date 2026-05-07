cbuffer EROSION_DELTA_CONSTANT_BUFFER : register(b0)
{
    float erosion_rate;
    float deposition_rate;
    float smoothing_rate;
    float relaxation_rate;
    float max_erosion;
    float max_deposition;
    uint width;
    uint height;
};

Texture2D g_TerrainHeight : register(t0);
Texture2D g_SurfaceWater : register(t1);
Texture2D g_SurfaceWaterFlow : register(t2);
Texture2D g_PreviousErosionDelta : register(t3);
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

    float4 surface = g_SurfaceWater.SampleLevel(g_ErosionSampler, saturate(uv), 0.0f);
    float water_amount = surface.r;
    float standing_water = smoothstep(0.04f, 0.16f, water_amount);
    float4 flow = g_SurfaceWaterFlow.SampleLevel(g_ErosionSampler, saturate(uv), 0.0f);
    float2 flow_vector = float2(flow.x - flow.y, flow.w - flow.z);
    float flow_strength =
        saturate(length(flow_vector) * 6.0f + (flow.x + flow.y + flow.z + flow.w) * 3.0f);

    float2 previous = SampleAccumulated(uv);
    float2 neighbor_average =
        SampleAccumulated(uv + float2(texel.x, 0.0f)) +
        SampleAccumulated(uv + float2(-texel.x, 0.0f)) +
        SampleAccumulated(uv + float2(0.0f, texel.y)) +
        SampleAccumulated(uv + float2(0.0f, -texel.y));
    neighbor_average *= 0.25f;

    float2 terrain_gradient = float2(terrain_e - terrain_w, terrain_n - terrain_s);
    float slope = saturate(length(terrain_gradient) * 0.20f);
    float basin_factor = saturate((terrain_center - min(min(terrain_e, terrain_w), min(terrain_n, terrain_s))) * 0.12f + 0.5f);

    float transport_energy =
        flow_strength *
        (0.35f + water_amount * 0.65f) *
        (0.25f + standing_water * 0.75f) *
        (0.10f + slope * 0.90f);
    transport_energy = saturate(transport_energy);

    float erosion_gain = transport_energy * erosion_rate;
    float deposition_gain =
        (1.0f - flow_strength) *
        standing_water *
        (0.15f + water_amount * 0.85f) *
        (1.0f - slope) *
        basin_factor *
        deposition_rate;

    float erosion = previous.x + erosion_gain - deposition_gain * 0.10f;
    float deposition = previous.y + deposition_gain - erosion_gain * 0.08f;

    erosion = lerp(erosion, neighbor_average.x, smoothing_rate);
    deposition = lerp(deposition, neighbor_average.y, smoothing_rate * 0.75f);

    erosion = max(erosion - relaxation_rate, 0.0f);
    deposition = max(deposition - relaxation_rate * 0.50f, 0.0f);

    erosion = min(erosion, max_erosion);
    deposition = min(deposition, max_deposition);

    float signed_preview = saturate(0.5f + (deposition - erosion) * 2.5f);
    float flow_preview = saturate(transport_energy);

    g_ErosionDelta[dispatch_thread_id.xy] = float4(erosion, deposition, signed_preview, flow_preview);
}
