#ifndef COMPUTE_TASK_PUBLISH_H
#define COMPUTE_TASK_PUBLISH_H

#include "compute_grass_data_texture.h"
#include "compute_meteorograph_texture.h"
#include "compute_noise_texture.h"
#include "compute_shared_resource_registry.h"
#include "compute_terrain_classification_texture.h"
#include "compute_terrain_data_texture.h"
#include "render_water_surface.h"
#include "terrain_water_state.h"

struct ComputeTaskPublishInputs
{
	ComputeSharedResourceRegistry* registry = nullptr;
	TerrainWaterState* terrain_water_state = nullptr;
	float active_water_surface_height = 0.0f;
	WaterSurfaceDesc active_water_surface_desc{};
	Backend::RenderShaderResource base_terrain_height{};
	Backend::RenderShaderResource active_terrain_height{};
	Backend::RenderShaderResource active_terrain_normal{};
	const ComputeTerrainDataTexture* water = nullptr;
	const ComputeTerrainClassificationTexture* classification = nullptr;
	const ComputeGrassDataTexture* grass_data = nullptr;
	const ComputeMeteorographTexture* meteorograph = nullptr;
	const ComputeNoiseTexture* noise = nullptr;
};

void PublishComputeTaskResources(const ComputeTaskPublishInputs& inputs);

#endif // COMPUTE_TASK_PUBLISH_H
