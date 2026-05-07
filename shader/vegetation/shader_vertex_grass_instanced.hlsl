cbuffer VS_CONSTANT_BUFFER : register(b0)
{
    float4x4 view;
};

cbuffer VS_CONSTANT_BUFFER : register(b1)
{
    float4x4 proj;
};

cbuffer VS_GRASS_WIND : register(b2)
{
    float g_TimeSeconds;
    float g_FieldUvScale;
    float g_BendScale;
    float g_Padding0;
};

Texture2D g_WindField : register(t0);
SamplerState samp : register(s0);

struct VS_IN
{
    float3 posL : POSITION0;
    float3 normalL : NORMAL0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float4 world0 : TEXCOORD1;
    float4 world1 : TEXCOORD2;
    float4 world2 : TEXCOORD3;
    float4 world3 : TEXCOORD4;
};

struct VS_OUT
{
    float4 posH : SV_POSITION;
    float4 posW : POSITION0;
    float4 normalW : NORMAL0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

float4 MulPointByInstance(float4 point_value, VS_IN vi)
{
    return float4(
        dot(point_value, vi.world0),
        dot(point_value, vi.world1),
        dot(point_value, vi.world2),
        dot(point_value, vi.world3));
}

float3 MulVectorByInstance(float3 vector_value, VS_IN vi)
{
    return float3(
        dot(vector_value, vi.world0.xyz),
        dot(vector_value, vi.world1.xyz),
        dot(vector_value, vi.world2.xyz));
}

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;

    const float4 localPos = float4(vi.posL, 1.0f);
    float4 worldPos = MulPointByInstance(localPos, vi);
    float3 worldNormal = normalize(MulVectorByInstance(vi.normalL, vi));

    const float3 instanceOrigin = float3(vi.world0.w, vi.world1.w, vi.world2.w);
    const float2 windUv = frac(instanceOrigin.xz * g_FieldUvScale);
    const float4 windSample = g_WindField.SampleLevel(samp, windUv, 0.0f);
    float2 windDir = windSample.xy * 2.0f - 1.0f;
    float windStrength = saturate(windSample.z);
    const float windDirLen = length(windDir);
    if (windDirLen > 0.0001f)
    {
        windDir /= windDirLen;
    }
    else
    {
        windDir = float2(1.0f, 0.0f);
    }

    windStrength = windStrength * windStrength;

    const float bendFactor = saturate(vi.posL.y + 0.5f);
    const float bendWeight = bendFactor * bendFactor * bendFactor;
    const float2 crossDir = float2(-windDir.y, windDir.x);

    // Drive the main motion from a world-space band that travels along the wind direction.
    const float alongWind = dot(instanceOrigin.xz, windDir * 0.032f);
    const float acrossWind = dot(instanceOrigin.xz, crossDir * 0.010f);
    const float sweepPhase = g_TimeSeconds * (0.18f + windStrength * 0.10f) - alongWind;
    const float waveWarp = sin(acrossWind + g_TimeSeconds * 0.04f) * 0.45f;
    const float sweepWave = sin(sweepPhase + waveWarp);

    // Keep a smaller local flutter so neighboring clumps do not move identically.
    const float localPhase = g_TimeSeconds * 0.08f + dot(instanceOrigin.xz, float2(0.006f, 0.004f));
    const float localFlutter = sin(localPhase);

    const float gustPhase = g_TimeSeconds * 0.02f + dot(instanceOrigin.xz, float2(0.0018f, 0.0023f));
    const float gust = 0.5f + 0.5f * sin(gustPhase);
    const float staticLean = bendWeight * g_BendScale * windStrength * 0.060f;
    const float dynamicWave = 0.70f * sweepWave + 0.30f * localFlutter;
    const float dynamicBend = bendWeight * g_BendScale * (0.010f + windStrength * 0.035f + gust * 0.012f) * dynamicWave;
    const float bendAmount = staticLean + dynamicBend;

    worldPos.x += windDir.x * bendAmount;
    worldPos.z += windDir.y * bendAmount;

    vo.posW = worldPos;
    vo.normalW = float4(worldNormal, 0.0f);
    vo.color = vi.color;
    vo.uv = vi.uv;

    const float4 viewPos = mul(worldPos, view);
    vo.posH = mul(viewPos, proj);

    return vo;
}
