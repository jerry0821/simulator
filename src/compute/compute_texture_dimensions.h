#ifndef COMPUTE_TEXTURE_DIMENSIONS_H
#define COMPUTE_TEXTURE_DIMENSIONS_H

namespace ComputeTextureDimensions
{
constexpr float kWorldSideLength = 2048.0f;
constexpr float kWorldHalfExtent = kWorldSideLength * 0.5f;
constexpr unsigned int kTerrainMeshResolution = 256u;
constexpr unsigned int kTerrainHeightResolution = 4096u;
constexpr unsigned int kHydrologyResolution = 1024u;
}

#endif // COMPUTE_TEXTURE_DIMENSIONS_H
