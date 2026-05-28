#include "application.h"

#include "debug_menu.h"
#include "meshfield.h"
#include "terrain_data_model.h"
#include "terrain_height_field.h"

Backend::RenderShaderResource Application::BaseTerrainHeightResource() const
{
	return TerrainHeightField::HeightResource();
}

Backend::RenderShaderResource Application::SimulationTerrainHeightResource() const
{
	if (UsingSharedTerrainWaterHeightfield())
	{
		return m_simulation.compute_terrain_data_texture.Resource();
	}

	return BaseTerrainHeightResource();
}

Backend::RenderShaderResource Application::ActiveTerrainHeightResource() const
{
	if (UsingSharedTerrainWaterHeightfield())
	{
		return m_simulation.compute_terrain_data_texture.Resource();
	}

	return TerrainDataModel::HeightResource();
}

Backend::RenderShaderResource Application::ActiveTerrainNormalResource() const
{
	return m_simulation.compute_terrain_data_texture.TerrainNormalResource();
}

bool Application::UsingSharedTerrainWaterHeightfield() const
{
	return
		m_simulation.use_shared_terrain_water_heightfield &&
		m_simulation.compute_terrain_data_texture.IsValid() &&
		m_simulation.compute_terrain_data_texture.Resource().isValid();
}

WaterSurfaceDesc Application::ResolveActiveWaterSurfaceDesc() const
{
	WaterSurfaceDesc water_surface_desc = DebugMenu_GetWaterSurfaceSettings();
	water_surface_desc.enabled = true;
	water_surface_desc.center_x = 0.0f;
	water_surface_desc.center_z = 0.0f;
	water_surface_desc.height = ResolveActiveWaterSurfaceHeight();
	water_surface_desc.size_x = TerrainHeightField::FieldWidth();
	water_surface_desc.size_z = TerrainHeightField::FieldDepth();
	if (water_surface_desc.base_color.w <= 0.0f)
	{
		water_surface_desc.base_color = { 0.06f, 0.22f, 0.46f, 0.36f };
		water_surface_desc.ripple_strength = 1.65f;
		water_surface_desc.edge_emphasis = 1.0f;
	}
	return water_surface_desc;
}

float Application::ResolveActiveWaterSurfaceHeight() const
{
	const float debug_water_height = DebugMenu_GetWaterSurfaceSettings().height;
	return debug_water_height > 0.0f ? debug_water_height : TerrainDataModel::SuggestedWaterHeight();
}

void Application::PublishSharedComputeResources()
{
	PublishComputeTaskResources({
		&m_simulation.compute_shared_resource_registry,
		&m_simulation.terrain_water_state,
		m_simulation.active_water_surface_height,
		m_simulation.active_water_surface_desc,
		BaseTerrainHeightResource(),
		ActiveTerrainHeightResource(),
		ActiveTerrainNormalResource(),
		&m_simulation.compute_terrain_data_texture,
		&m_simulation.compute_terrain_classification_texture,
		&m_simulation.compute_grass_data_texture,
		&m_simulation.compute_meteorograph_texture,
		&m_simulation.compute_noise_texture });
}
