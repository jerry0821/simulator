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
    uint g_HasAuthoredHeightMap;
    uint g_Width;
    uint g_Height;
    float g_Padding0;
    float g_Padding1;
};

Texture2D g_AuthoredHeightMap : register(t0);
SamplerState g_TerrainSampler : register(s0);
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
    float nx = worldXZ.x * g_BaseFrequency;
    float nz = worldXZ.y * g_BaseFrequency;
    float2 coarseWarp = DomainWarp(float2(nx, nz), 0.65f, 1.35f, float2(13.0f, -17.0f));
    float2 fineWarp = DomainWarp(float2(nx, nz) + coarseWarp, 1.55f, 0.45f, float2(-29.0f, 7.0f));
    float2 domain = float2(nx, nz) + coarseWarp + fineWarp;

    float worleyBase = 1.0f - saturate(WorleyNoise(domain * 2.8f));

    float steepWeight = lerp(
        1.10f,
        3.05f,
        ValueNoise(floor(domain * 1.65f) + float2(19.0f, -23.0f)));
    float weightedMass = pow(saturate(worleyBase), steepWeight);

    float dissolution = Fbm(domain * 1.75f + float2(23.0f, -11.0f), 4);
    float dissolvedMass = weightedMass * lerp(0.42f, 1.26f, dissolution);

    float fractalSimplex = Fbm(
        domain * (g_DetailFrequency * 0.95f) + float2(31.0f, -17.0f),
        5);

    float riftSource = RidgedFbm(domain * float2(0.42f, 0.36f) + float2(-17.0f, 29.0f), 4);
    float riftMask = pow(1.0f - saturate(riftSource), 1.65f);

    float ridgeBase = saturate(RidgedFbm(
        domain * g_RidgeFrequency + float2(7.0f, -13.0f),
        5));
    float ridgeMask = pow(ridgeBase, 2.25f);

    const float2 rangeCenterA = float2(-95.0f, 26.0f);
    const float2 rangeCenterB = float2(82.0f, 118.0f);
    float rangeA =
        1.0f - saturate(length(float2(
            (worldXZ.x - rangeCenterA.x) / 288.0f,
            (worldXZ.y - rangeCenterA.y) / 228.0f)));
    float rangeB =
        1.0f - saturate(length(float2(
            (worldXZ.x - rangeCenterB.x) / 268.0f,
            (worldXZ.y - rangeCenterB.y) / 214.0f)));
    float globalTrend = max(pow(rangeA, 2.35f), pow(rangeB, 2.85f));
    float massifMacro = globalTrend;

    float rollingBase = Fbm(worldXZ * 0.0045f + float2(7.0f, -5.0f), 4);
    float macroWarpDetail = Fbm(worldXZ * 0.010f + float2(-9.0f, 14.0f), 3) - 0.5f;

    const float2 heroPeak = float2(46.0f, 118.0f);
    const float2 spineStart = float2(-68.0f, 48.0f);
    const float2 spineEnd = float2(128.0f, 136.0f);
    float2 spine = spineEnd - spineStart;
    float spineLengthSq = max(dot(spine, spine), 1.0f);
    float spineLength = sqrt(spineLengthSq);
    float2 spineDir = spine / spineLength;
    float2 spineNormal = float2(-spineDir.y, spineDir.x);
    float spineT = saturate(dot(worldXZ - spineStart, spine) / spineLengthSq);
    float2 nearestSpine = spineStart + spine * spineT;
    float spineDistance = length(worldXZ - nearestSpine);
    float spineProgress = Smoothstep01(spineT);
    float spineWidth = lerp(148.0f, 86.0f, spineProgress);
    float spineShoulderMask = pow(saturate(1.0f - spineDistance / spineWidth), 1.42f);
    float spineCoreWidth = spineWidth * lerp(0.54f, 0.40f, spineProgress);
    float spineCoreMask = pow(saturate(1.0f - spineDistance / spineCoreWidth), 2.85f);
    float mountainSpineMask = max(
        spineShoulderMask * lerp(0.78f, 1.08f, spineProgress),
        spineCoreMask * lerp(0.22f, 1.12f, pow(RemapClamped(spineProgress, 0.35f, 1.0f), 1.45f)));

    float2 heroDelta = worldXZ - heroPeak;
    float heroParallel = dot(heroDelta, spineDir);
    float heroPerpendicular = dot(heroDelta, spineNormal);
    float heroShoulderDistance = length(float2(heroParallel / 170.0f, heroPerpendicular / 96.0f));
    float heroPeakMask = pow(saturate(1.0f - heroShoulderDistance), 2.10f);
    float heroCoreDistance = length(float2(heroParallel / 102.0f, heroPerpendicular / 58.0f));
    float heroPeakCore = pow(saturate(1.0f - heroCoreDistance), 3.40f);
    float summitCluster = max(max(heroPeakMask, heroPeakCore), mountainSpineMask);

    float massifBody = pow(saturate(dissolvedMass * massifMacro), 1.18f);
    float massifPeak = pow(saturate(dissolvedMass * massifMacro), 5.45f);
    float shoulderNoise =
        (fractalSimplex - 0.5f) *
        lerp(0.04f, 0.62f, massifMacro) *
        lerp(1.0f, 0.55f, heroPeakCore);
    float summitVariation =
        (ValueNoise(worldXZ * 0.016f + float2(31.0f, -13.0f)) - 0.5f) * massifPeak;
    float ridgeFocus = pow(massifMacro, 1.30f);
    float alpineUplift = pow(saturate(massifMacro), 2.55f);
    float heroUplift = pow(max(heroPeakCore, summitCluster * 0.82f), 1.10f);

    const float2 valleyAxisDir = normalize(float2(0.86f, 0.51f));
    const float2 valleyAxisNormal = float2(-valleyAxisDir.y, valleyAxisDir.x);
    float valleyWarpPrimary =
        (Fbm(worldXZ * 0.0055f + float2(-17.0f, 29.0f), 4) - 0.5f) * 96.0f;
    float valleyWarpSecondary =
        (Fbm(worldXZ * 0.0115f + float2(21.0f, -41.0f), 3) - 0.5f) * 34.0f;
    float trunkValleyDistance = abs(dot(worldXZ, valleyAxisNormal) + valleyWarpPrimary + valleyWarpSecondary);
    float trunkValleyWidth = lerp(148.0f, 62.0f, saturate(massifMacro));
    float trunkValley = pow(saturate(1.0f - trunkValleyDistance / trunkValleyWidth), 2.15f);

    const float2 branchAxisDir = normalize(float2(-0.38f, 0.92f));
    const float2 branchAxisNormal = float2(-branchAxisDir.y, branchAxisDir.x);
    float branchWave =
        sin(dot(worldXZ, branchAxisDir) * 0.014f + 1.7f) * 36.0f +
        (Fbm(worldXZ * 0.0095f + float2(43.0f, -9.0f), 3) - 0.5f) * 42.0f;
    float branchValleyDistance = abs(dot(worldXZ - float2(14.0f, 22.0f), branchAxisNormal) + branchWave);
    float branchValley = pow(saturate(1.0f - branchValleyDistance / 96.0f), 2.45f);

    float lowlandNoise = Fbm(worldXZ * 0.0068f + float2(-33.0f, 15.0f), 4) - 0.5f;
    float valleyCarveMask = saturate(max(trunkValley, branchValley * 0.62f) * (1.0f - saturate(heroPeakCore * 0.85f + ridgeMask * 0.28f)));
    float wetlandMask = saturate((1.0f - massifMacro) * 0.95f + trunkValley * 0.38f - ridgeMask * 0.24f);

    float height = (rollingBase - 0.5f) * (g_BaseHeight * 0.10f);
    height += macroWarpDetail * g_DetailHeight * 0.18f;
    height += massifBody * (g_BaseHeight * 2.00f);
    height += (fractalSimplex - 0.5f) * g_DetailHeight * lerp(0.05f, 0.68f, massifMacro);
    height += ridgeMask * ridgeFocus * (g_RidgeHeight * 1.38f);
    height -= riftMask * ridgeFocus * (g_RidgeHeight * 0.88f);
    height += massifPeak * (g_RidgeHeight * 2.20f);
    height += alpineUplift * (g_BaseHeight * 0.88f);
    height += spineShoulderMask * lerp(0.55f, 0.92f, spineProgress) * (g_BaseHeight * 1.05f);
    height += spineCoreMask * lerp(0.15f, 1.00f, spineProgress) * (g_RidgeHeight * 1.10f);
    height += heroPeakMask * (g_RidgeHeight * 1.55f);
    height += heroPeakCore * ((g_RidgeHeight * 1.70f) + (g_BaseHeight * 0.35f));
    height += heroUplift * ridgeMask * (g_RidgeHeight * 1.05f);
    height += shoulderNoise * g_DetailHeight * 0.72f;
    height += summitVariation * g_DetailHeight * 0.68f;
    height -= valleyCarveMask * (g_BaseHeight * 0.88f + g_ContinentHeight * 0.56f);
    height += lowlandNoise * g_DetailHeight * 0.32f * wetlandMask;
    height -= wetlandMask * (g_BaseHeight * 0.10f);
    height += massifMacro * g_ContinentHeight * 0.90f;

    return height;
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
