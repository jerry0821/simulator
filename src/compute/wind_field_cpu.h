#ifndef WIND_FIELD_CPU_H
#define WIND_FIELD_CPU_H

#include <DirectXMath.h>

#include "compute_texture_dimensions.h"

struct ComputeNoiseSettings;

DirectX::XMFLOAT3 WindFieldCpu_SampleWorld(
    float world_x,
    float world_z,
    float time_seconds,
    const ComputeNoiseSettings& settings,
    float world_min_x = -ComputeTextureDimensions::kWorldHalfExtent,
    float world_max_x = ComputeTextureDimensions::kWorldHalfExtent,
    float world_min_z = -ComputeTextureDimensions::kWorldHalfExtent,
    float world_max_z = ComputeTextureDimensions::kWorldHalfExtent);

#endif // WIND_FIELD_CPU_H
