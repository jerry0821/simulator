#include "renderer_frame_builder.h"

#include <algorithm>
#include <cmath>

#include "camera.h"
#include "compute_shared_resource_registry.h"
#include "compute_texture_dimensions.h"
#include "direct3d.h"
#include "terrain_height_field.h"

namespace
{
bool ComputeBaseTerrainHeightRange(float& out_min_height, float& out_max_height)
{
	constexpr unsigned int kSampleResolution = 257;
	const float field_width = TerrainHeightField::FieldWidth();
	const float field_depth = TerrainHeightField::FieldDepth();
	if (field_width <= 0.0f || field_depth <= 0.0f)
	{
		return false;
	}

	bool has_sample = false;
	float min_height = 0.0f;
	float max_height = 0.0f;
	for (unsigned int row = 0; row < kSampleResolution; ++row)
	{
		const float v = static_cast<float>(row) / static_cast<float>(kSampleResolution - 1);
		const float world_z = std::lerp(-field_depth * 0.5f, field_depth * 0.5f, v);
		for (unsigned int col = 0; col < kSampleResolution; ++col)
		{
			const float u = static_cast<float>(col) / static_cast<float>(kSampleResolution - 1);
			const float world_x = std::lerp(-field_width * 0.5f, field_width * 0.5f, u);
			const float height = TerrainHeightField::GetHeight(world_x, world_z);
			if (!has_sample)
			{
				min_height = height;
				max_height = height;
				has_sample = true;
			}
			else
			{
				min_height = std::min(min_height, height);
				max_height = std::max(max_height, height);
			}
		}
	}

	if (!has_sample)
	{
		return false;
	}

	out_min_height = min_height;
	out_max_height = max_height;
	return true;
}
}

