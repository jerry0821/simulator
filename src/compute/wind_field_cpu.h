#ifndef WIND_FIELD_CPU_H
#define WIND_FIELD_CPU_H

#include <DirectXMath.h>

struct ComputeNoiseSettings;

DirectX::XMFLOAT3 WindFieldCpu_SampleWorld(
    float world_x,
    float world_z,
    float time_seconds,
    const ComputeNoiseSettings& settings,
    float world_min_x = -640.0f,
    float world_max_x = 640.0f,
    float world_min_z = -640.0f,
    float world_max_z = 640.0f);

#endif // WIND_FIELD_CPU_H
