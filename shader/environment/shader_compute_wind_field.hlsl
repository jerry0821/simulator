RWTexture2D<float4> g_Output : register(u0);

cbuffer CS_WIND_FIELD : register(b0)
{
    float g_TimeSeconds;
    float g_WindDirectionX;
    float g_WindDirectionY;
    float g_WindStrength;
    float g_WindCrossInfluence;
    float g_NoiseScale;
    float g_Padding0;
    float g_Padding1;
    uint g_Width;
    uint g_Height;
    uint g_Padding2;
    uint g_Padding3;
};

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

float FBM(float2 p)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    float2 domain = p;

    [unroll]
    for (int octave = 0; octave < 4; ++octave)
    {
        value += ValueNoise(domain) * amplitude;
        domain = domain * 2.03f + float2(17.0f, 9.0f);
        amplitude *= 0.5f;
    }

    return value;
}

float2 SafeNormalize(float2 v)
{
    float length_sq = dot(v, v);
    float2 result = float2(1.0f, 0.0f);
    if (length_sq >= 1e-6f)
    {
        result = v * rsqrt(length_sq);
    }
    return result;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= g_Width || dispatch_thread_id.y >= g_Height)
    {
        return;
    }

    float2 uv = (dispatch_thread_id.xy + 0.5) / float2(g_Width, g_Height);
    float2 wind_dir = SafeNormalize(float2(g_WindDirectionX, g_WindDirectionY));
    float2 cross_dir = float2(-wind_dir.y, wind_dir.x);

    float2 domain = uv * max(g_NoiseScale, 1.0);
    float2 drift = wind_dir * g_TimeSeconds * 0.008 + cross_dir * sin(g_TimeSeconds * 0.004) * 0.025;

    // Broad atmospheric bands provide only a weak global bias.
    float jet_band_north = exp(-pow((uv.y - 0.22) / 0.14, 2.0));
    float jet_band_mid = exp(-pow((uv.y - 0.52) / 0.20, 2.0));
    float jet_band_south = exp(-pow((uv.y - 0.80) / 0.16, 2.0));
    float2 bias_flow =
        wind_dir * (0.48 + jet_band_north * 0.42 + jet_band_mid * 0.16 - jet_band_south * 0.06) +
        cross_dir * ((jet_band_north - jet_band_south) * 0.08 + (jet_band_mid - 0.35) * 0.03);

    // Curl-noise dominated flow: start from a smooth scalar potential and derive a divergence-free field.
    float2 warp = float2(
        FBM(domain * 0.14 + drift * 10.0 + float2(7.1, 13.4)),
        FBM(domain * 0.14 - drift * 8.0 + float2(-4.8, 3.2))) - 0.5;
    float2 flow_uv = uv + drift + warp * (0.08 + g_WindCrossInfluence * 0.06);

    const float eps = 0.010;
    float potential_x1 = FBM((flow_uv + float2(eps, 0.0)) * 2.0 + float2(11.0, 5.0))
                       + FBM((flow_uv + float2(eps, 0.0)) * 4.2 + float2(-9.2, 14.7)) * 0.28;
    float potential_x0 = FBM((flow_uv - float2(eps, 0.0)) * 2.0 + float2(11.0, 5.0))
                       + FBM((flow_uv - float2(eps, 0.0)) * 4.2 + float2(-9.2, 14.7)) * 0.28;
    float potential_y1 = FBM((flow_uv + float2(0.0, eps)) * 2.0 + float2(11.0, 5.0))
                       + FBM((flow_uv + float2(0.0, eps)) * 4.2 + float2(-9.2, 14.7)) * 0.28;
    float potential_y0 = FBM((flow_uv - float2(0.0, eps)) * 2.0 + float2(11.0, 5.0))
                       + FBM((flow_uv - float2(0.0, eps)) * 4.2 + float2(-9.2, 14.7)) * 0.28;

    float dphi_dx = (potential_x1 - potential_x0) / (eps * 2.0);
    float dphi_dy = (potential_y1 - potential_y0) / (eps * 2.0);
    float2 curl_flow = float2(dphi_dy, -dphi_dx);

    // Keep the global wind as the dominant direction and use curl only as a gentle perturbation.
    float2 combined_flow = bias_flow * 1.35 + curl_flow * 0.28;
    float2 local_dir = SafeNormalize(combined_flow);

    float strength_noise = FBM(flow_uv * 1.4 + float2(9.6, -6.4));
    float strength = saturate(
        0.08 +
        g_WindStrength * 1.35 +
        length(curl_flow) * 0.06 +
        max(max(jet_band_north, jet_band_mid), jet_band_south) * 0.10 +
        (strength_noise - 0.5) * 0.06);
    float2 encoded_dir = local_dir * 0.5 + 0.5;

    g_Output[dispatch_thread_id.xy] = float4(encoded_dir, strength, 1.0);
}
