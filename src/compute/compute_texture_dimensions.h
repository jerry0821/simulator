#ifndef COMPUTE_TEXTURE_DIMENSIONS_H
#define COMPUTE_TEXTURE_DIMENSIONS_H

namespace ComputeTextureDimensions
{
constexpr float kWorldSideLength = 2048.0f;
constexpr float kWorldHalfExtent = kWorldSideLength * 0.5f;
constexpr unsigned int kTerrainMeshResolution = 256u;
constexpr unsigned int kWaterMeshResolution = 256u;
constexpr float kTerrainMeshInterval = 4.0f;
constexpr float kWaterMeshInterval = 4.0f;
constexpr float kTerrainPatchCoverage =
	(static_cast<float>(kTerrainMeshResolution) * kTerrainMeshInterval) / kWorldSideLength;
constexpr float kWaterPatchCoverage =
	(static_cast<float>(kWaterMeshResolution) * kWaterMeshInterval) / kWorldSideLength;
constexpr unsigned int kTerrainHeightResolution = 4096u;
constexpr unsigned int kWaterDataResolution = 4096u;
constexpr unsigned int kTerrainClassificationResolution = 4096u;
constexpr unsigned int kGrassDataResolution = 2048u;
constexpr unsigned int kHydrologyResolution = kWaterDataResolution;
}

#endif // COMPUTE_TEXTURE_DIMENSIONS_H
