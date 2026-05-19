cbuffer VS_SHADOW_LIGHT_BUFFER : register(b0)
{
    float4x4 lightViewProjection;
};

cbuffer VS_SHADOW_WIND_BUFFER : register(b1)
{
    float time_seconds;
    float wind_dir_x;
    float wind_dir_y;
    float wind_strength;
    float world_min_x;
    float world_max_x;
    float world_min_z;
    float world_max_z;
};

Texture2D<float4> g_Meteorograph : register(t0);

struct GrassInstance
{
    float4 world0;
    float4 world1;
    float4 world2;
    float4 world3;
    float4 lodColor;
};

StructuredBuffer<GrassInstance> g_InstanceData : register(t1);
SamplerState g_WindSampler : register(s0);

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

struct VS_IN
{
    float4 posL : POSITION0;
    float4 normalL : NORMAL0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct VS_OUT
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 MulPointByInstance(float4 point_value, GrassInstance instance_data)
{
    return float4(
        dot(point_value, instance_data.world0),
        dot(point_value, instance_data.world1),
        dot(point_value, instance_data.world2),
        dot(point_value, instance_data.world3));
}

VS_OUT main(VS_IN vi, uint instance_id : SV_InstanceID)
{
    VS_OUT vo;

    GrassInstance instance_data = g_InstanceData[instance_id];
    float4 world0 = instance_data.world0;
    float4 world1 = instance_data.world1;
    float4 world2 = instance_data.world2;

    float blade_factor = saturate((vi.posL.y + 0.5f) / 1.0f);
    blade_factor = blade_factor * blade_factor;

    float4 world_origin = float4(world0.w, world1.w, world2.w, 1.0f);
    float2 global_wind_dir = normalize(float2(wind_dir_x, wind_dir_y) + 1.0e-6f.xx);
    float2 wind_uv = float2(
        saturate((world_origin.x - world_min_x) / max(world_max_x - world_min_x, 1.0e-4f)),
        saturate((world_origin.z - world_min_z) / max(world_max_z - world_min_z, 1.0e-4f)));

    uint meteo_width = 0;
    uint meteo_height = 0;
    g_Meteorograph.GetDimensions(meteo_width, meteo_height);
    float2 texel_size = 1.0f / max(float2(meteo_width, meteo_height), 1.0f.xx);
    float4 wind_center = g_Meteorograph.SampleLevel(g_WindSampler, wind_uv, 0.0f);
    float4 wind_xp = g_Meteorograph.SampleLevel(g_WindSampler, wind_uv + float2(texel_size.x, 0.0f), 0.0f);
    float4 wind_xm = g_Meteorograph.SampleLevel(g_WindSampler, wind_uv - float2(texel_size.x, 0.0f), 0.0f);
    float4 wind_yp = g_Meteorograph.SampleLevel(g_WindSampler, wind_uv + float2(0.0f, texel_size.y), 0.0f);
    float4 wind_ym = g_Meteorograph.SampleLevel(g_WindSampler, wind_uv - float2(0.0f, texel_size.y), 0.0f);
    float4 wind_sample = wind_center * 0.40f + (wind_xp + wind_xm + wind_yp + wind_ym) * 0.15f;

    float2 sampled_dir = wind_sample.xy;
    float sampled_strength = saturate(length(sampled_dir));
    sampled_dir = normalize(sampled_dir + 1.0e-6f.xx);

    float2 wind_dir = normalize(lerp(global_wind_dir, sampled_dir, 0.18f) + 1.0e-6f.xx);
    float local_strength = saturate(lerp(wind_strength, sampled_strength, 0.18f));

    float2 gust_uv =
        world_origin.xz * 0.028f +
        global_wind_dir * (time_seconds * 0.080f) +
        float2(-global_wind_dir.y, global_wind_dir.x) * sin(time_seconds * 0.035f) * 0.18f;
    float gust_noise = ValueNoise(gust_uv + float2(11.4f, 7.8f));
    float gust_window = smoothstep(0.62f, 0.84f, gust_noise);
    float gust_shape = gust_window * gust_window * (3.0f - 2.0f * gust_window);

    float sweep_phase = dot(world_origin.xz, wind_dir) * 0.06f + time_seconds * (1.6f + local_strength * 0.035f);
    float cross_phase = dot(world_origin.xz, float2(-wind_dir.y, wind_dir.x)) * 0.028f + time_seconds * 0.035f;

    float sweep_wave = sin(sweep_phase);
    float cross_wave = sin(cross_phase);
    float static_bend = (0.016f + local_strength * 0.055f) * blade_factor;
    float gust_bend = (0.034f + local_strength * 4.50f) * gust_shape * blade_factor;
    float bend_amount = static_bend + gust_bend;

    float4 posW = MulPointByInstance(float4(vi.posL.xyz, 1.0f), instance_data);
    posW.x += wind_dir.x * sweep_wave * bend_amount;
    posW.z += wind_dir.y * sweep_wave * bend_amount;
    posW.x += (-wind_dir.y) * cross_wave * gust_bend * 0.06f;
    posW.z += ( wind_dir.x) * cross_wave * gust_bend * 0.06f;

    vo.posH = mul(posW, lightViewProjection);
    vo.uv = vi.uv;
    return vo;
}
