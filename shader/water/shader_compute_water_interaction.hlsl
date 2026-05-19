cbuffer WATER_INTERACTION_CONSTANT_BUFFER : register(b0)
{
    uint width;
    uint height;
    uint padding0;
    uint padding1;
};

Texture2D g_VisibleWater : register(t0);
Texture2D g_WaterMask : register(t1);
Texture2D g_SoilMoisture : register(t2);
SamplerState g_SurfaceSampler : register(s0);
RWTexture2D<float4> g_WaterInteractionData : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const float2 uv = (float2(dispatch_thread_id.xy) + 0.5f) / float2(width, height);
    const float4 visible_water = g_VisibleWater.SampleLevel(g_SurfaceSampler, uv, 0.0f);
    const float4 water_mask = g_WaterMask.SampleLevel(g_SurfaceSampler, uv, 0.0f);
    const float4 soil_moisture = g_SoilMoisture.SampleLevel(g_SurfaceSampler, uv, 0.0f);

    const float surface_interaction = saturate(
        max(
            water_mask.r,
            visible_water.r * 0.88f + visible_water.g * 0.22f));
    const float shoreline_influence = saturate(
        max(
            water_mask.g,
            max(soil_moisture.g, water_mask.b * 0.28f)));
    const float pooled_interaction = saturate(
        max(
            water_mask.b,
            visible_water.b * 0.92f));
    const float retained_wetness = saturate(
        max(
            soil_moisture.r * 0.72f,
            soil_moisture.a * 0.38f + soil_moisture.b * 0.08f));

    g_WaterInteractionData[dispatch_thread_id.xy] = float4(
        surface_interaction,
        shoreline_influence,
        pooled_interaction,
        retained_wetness);
}
