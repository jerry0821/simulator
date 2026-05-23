#include "application.h"

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "debug_menu.h"
#include "direct3d.h"
#include "game_window.h"
#include "meshfield.h"
#include "renderer.h"
#include "scene_render_adapter.h"

bool Application::InitializePlatform(HINSTANCE instance_handle, int show_command)
{
	static_cast<void>(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	m_window_handle = GameWindow_Create(instance_handle);
	if (m_window_handle == nullptr)
	{
		return false;
	}

	ShowWindow(m_window_handle, show_command);
	UpdateWindow(m_window_handle);
	return true;
}

bool Application::InitializeEngineSystems()
{
	if (!RuntimeSystem::Initialize(m_window_handle))
	{
		return false;
	}

	ConfigureComputeTasks();
	return InitializeComputeResources();
}

void Application::InitializeRuntimeObjects()
{
	m_renderer = std::make_unique<Renderer>(m_render_backend);
	m_scene_controller.Initialize();
	m_scene_render_adapter = std::make_unique<SceneRenderAdapter>(m_scene_controller);
}

void Application::InitializeDebugTools()
{
	DebugMenu_Initialize(Direct3D_GetDevice(), Direct3D_GetContext(), m_window_handle);
	DebugMenu_SetFramePlan(m_renderer->BuildFramePlan());

#if defined(DEBUG) || defined(_DEBUG)
	m_debug_text = std::make_unique<hal::DebugText>(
		Direct3D_GetDevice(),
		Direct3D_GetContext(),
		L"resource/texture/consolab_ascii_512.png",
		Direct3D_GetBackBufferWidth(),
		Direct3D_GetBackBufferHeight(),
		0.0f,
		0.0f,
		0,
		0,
		0.0f,
		14.0f);
#endif
}

void Application::FinalizeEngineSystems()
{
	DebugMenu_Finalize();
	m_simulation.has_last_terrain_material_settings = false;
	m_simulation.terrain_classification_last_update_time = -1000.0;
	m_simulation.active_water_surface_desc = WaterSurfaceDesc{};
	m_simulation.active_water_surface_height = 0.0f;
	m_simulation.terrain_water_state.Clear();
	m_simulation.compute_terrain_classification_texture.Finalize();
	m_simulation.compute_grass_data_texture.Finalize();
	MeshFieldRenderer::SetRenderHeightSRV(nullptr);
	MeshFieldRenderer::SetRenderNormalSRV(nullptr);
	m_simulation.compute_task_runner.FinalizeAll();
	m_scene_controller.Finalize();
	RuntimeSystem::Finalize();
}
