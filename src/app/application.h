#ifndef APPLICATION_H
#define APPLICATION_H

#include <memory>

#include <Windows.h>

#include "compute_task_publish.h"
#include "compute_task_system_state.h"
#include "renderer_frame_builder.h"
#include "runtime_system.h"
#include "debug_text.h"
#include "render_backend_dx11.h"
#include "renderer.h"
#include "scene.h"
#include "scene_render_adapter.h"

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
	bool InitializeComputeResources();
	void ConfigureComputeTasks();
	void InitializeRuntimeObjects();
	void InitializeDebugTools();
	void ResetFrameState();
	void BeginFrame(double current_time, double elapsed_time);
	void ReloadComputeResources();
	void DispatchComputeFrame(double current_time, double elapsed_time);
	void RefreshDerivedComputeResources(double current_time);
	Backend::RenderShaderResource BaseTerrainHeightResource() const;
	Backend::RenderShaderResource SimulationTerrainHeightResource() const;
	Backend::RenderShaderResource ActiveTerrainHeightResource() const;
	Backend::RenderShaderResource ActiveTerrainNormalResource() const;
	bool UsingSharedTerrainWaterHeightfield() const;
	WaterSurfaceDesc ResolveActiveWaterSurfaceDesc() const;
	float ResolveActiveWaterSurfaceHeight() const;
	void PublishSharedComputeResources();
	void RenderDebugText();
	void RenderCurrentFrame(double current_time, double elapsed_time);
	void FinalizeEngineSystems();

	void UpdateFps(double current_time);
	void TickFrame(double current_time);

	HWND m_window_handle = nullptr;
	RenderBackendDX11 m_render_backend;
	std::unique_ptr<Renderer> m_renderer;
	std::unique_ptr<SceneRenderAdapter> m_scene_render_adapter;
	SceneController m_scene_controller{};
	ComputeTaskSystemState m_simulation{};

#if defined(DEBUG) || defined(_DEBUG)
	std::unique_ptr<hal::DebugText> m_debug_text;
#endif

	double m_exec_last_time = 0.0;
	double m_fps_last_time = 0.0;
	double m_fps = 0.0;
	unsigned long m_frame_count = 0;
	float m_glitch_amount = 0.0f;
};

#endif // APPLICATION_H