RenderFrameContext BuildRendererFrameContext(const RendererFrameBuildInputs& inputs)
{
	RenderFrameContext frame_context{};
	frame_context.render_scene = inputs.render_scene;
	frame_context.globals.view_matrix = Camera_GetMatrix();
	frame_context.globals.projection_matrix = Camera_GetPerspectiveMatrix();
	frame_context.globals.camera_position = Camera_GetPosition();
	frame_context.globals.backbuffer_width = Direct3D_GetBackBufferWidth();
	frame_context.globals.backbuffer_height = Direct3D_GetBackBufferHeight();
	frame_context.globals.glitch_amount = inputs.glitch_amount;
	frame_context.globals.time_seconds = inputs.current_time;
	frame_context.globals.elapsed_time = inputs.elapsed_time;

	if (inputs.terrain_water_state != nullptr)
	{
		frame_context.resources.terrain_water = inputs.terrain_water_state->FrameState();
	}

	if (inputs.shared_registry != nullptr)
	{
		frame_context.resources.base_terrain_height =
			inputs.shared_registry->GetShaderResource(ComputeSharedResourceId::BaseTerrainHeight);
		frame_context.resources.rain_map =
			inputs.shared_registry->GetShaderResource(ComputeSharedResourceId::RainMap);
		frame_context.resources.atmosphere_preview =
			inputs.shared_registry->GetShaderResource(ComputeSharedResourceId::AtmospherePreview);
		frame_context.resources.compute_noise =
			inputs.shared_registry->GetShaderResource(ComputeSharedResourceId::ComputeNoise);
		frame_context.resources.meteorograph_field =
			inputs.shared_registry->GetShaderResource(ComputeSharedResourceId::MeteorographField);
		frame_context.resources.compute_shared_registry =
			const_cast<ComputeSharedResourceRegistry*>(inputs.shared_registry);
	}

	frame_context.resources.has_base_terrain_range = ComputeBaseTerrainHeightRange(
		frame_context.resources.base_terrain_min_height,
		frame_context.resources.base_terrain_max_height);

	frame_context.resources.terrain_height = inputs.active_terrain_height;

	if (inputs.water != nullptr)
	{
		frame_context.resources.has_terrain_heightfield_range =
			inputs.water->ComputeTerrainHeightRange(
				frame_context.resources.terrain_heightfield_min_height,
				frame_context.resources.terrain_heightfield_max_height);
		frame_context.resources.has_water_heightfield_range =
			inputs.water->ComputeWaterHeightRange(
				frame_context.resources.water_heightfield_min_height,
				frame_context.resources.water_heightfield_max_height);
		frame_context.resources.terrain_heightfield_range_is_fallback = false;
		frame_context.resources.water_heightfield_range_is_fallback = false;
	}

	if (!frame_context.resources.has_terrain_heightfield_range &&
		frame_context.resources.has_base_terrain_range)
	{
		frame_context.resources.has_terrain_heightfield_range = true;
		frame_context.resources.terrain_heightfield_range_is_fallback = true;
		frame_context.resources.terrain_heightfield_min_height =
			frame_context.resources.base_terrain_min_height;
		frame_context.resources.terrain_heightfield_max_height =
			frame_context.resources.base_terrain_max_height;
	}

	if (!frame_context.resources.has_water_heightfield_range &&
		frame_context.resources.has_base_terrain_range)
	{
		frame_context.resources.has_water_heightfield_range = true;
		frame_context.resources.water_heightfield_range_is_fallback = true;
		frame_context.resources.water_heightfield_min_height =
			std::min(
				frame_context.resources.base_terrain_min_height,
				inputs.active_water_surface_height);
		frame_context.resources.water_heightfield_max_height =
			std::max(
				frame_context.resources.base_terrain_max_height,
				inputs.active_water_surface_height);
	}

	if (inputs.using_shared_terrain_water_heightfield &&
		frame_context.resources.has_terrain_heightfield_range)
	{
		frame_context.resources.has_active_terrain_range = true;
		frame_context.resources.active_terrain_min_height =
			frame_context.resources.terrain_heightfield_min_height;
		frame_context.resources.active_terrain_max_height =
			frame_context.resources.terrain_heightfield_max_height;
	}
	else
	{
		frame_context.resources.has_active_terrain_range = frame_context.resources.has_base_terrain_range;
		frame_context.resources.active_terrain_min_height = frame_context.resources.base_terrain_min_height;
		frame_context.resources.active_terrain_max_height = frame_context.resources.base_terrain_max_height;
	}

	frame_context.resources.terrain_normal = frame_context.resources.terrain_water.terrain_normal;
	frame_context.resources.terrain_surface_data =
		frame_context.resources.terrain_water.terrain_surface_data;

	if (inputs.classification != nullptr)
	{
		frame_context.resources.terrain_vegetation_suitability =
			inputs.classification->VegetationSuitabilityResource();
	}
	if (inputs.grass_data != nullptr)
	{
		frame_context.resources.grass_data = inputs.grass_data->Resource();
	}

	frame_context.resources.water_surface_height =
		frame_context.resources.terrain_water.water_surface_height_texture;
	frame_context.resources.surface_water_flow =
		frame_context.resources.terrain_water.surface_water_flow;
	frame_context.resources.water_velocity =
		frame_context.resources.terrain_water.water_velocity;
	frame_context.resources.water_sediment =
		frame_context.resources.terrain_water.water_sediment;
	frame_context.resources.water_interaction_data =
		frame_context.resources.terrain_water.water_interaction_data;
	frame_context.resources.soil_moisture =
		frame_context.resources.terrain_water.soil_moisture;
	frame_context.resources.erosion_delta =
		frame_context.resources.terrain_water.erosion_delta;

	if (inputs.floating_lights != nullptr)
	{
		frame_context.resources.floating_light_instance_buffer =
			inputs.floating_lights->InstanceBuffer();
		frame_context.resources.floating_light_instance_count =
			inputs.floating_lights->InstanceCount();
	}

	return frame_context;
}
