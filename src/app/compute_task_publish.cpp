#include "compute_task_publish.h"

void PublishComputeTaskResources(const ComputeTaskPublishInputs& inputs)
{
	if (inputs.registry == nullptr ||
		inputs.terrain_water_state == nullptr ||
		inputs.water == nullptr ||
		inputs.classification == nullptr ||
		inputs.grass_data == nullptr ||
		inputs.meteorograph == nullptr ||
		inputs.noise == nullptr)
	{
		return;
	}

	inputs.registry->Clear();
	inputs.registry->PublishShaderResource(
		ComputeSharedResourceId::BaseTerrainHeight,
		"BaseTerrainHeight",
		inputs.base_terrain_height);

	inputs.terrain_water_state->Update(
		inputs.active_water_surface_height,
		inputs.active_water_surface_desc,
		inputs.active_terrain_height,
		inputs.active_terrain_normal,
		inputs.water->TerrainSurfaceDataResource(),
		inputs.water->Resource(),
		inputs.water->FlowResource(),
		inputs.water->VelocityResource(),
		inputs.water->SedimentResource());
	inputs.terrain_water_state->PublishTo(*inputs.registry);

	inputs.registry->PublishShaderResource(
		ComputeSharedResourceId::TerrainVegetationSuitability,
		"TerrainVegetationSuitability",
		inputs.classification->VegetationSuitabilityResource());
	inputs.registry->PublishShaderResource(
		ComputeSharedResourceId::GrassData,
		"GrassData",
		inputs.grass_data->Resource());
	inputs.registry->PublishShaderResource(
		ComputeSharedResourceId::RainMap,
		"RainMap",
		inputs.meteorograph->RainResource());
	inputs.registry->PublishShaderResource(
		ComputeSharedResourceId::AtmospherePreview,
		"AtmospherePreview",
		inputs.meteorograph->PreviewResource());
	inputs.registry->PublishShaderResource(
		ComputeSharedResourceId::ComputeNoise,
		"ComputeNoise",
		inputs.noise->Resource());
	inputs.registry->PublishShaderResource(
		ComputeSharedResourceId::MeteorographField,
		"MeteorographField",
		inputs.meteorograph->Resource());
}
