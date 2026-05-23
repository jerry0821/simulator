#include "application.h"

#include <algorithm>

#include "debug_menu.h"
#include "frustum_culling_debug.h"
#include "instancing_debug.h"
#include "key_logger.h"
#include "meshfield.h"
#include "pad_logger.h"
#include "shader_field.h"
#include "sprite.h"
#include "sprite_anim.h"
#include "terrain_data_model.h"
#include "terrain_height_field.h"

void Application::BeginFrame(double current_time, double elapsed_time)
{
	KeyLogger_Update();
	PadLogger_Update();
	DebugMenu_Begin();
	InstancingDebug_ResetStats();
	FrustumCullingDebug_ResetStats();
	m_scene_controller.Update(elapsed_time);
	SpriteAnim_Update(elapsed_time);
	TerrainHeightField::ApplyTerrainSettings(DebugMenu_GetTerrainSettings());
	m_simulation.pending_surface_water_reset = DebugMenu_ConsumeSurfaceWaterResetRequest();

	if (KeyLogger_IsTrigger(KK_G))
	{
		m_glitch_amount = 0.2f;
	}
	else
	{
		m_glitch_amount = std::max(0.0f, m_glitch_amount - static_cast<float>(elapsed_time) * 0.8f);
	}

	if (KeyLogger_IsTrigger(KK_F5) || DebugMenu_ConsumeShaderReloadRequest())
	{
		ReloadComputeResources();
	}

	m_simulation.active_water_surface_desc = ResolveActiveWaterSurfaceDesc();
	m_simulation.active_water_surface_height = m_simulation.active_water_surface_desc.height;
	DispatchComputeFrame(current_time, elapsed_time);
	TerrainMaterialSettings terrain_material_settings = DebugMenu_GetTerrainMaterialSettings();
	terrain_material_settings.water_height = m_simulation.active_water_surface_height;
	TerrainDataModel::SetMaterialSettings(terrain_material_settings);
	MeshFieldRenderer::SetRenderHeightSRV(
		ActiveTerrainHeightResource().shaderResourceView());
	MeshFieldRenderer::SetRenderNormalSRV(ActiveTerrainNormalResource().shaderResourceView());
	PublishSharedComputeResources();
	ShaderField_SetTerrainMaterialSettings(terrain_material_settings);
	Sprite_Begin();
}
