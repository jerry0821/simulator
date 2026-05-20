#ifndef COMPUTE_TEXTURE_DIMENSIONS_H
#define COMPUTE_TEXTURE_DIMENSIONS_H

namespace ComputeTextureDimensions
{
// Hydrology simulation texture resolution (1024px covers 2048m world at 2m/px)
constexpr unsigned int kHydrologyResolution = 1024u;

// World size constants (mirrors WorldDefinitions in shaders)
constexpr float kWorldSideLength = 2048.0f;
constexpr float kWorldCenterOffset = -1024.0f;  // = worldSideLength * -0.5
constexpr float kDataInterval = kWorldSideLength / static_cast<float>(kHydrologyResolution); // 2.0f
}

#endif // COMPUTE_TEXTURE_DIMENSIONS_H
