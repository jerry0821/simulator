cbuffer TERRAIN_HEIGHT_CONSTANTS : register(b0)
{
    float g_BaseFrequency;
    float g_BaseHeight;
    float g_DetailFrequency;
    float g_DetailHeight;
    float g_RidgeFrequency;
    float g_RidgeHeight;
    float g_ContinentHeight;
    float g_LakeCenterZ;
    float g_LakeRadiusX;
    float g_LakeRadiusZ;
    float g_LakeDepth;
    float g_FieldWidth;
    float g_FieldDepth;
    float g_CellSizeX;
    float g_CellSizeZ;
    uint g_Width;
    uint g_Height;
    float g_Padding0;
    float g_Padding1;
    float g_Padding2;
};

RWTexture2D<float> g_TerrainHeight : register(u0);

float Hash21(float2 p)
{
    float value = sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f;
    return frac(value);
}

float Smoothstep01(float value)
{
    return value * value * (3.0f - 2.0f * value);
}

float RemapClamped(float value, float inMin, float inMax)
{
    return saturate((value - inMin) / max(inMax - inMin, 1.0e-5f));
}

float ValueNoise(float2 p)
{
    float2 floorP = floor(p);
    float2 fracP = frac(p);

    float n00 = Hash21(floorP);
    float n10 = Hash21(floorP + float2(1.0f, 0.0f));
    float n01 = Hash21(floorP + float2(0.0f, 1.0f));
    float n11 = Hash21(floorP + float2(1.0f, 1.0f));

    float2 smooth = fracP * fracP * (3.0f - 2.0f * fracP);
    float nx0 = lerp(n00, n10, smooth.x);
    float nx1 = lerp(n01, n11, smooth.x);
    return lerp(nx0, nx1, smooth.y);
}

float SignedValueNoise(float2 p)
{
    return ValueNoise(p) * 2.0f - 1.0f;
}

float WorleyNoise(float2 p)
{
    float2 cell = floor(p);
    float minDistanceSq = 1.0e9f;

    [unroll]
    for (int oz = -1; oz <= 1; ++oz)
    {
        [unroll]
        for (int ox = -1; ox <= 1; ++ox)
        {
            float2 sampleCell = cell + float2((float)ox, (float)oz);
            float pointX = sampleCell.x + Hash21(float2(sampleCell.x + 19.0f, sampleCell.y - 7.0f));
            float pointZ = sampleCell.y + Hash21(float2(sampleCell.x - 11.0f, sampleCell.y + 23.0f));
            float2 delta = float2(pointX, pointZ) - p;
            minDistanceSq = min(minDistanceSq, dot(delta, delta));
        }
    }

    return sqrt(minDistanceSq);
}

float Fbm(float2 p, int octaves)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;

    [loop]
    for (int octave = 0; octave < octaves; ++octave)
    {
        value += ValueNoise(p * frequency) * amplitude;
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }

    return value;
}

float RidgedFbm(float2 p, int octaves)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;

    [loop]
    for (int octave = 0; octave < octaves; ++octave)
    {
        float sampleValue = ValueNoise(p * frequency);
        sampleValue = 1.0f - abs(sampleValue * 2.0f - 1.0f);
        sampleValue *= sampleValue;
        value += sampleValue * amplitude;
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }

    return value;
}

float2 DomainWarp(float2 p, float frequency, float amplitude, float2 offset)
{
    float warpX = Fbm(p * frequency + offset, 3) - 0.5f;
    float warpZ = Fbm(p * frequency + float2(-offset.y, -offset.x), 3) - 0.5f;
    return float2(warpX, warpZ) * amplitude;
}

