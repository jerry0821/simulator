#ifndef COMPUTE_TASK_SYSTEM_STATE_H
#define COMPUTE_TASK_SYSTEM_STATE_H

#include "compute_floating_light_points.h"
#include "compute_grass_data_texture.h"
#include "compute_meteorograph_texture.h"
#include "compute_noise_texture.h"
#include "compute_shared_resource_registry.h"
#include "compute_task_runner.h"
#include "compute_terrain_classification_texture.h"
#include "compute_water_surface_height_texture.h"
#include "render_water_surface.h"
#include "terrain_surface_settings.h"
#include "terrain_water_state.h"

struct ComputeTaskSystemState
{
	ComputeTaskRunner compute_task_runner{};
	ComputeFloatingLightPoints compute_floating_light_points{};
	ComputeGrassDataTexture compute_grass_data_texture{};
	ComputeMeteorographTexture compute_meteorograph_texture{};
	ComputeNoiseTexture compute_noise_texture{};
	ComputeSharedResourceRegistry compute_shared_resource_registry{};
	ComputeTerrainClassificationTexture compute_terrain_classification_texture{};
	ComputeWaterSurfaceHeightTexture compute_water_surface_height_texture{};
	TerrainWaterState terrain_water_state{};

	double terrain_classification_last_update_time = -1000.0;
	WaterSurfaceDesc active_water_surface_desc{};
	float active_water_surface_height = 0.0f;
	TerrainMaterialSettings last_terrain_material_settings{};
	bool has_last_terrain_material_settings = false;
	bool use_shared_terrain_water_heightfield = true;
	bool pending_surface_water_reset = false;
};

#endif // COMPUTE_TASK_SYSTEM_STATE_H
