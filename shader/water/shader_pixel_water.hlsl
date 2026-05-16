cbuffer PS_CONSTANT_BUFFER0 : register(b0)
{
    float4 diffuse_color;
};

cbuffer PS_CONSTANT_BUFFER1 : register(b1)
{
    float3 camera_position;
    float fresnel_power;
    float highlight_strength;
    float time_seconds;
    float surface_center_x;
    float surface_center_z;
    float surface_size_x;
    float surface_size_z;
    float padding0;
    float padding1;
};

struct PS_INPUT
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

Texture2D surface_water_tex : register(t0);
SamplerState samp : register(s0);

float4 main(PS_INPUT ps_in) : SV_TARGET
{
    const float2 uv = saturate(ps_in.uv);
    const float water_depth = max(surface_water_tex.SampleLevel(samp, uv, 0.0f).r, 0.0f);
    const float alpha = smoothstep(0.004f, 0.035f, water_depth) * 0.34f;
    const float depth_factor = smoothstep(0.01f, 0.10f, water_depth);
    const float3 color = lerp(
        diffuse_color.rgb * float3(0.95f, 0.99f, 1.02f),
        diffuse_color.rgb * float3(0.76f, 0.86f, 0.98f),
        depth_factor);
    return float4(color, alpha);
}
