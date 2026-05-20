#ifndef APPLICATION_H
#define APPLICATION_H

#include <memory>

#include <Windows.h>

#include "debug_text.h"
#include "compute_climate_texture.h"
#include "compute_floating_light_points.h"
#include "compute_grass_data_texture.h"
#include "compute_meteorograph_texture.h"
#include "compute_noise_texture.h"
#include "compute_shared_resource_registry.h"
#include "compute_task_runner.h"
#include "compute_terrain_classification_texture.h"
#include "compute_terrain_normal_texture.h"
#include "compute_water_surface_height_texture.h"
#include "render_backend_dx11.h"
#include "render_water_surface.h"
#include "renderer.h"
#include "scene.h"
#include "scene_render_adapter.h"
#include "terrain_water_state.h"

class Application
{
public:
	Application() = default;
	~Application() = default;

	bool Initialize(HINSTANCE instance_handle, int show_command);
	int Run();
	void Shutdown();

private:
	bool InitializePlatform(HINSTANCE instance_handle, int show_command);
	bool InitializeEngineSystems();
	void InitializeRuntimeObjects();
	void InitializeDebugTools();
	void ResetFrameState();
	void BeginFrame(double current_time, double elapsed_time);
	Backend::RenderShaderResource BaseTerrainHeightResource() const;
	Backend::RenderShaderResource SimulationTerrainHeightResource() const;
	Backend::RenderShaderResource ActiveTerrainHeightResource() const;
	Backend::RenderShaderResource ActiveTerrainNormalResource() const;
	bool UsingSharedTerrainWaterHeightfield() const;
	WaterSurfaceDesc ResolveActiveWaterSurfaceDesc() const;
	float ResolveActiveWaterSurfaceHeight() const;
	void PublishSharedComputeResources();
	void RenderDebugText();
	RenderFrameContext BuildFrameContext(double current_time, double elapsed_time) const;
	void FinalizeEngineSystems();

	void UpdateFps(double current_time);
	void TickFrame(double current_time);

	HWND m_window_handle = nullptr;
	RenderBackendDX11 m_render_backend;
	std::unique_ptr<Renderer> m_renderer;
	std::unique_ptr<SceneRenderAdapter> m_scene_render_adapter;
	SceneController m_scene_controller{};
	ComputeTaskRunner m_compute_task_runner{};
	ComputeClimateTexture m_compute_climate_texture{};
	ComputeFloatingLightPoints m_compute_floating_light_points{};
	ComputeGrassDataTexture m_compute_grass_data_texture{};
	ComputeMeteorographTexture m_compute_meteorograph_texture{};
	ComputeNoiseTexture m_compute_noise_texture{};
	ComputeSharedResourceRegistry m_compute_shared_resource_registry{};
	ComputeTerrainClassificationTexture m_compute_terrain_classification_texture{};
	ComputeTerrainNormalTexture m_compute_terrain_normal_texture{};
	ComputeWaterSurfaceHeightTexture m_compute_water_surface_height_texture{};
	TerrainWaterState m_terrain_water_state{};

#if defined(DEBUG) || defined(_DEBUG)
	std::unique_ptr<hal::DebugText> m_debug_text;
#endif

	double m_exec_last_time = 0.0;
	double m_fps_last_time = 0.0;
	double m_fps = 0.0;
	double m_terrain_normal_last_update_time = -1000.0;
	double m_terrain_classification_last_update_time = -1000.0;
	unsigned long m_frame_count = 0;
	WaterSurfaceDesc m_active_water_surface_desc{};
	float m_active_water_surface_height = 0.0f;
	float m_glitch_amount = 0.0f;
	TerrainMaterialSettings m_last_terrain_material_settings{};
	bool m_has_last_terrain_material_settings = false;
	bool m_use_shared_terrain_water_heightfield = true;
	bool m_pending_surface_water_reset = false;
};

#endif // APPLICATION_H