float GenerateTerrainHeight(float2 worldXZ)
{
    float2 uv = float2(
        (worldXZ.x + g_FieldWidth * 0.5f) / max(g_FieldWidth, 1.0e-5f),
        (worldXZ.y + g_FieldDepth * 0.5f) / max(g_FieldDepth, 1.0e-5f));

    const float baseScale = max(g_BaseFrequency / 0.0031f, 0.1f);
    const float detailScale = max(g_DetailFrequency / 1.7f, 0.1f);
    const float ridgeScale = max(g_RidgeFrequency / 0.78f, 0.1f);
    const float baseHeightScale = max(g_BaseHeight / 24.0f, 0.1f);
    const float detailHeightScale = max(g_DetailHeight / 3.2f, 0.1f);
    const float ridgeHeightScale = max(g_RidgeHeight / 26.0f, 0.1f);
    const float continentHeightScale = max(g_ContinentHeight / 6.5f, 0.1f);

    const float flattness = 0.25f;
    const float steepMaskScaling = 20.0f * baseScale;
    const float regularity = 0.4f;
    const float disturbanceIntensityScaling = 10.0f * baseScale;
    const float disturbanceFactor = 0.025f;
    const float disturbanceIntensityMin = 0.2f;
    const float disturbanceScaling = 200.0f * baseScale;

    const float baseTerrainScaling = 50.0f * baseScale;
    const float baseTerrainHeightFactor = 80.0f * baseHeightScale;
    const float baseTerrainBias = 4.0f;

    const uint fractalCount = 8u;
    const float minFractalHeightWeight = 0.1f;
    const float fractalHeightWeightIntensity = 4.0f * detailHeightScale;

    const float riftLimit = 0.4f;
    const float riftMaskScaling = 40.0f * baseScale;
    const float riftScaling = 80.0f * baseScale;
    const float riftDisturbanceIntensity = 2.0f;
    const float riftDepth = 10.0f * detailHeightScale;
    const float riftSoftness = 0.2f;
    const float riftWidth = 0.1f;

    const float ridgeScaling = 30.0f * ridgeScale;
    const float ridgeMaskScaling = 10.0f * ridgeScale;
    const float ridgeSoftness = 0.15f;
    const float ridgeTransparency = 0.2f;
    const float ridgeLimit = 0.25f;
    const float ridgeAmplitude = 60.0f * ridgeHeightScale;

    const float trendScaling = 2.0f * baseScale;
    const float trendAmplitude = 80.0f * continentHeightScale;

    float steep = smoothstep(
        0.0f,
        1.0f - flattness,
        ValueNoise(uv * steepMaskScaling + float2(3.1f, -7.4f)) - flattness);

    float disturbanceIntensity = max(
        ValueNoise(uv * disturbanceIntensityScaling + float2(-11.0f, 19.0f)) - regularity,
        disturbanceIntensityMin) * disturbanceFactor;
    float2 disturbance = float2(
        SignedValueNoise(uv * disturbanceScaling + float2(17.0f, -31.0f)),
        SignedValueNoise(uv * disturbanceScaling + float2(-29.0f, 41.0f))) * disturbanceIntensity;

    float terrainHeight =
        (1.0f - saturate(WorleyNoise((uv + disturbance) * baseTerrainScaling))) *
        baseTerrainHeightFactor * steep +
        baseTerrainBias;

    float fractalWeight = 1.0f;
    float fractalScaling = 2.0f * detailScale;
    const float fractalHeightWeight = (steep + minFractalHeightWeight) * fractalHeightWeightIntensity;
    [unroll]
    for (uint index = 0; index < fractalCount; ++index)
    {
        fractalWeight *= 0.5f;
        fractalScaling *= 2.0f;
        terrainHeight +=
            SignedValueNoise(uv * fractalScaling + float2(13.0f + index * 7.0f, -17.0f - index * 5.0f)) *
            fractalWeight *
            baseTerrainScaling *
            fractalHeightWeight;
    }

    float2 riftDomain = uv + disturbance * riftDisturbanceIntensity;
    float riftMask = smoothstep(
        0.0f,
        riftSoftness,
        ValueNoise(riftDomain * riftMaskScaling + float2(23.0f, -5.0f)) - riftLimit);
    float riftHeight = -smoothstep(
        0.0f,
        riftSoftness,
        riftWidth - abs(SignedValueNoise(riftDomain * riftScaling + float2(-19.0f, 37.0f)))) * riftDepth;
    riftHeight *= max(0.4f - steep, 0.0f) * riftMask;
    terrainHeight += riftHeight;

    float ridge =
        (0.5f - abs(SignedValueNoise((uv + disturbance * 0.5f) * ridgeScaling + float2(7.0f, -13.0f)))) *
        ridgeAmplitude;
    float ridgeMask = max(
        smoothstep(
            0.0f,
            ridgeSoftness,
            (ValueNoise(uv * ridgeMaskScaling + float2(-41.0f, 29.0f)) - ridgeLimit) * steep) -
            ridgeTransparency,
        0.0f);
    terrainHeight = lerp(terrainHeight, ridge, ridgeMask);

    terrainHeight += SignedValueNoise(uv * trendScaling + float2(5.0f, -11.0f)) * trendAmplitude;

    float centerPlatform = saturate(length(worldXZ) * 0.01f);
    terrainHeight = lerp(0.0f, terrainHeight, centerPlatform);

    float2 lakeDelta = worldXZ - float2(0.0f, g_LakeCenterZ);
    float2 lakeRadii = max(float2(g_LakeRadiusX, g_LakeRadiusZ), 1.0f.xx);
    float lakeMask = saturate(1.0f - length(lakeDelta / lakeRadii));
    terrainHeight -= pow(lakeMask, 2.0f) * g_LakeDepth;

    return terrainHeight;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= g_Width || dispatchThreadId.y >= g_Height)
    {
        return;
    }

    float2 worldXZ = float2(
        (float)dispatchThreadId.x * g_CellSizeX - g_FieldWidth * 0.5f,
        (float)dispatchThreadId.y * g_CellSizeZ - g_FieldDepth * 0.5f);

    g_TerrainHeight[dispatchThreadId.xy] = GenerateTerrainHeight(worldXZ);
}
