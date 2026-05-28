#include "application.h"

void Application::RenderCurrentFrame(double current_time, double elapsed_time)
{
	const RenderFrameContext frame_context = BuildRendererFrameContext({
		m_scene_render_adapter.get(),
		&m_simulation.terrain_water_state,
		&m_simulation.compute_shared_resource_registry,
		&m_simulation.compute_terrain_data_texture,
		&m_simulation.compute_terrain_classification_texture,
		&m_simulation.compute_grass_data_texture,
		&m_simulation.compute_floating_light_points,
		ActiveTerrainHeightResource(),
		UsingSharedTerrainWaterHeightfield(),
		m_simulation.active_water_surface_height,
		m_glitch_amount,
		current_time,
		elapsed_time });
	m_renderer->RenderFrame(frame_context);
}
