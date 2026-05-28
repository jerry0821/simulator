#ifndef RENDERER_FRAME_BUILDER_H
#define RENDERER_FRAME_BUILDER_H

#include "compute_floating_light_points.h"
#include "compute_grass_data_texture.h"
#include "compute_shared_resource_registry.h"
#include "compute_terrain_classification_texture.h"
#include "compute_terrain_data_texture.h"
#include "render_frame_context.h"
#include "render_scene.h"
#include "terrain_water_state.h"

struct RendererFrameBuildInputs
{
	RenderScene* render_scene = nullptr;
	const TerrainWaterState* terrain_water_state = nullptr;
	const ComputeSharedResourceRegistry* shared_registry = nullptr;
	const ComputeTerrainDataTexture* water = nullptr;
	const ComputeTerrainClassificationTexture* classification = nullptr;
	const ComputeGrassDataTexture* grass_data = nullptr;
	const ComputeFloatingLightPoints* floating_lights = nullptr;
	Backend::RenderShaderResource active_terrain_height{};
	bool using_shared_terrain_water_heightfield = false;
	float active_water_surface_height = 0.0f;
	float glitch_amount = 0.0f;
	double current_time = 0.0;
	double elapsed_time = 0.0;
};

RenderFrameContext BuildRendererFrameContext(const RendererFrameBuildInputs& inputs);

#endif // RENDERER_FRAME_BUILDER_H
