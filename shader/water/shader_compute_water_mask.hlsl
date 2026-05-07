cbuffer WATER_MASK_CONSTANT_BUFFER : register(b0)
{
    float water_threshold;
    float water_fade;
    float shore_band;
    float padding0;
    uint width;
    uint height;
    uint padding1;
    uint padding2;
};

Texture2D g_VisibleWater : register(t0);
SamplerState g_SurfaceSampler : register(s0);
RWTexture2D<float4> g_WaterMask : register(u0);

float Smoothstep01(float v)
{
    float t = saturate(v);
    return t * t * (3.0 - 2.0 * t);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    float2 uv = (float2(dispatch_thread_id.xy) + 0.5) / float2(width, height);
    float4 visible_water = g_VisibleWater.SampleLevel(g_SurfaceSampler, uv, 0.0f);
    float water_alpha = visible_water.r;
    float runoff_hint = visible_water.g;
    float pooled_water = visible_water.b;

    float2 texel = 1.0f / float2(width, height);
    float4 surface_left = g_VisibleWater.SampleLevel(g_SurfaceSampler, uv + float2(-texel.x, 0.0f), 0.0f);
    float4 surface_right = g_VisibleWater.SampleLevel(g_SurfaceSampler, uv + float2(texel.x, 0.0f), 0.0f);
    float4 surface_up = g_VisibleWater.SampleLevel(g_SurfaceSampler, uv + float2(0.0f, -texel.y), 0.0f);
    float4 surface_down = g_VisibleWater.SampleLevel(g_SurfaceSampler, uv + float2(0.0f, texel.y), 0.0f);

    const float visible_left = surface_left.r;
    const float visible_right = surface_right.r;
    const float visible_up = surface_up.r;
    const float visible_down = surface_down.r;

    const float neighbor_average = 0.25f * (visible_left + visible_right + visible_up + visible_down);
    const float edge_soften =
        saturate(lerp(neighbor_average * 0.35f, neighbor_average * 0.80f, water_alpha));
    water_alpha = max(water_alpha, edge_soften);

    const float runoff_edge =
        max(max(surface_left.g, surface_right.g), max(surface_up.g, surface_down.g));
    water_alpha = max(water_alpha, runoff_edge * 0.08f);

    float water_gradient =
        abs(surface_left.r - surface_right.r) +
        abs(surface_up.r - surface_down.r) +
        0.45f * (abs(surface_left.b - surface_right.b) + abs(surface_up.b - surface_down.b));
    float shore_mask = Smoothstep01(saturate(water_gradient / max(shore_band, 1.0e-4f)));
    shore_mask *= smoothstep(0.12f, 0.55f, max(water_alpha, neighbor_average));
    shore_mask *= smoothstep(0.08f, 0.45f, max(pooled_water, max(surface_left.b, max(surface_right.b, max(surface_up.b, surface_down.b)))));

    g_WaterMask[dispatch_thread_id.xy] = float4(
        1.0f,
        1.0f,
        1.0f,
        saturate(max(water_alpha, shore_mask * 0.12f)));
}
