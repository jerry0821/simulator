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

Texture2D<float2> water_surface_height_tex : register(t0);
SamplerState samp : register(s0);

float4 main(PS_INPUT ps_in) : SV_TARGET
{
    uint tex_width = 0;
    uint tex_height = 0;
    water_surface_height_tex.GetDimensions(tex_width, tex_height);
    const uint2 texel = uint2(
        min((uint)round(ps_in.uv.x * max((int)tex_width - 1, 0)), tex_width - 1),
        min((uint)round(ps_in.uv.y * max((int)tex_height - 1, 0)), tex_height - 1));
    const float2 terrain_water_height = water_surface_height_tex.Load(int3(texel, 0));
    const float water_depth = max(terrain_water_height.y - terrain_water_height.x, 0.0f);
    const float alpha = water_depth > 0.001f
        ? saturate(0.12f + water_depth * 0.22f)
        : 0.0f;
    const float depth_tint = saturate(water_depth * 0.08f);
    const float3 shallow_color = float3(0.34f, 0.78f, 1.00f);
    const float3 deep_color = float3(0.03f, 0.20f, 0.62f);
    const float3 water_color = lerp(shallow_color, deep_color, depth_tint);
    return float4(water_color, alpha);
}
