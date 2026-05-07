Texture2D<float4> g_ClimateField : register(t0);
Texture2D<float4> g_WindField : register(t1);
SamplerState g_LinearSampler : register(s0);
RWTexture2D<float4> g_Output : register(u0);

cbuffer CS_METEOROGRAPH : register(b0)
{
    float g_CameraWorldX;
    float g_CameraWorldZ;
    float g_WorldMinX;
    float g_WorldMinZ;
    float g_WorldMaxX;
    float g_WorldMaxZ;
    float g_ArrowGridCols;
    float g_ArrowGridRows;
    uint g_Width;
    uint g_Height;
    uint g_Padding0;
    uint g_Padding1;
};

float sdSegment(float2 p, float2 a, float2 b)
{
    float2 pa = p - a;
    float2 ba = b - a;
    float h = saturate(dot(pa, ba) / max(dot(ba, ba), 1e-5));
    return length(pa - ba * h);
}

float3 sampleSmoothedWind(float2 uv, float2 texel_size)
{
    float4 w_center = g_WindField.SampleLevel(g_LinearSampler, uv, 0);
    float4 w_left = g_WindField.SampleLevel(g_LinearSampler, uv + float2(-texel_size.x * 2.0, 0.0), 0);
    float4 w_right = g_WindField.SampleLevel(g_LinearSampler, uv + float2(texel_size.x * 2.0, 0.0), 0);
    float4 w_up = g_WindField.SampleLevel(g_LinearSampler, uv + float2(0.0, -texel_size.y * 2.0), 0);
    float4 w_down = g_WindField.SampleLevel(g_LinearSampler, uv + float2(0.0, texel_size.y * 2.0), 0);

    float2 dir =
        (w_center.xy * 0.40 +
         w_left.xy * 0.15 +
         w_right.xy * 0.15 +
         w_up.xy * 0.15 +
         w_down.xy * 0.15) * 2.0 - 1.0;

    float len_sq = dot(dir, dir);
    if (len_sq < 1e-5)
    {
        dir = float2(1.0, 0.0);
    }
    else
    {
        dir *= rsqrt(len_sq);
    }

    float strength =
        w_center.z * 0.40 +
        w_left.z * 0.15 +
        w_right.z * 0.15 +
        w_up.z * 0.15 +
        w_down.z * 0.15;

    return float3(dir, saturate(strength));
}

float drawArrow(float2 uv, float2 cell_uv, float2 dir, float strength)
{
    float2 cell_local = (uv - cell_uv) * float2(g_ArrowGridCols, g_ArrowGridRows);
    float2 n = normalize(dir);
    if (all(abs(n) < 1e-5))
    {
        n = float2(1.0, 0.0);
    }
    float2 tip = n * (0.18 + strength * 0.22);
    float2 tail = -n * (0.08 + strength * 0.06);

    float shaft = 1.0 - smoothstep(0.025, 0.055, sdSegment(cell_local, tail, tip));
    float2 wing_a = tip - n * (0.07 + strength * 0.05) + float2(-n.y, n.x) * (0.05 + strength * 0.04);
    float2 wing_b = tip - n * (0.07 + strength * 0.05) - float2(-n.y, n.x) * (0.05 + strength * 0.04);
    float head_a = 1.0 - smoothstep(0.022, 0.050, sdSegment(cell_local, tip, wing_a));
    float head_b = 1.0 - smoothstep(0.022, 0.050, sdSegment(cell_local, tip, wing_b));
    return max(shaft, max(head_a, head_b));
}

float drawRing(float2 uv, float2 center_uv)
{
    float2 diff = uv - center_uv;
    float radius = 0.030;
    float dist = length(diff);
    return 1.0 - smoothstep(0.0035, 0.0075, abs(dist - radius));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= g_Width || dispatch_thread_id.y >= g_Height)
    {
        return;
    }

    float2 uv = (dispatch_thread_id.xy + 0.5) / float2(g_Width, g_Height);
    float2 texel_size = 1.0 / float2(g_Width, g_Height);
    float3 climate = g_ClimateField.SampleLevel(g_LinearSampler, uv, 0).rgb;

    float2 cell_index = floor(uv * float2(g_ArrowGridCols, g_ArrowGridRows));
    float2 cell_uv = (cell_index + 0.5) / float2(g_ArrowGridCols, g_ArrowGridRows);
    float3 wind_sample = sampleSmoothedWind(cell_uv, texel_size);
    float2 wind_dir = wind_sample.xy;
    float wind_strength = wind_sample.z;

    float arrow_mask = drawArrow(uv, cell_uv, wind_dir, wind_strength);

    float2 marker_uv = float2(
        saturate((g_CameraWorldX - g_WorldMinX) / max(g_WorldMaxX - g_WorldMinX, 1e-5)),
        saturate((g_CameraWorldZ - g_WorldMinZ) / max(g_WorldMaxZ - g_WorldMinZ, 1e-5)));
    float ring_mask = drawRing(uv, marker_uv);

    float3 color = climate;
    color = lerp(color, float3(1.0, 1.0, 1.0), saturate(arrow_mask * 0.92));
    color = lerp(color, float3(1.0, 1.0, 1.0), saturate(ring_mask));
    g_Output[dispatch_thread_id.xy] = float4(color, 1.0);
}
