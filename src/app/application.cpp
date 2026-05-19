#include "application.h"

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>
#include <random>
#include <sstream>

#include "camera.h"
#include "billboard.h"
#include "capsule.h"
#include "collision.h"
#include "compute_climate_texture.h"
#include "compute_meteorograph_texture.h"
#include "compute_noise_texture.h"
#include "compute_shared_resource_registry.h"
#include "compute_task.h"
#include "compute_terrain_normal_texture.h"
#include "compute_water_surface_height_texture.h"
#include "cube.h"
#include "debug_ostream.h"
#include "cylinder.h"
#include "debug_menu.h"
#include "debug_text.h"
#include "direct3d.h"
#include "frustum_culling_debug.h"
#include "game_window.h"
#include "grass_patch.h"
#include "grid.h"
#include "instancing_debug.h"
#include "key_logger.h"
#include "light.h"
#include "meshfield.h"
#include "mouse.h"
#include "pad_logger.h"
#include "renderer.h"
#include "sampler.h"
#include "scene_render_adapter.h"
#include "shader.h"
#include "shader3d.h"
#include "shader3d_instanced.h"
#include "shader_grass_instanced.h"
#include "shader3d_unlit.h"
#include "shader_field.h"
#include "shader_glitch.h"
#include "shader_post.h"
#include "shader_reload.h"
#include "shader_shadow.h"
#include "shader_sprite3d_cutout.h"
#include "shader_toon.h"
#include "sphere.h"
#include "sprite.h"
#include "sprite3d.h"
#include "sprite_anim.h"
#include "system_timer.h"
#include "terrain_data_model.h"
#include "texture.h"

namespace
{
constexpr double kTerrainClassificationUpdateIntervalSeconds = 1.0 / 12.0;

std::vector<ComputeFloatingLightPoints::Seed> BuildFloatingWindLightSeeds()
{
	constexpr int kCols = 24;
	constexpr int kRows = 24;
	constexpr int kLayers = 3;
	constexpr float kLocalMinRight = -7.5f;
	constexpr float kLocalMaxRight = 7.5f;
	constexpr float kLocalMinForward = 3.0f;
	constexpr float kLocalMaxForward = 15.0f;

	std::mt19937 rng(1337u);
	std::uniform_real_distribution<float> phase_distribution(0.0f, DirectX::XM_2PI);
	std::uniform_real_distribution<float> size_distribution(0.62f, 1.18f);
	std::uniform_real_distribution<float> bob_distribution(0.12f, 0.32f);
	std::uniform_real_distribution<float> drift_distribution(0.70f, 1.55f);
	std::uniform_real_distribution<float> horizontal_jitter(-0.8f, 0.8f);
	std::uniform_real_distribution<float> vertical_jitter(-0.22f, 0.12f);

	std::vector<ComputeFloatingLightPoints::Seed> seeds;
	seeds.reserve(kCols * kRows * kLayers);

	for (int layer = 0; layer < kLayers; ++layer)
	{
		for (int z = 0; z < kRows; ++z)
		{
			for (int x = 0; x < kCols; ++x)
			{
				const float u = static_cast<float>(x) / static_cast<float>(kCols - 1);
				const float v = static_cast<float>(z) / static_cast<float>(kRows - 1);
				const float local_x =
					std::lerp(kLocalMinRight, kLocalMaxRight, u) + horizontal_jitter(rng);
				const float local_z =
					std::lerp(kLocalMinForward, kLocalMaxForward, v) + horizontal_jitter(rng);
				const float base_y =
					-0.10f + static_cast<float>(layer) * 0.78f + vertical_jitter(rng);

				ComputeFloatingLightPoints::Seed seed{};
				seed.base_position_size = {
					local_x,
					base_y,
					local_z,
					size_distribution(rng) };
				seed.params = {
					phase_distribution(rng),
					bob_distribution(rng),
					drift_distribution(rng),
					0.0f };
				seeds.push_back(seed);
			}
		}
	}

	for (int i = 0; i < 40; ++i)
	{
		const float angle =
			DirectX::XM_2PI * (static_cast<float>(i) / 40.0f);
		const float ring_radius = (i % 2 == 0) ? 3.5f : 6.0f;
		const float local_x = std::cos(angle) * ring_radius;
		const float local_z = 9.0f + std::sin(angle) * ring_radius;

		ComputeFloatingLightPoints::Seed seed{};
		seed.base_position_size = {
			local_x,
			0.25f + (i % 3) * 0.32f,
			local_z,
			0.92f + (i % 4) * 0.18f };
		seed.params = {
			phase_distribution(rng),
			0.14f + (i % 5) * 0.04f,
			0.85f + (i % 4) * 0.12f,
			0.0f };
		seeds.push_back(seed);
	}

	return seeds;
}

DirectX::XMFLOAT3 ExtractMatrixAxis(const DirectX::XMMATRIX& matrix, int row_index)
{
	return {
		DirectX::XMVectorGetX(matrix.r[row_index]),
		DirectX::XMVectorGetY(matrix.r[row_index]),
		DirectX::XMVectorGetZ(matrix.r[row_index]) };
}

bool ComputeBaseTerrainHeightRange(float& out_min_height, float& out_max_height)
{
	constexpr unsigned int kSampleResolution = 257;
	const float field_width = MeshFieldRenderer::FieldWidth();
	const float field_depth = MeshFieldRenderer::FieldDepth();
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
			const float height = MeshFieldRenderer::GetHeight(world_x, world_z);
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

bool Application::Initialize(HINSTANCE instance_handle, int show_command)
{
	if (!InitializePlatform(instance_handle, show_command))
	{
		return false;
	}

	if (!InitializeEngineSystems())
	{
		return false;
	}

	InitializeRuntimeObjects();
	InitializeDebugTools();
	ResetFrameState();

	return true;
}

int Application::Run()
{
	MSG message{};

	do
	{
		if (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		else
		{
			const double current_time = SystemTimer_GetTime();
			UpdateFps(current_time);
			TickFrame(current_time);
		}
	} while (message.message != WM_QUIT);

	return static_cast<int>(message.wParam);
}

void Application::Shutdown()
{
#if defined(DEBUG) || defined(_DEBUG)
	m_debug_text.reset();
#endif
	m_scene_render_adapter.reset();
	m_renderer.reset();

	FinalizeEngineSystems();
}

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
	SystemTimer_Initialize();
	KeyLogger_Initialize();
	PadLogger_Initialize();
	Mouse_Initialize(m_window_handle);

	if (!Direct3D_Initialize(m_window_handle))
	{
		return false;
	}

	Shader_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Shader3D_Initialize();
	Shader3DInstanced_Initialize();
	ShaderGrassInstanced_Initialize();
	Shader3D_Unlit_Initialize();
	ShaderToon_Initialize();
	ShaderPost_Initialize();
	ShaderShadow_Initialize();
	ShaderGlitch_Initialize();
	TextureManager::Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Backend::DX11::Sampler::Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Sprite_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Sprite3D_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	SpriteAnim_Initialize();
	Billboard_Initialize();
	Collision::Debug::Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Grid_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Cube_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	GrassPatch_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Sphere_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Cylinder_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Capsule_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	MeshFieldRenderer::Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Light_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());

	m_compute_task_runner.Clear();
	m_compute_task_runner.Register(
		m_compute_noise_texture,
		[this](double current_time, double /*elapsed_time*/)
		{
			m_compute_noise_texture.Update(
				static_cast<float>(current_time),
				DebugMenu_GetComputeNoiseSettings());
		});
	m_compute_task_runner.Register(
		m_compute_floating_light_points,
		[this](double current_time, double /*elapsed_time*/)
		{
			const DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&Camera_GetMatrix());
			const DirectX::XMFLOAT3 camera_position = Camera_GetPosition();
			const DirectX::XMFLOAT3 camera_front = Camera_GetVector_Front();
			const DirectX::XMMATRIX inverse_view = DirectX::XMMatrixInverse(nullptr, view);
			const DirectX::XMFLOAT3 camera_right = ExtractMatrixAxis(inverse_view, 0);
			const DirectX::XMFLOAT3 camera_up = ExtractMatrixAxis(inverse_view, 1);
			m_compute_floating_light_points.Update(
				static_cast<float>(current_time),
				SimulationTerrainHeightResource().shaderResourceView(),
				m_compute_meteorograph_texture.Resource().shaderResourceView(),
				camera_position,
				camera_front,
				camera_right,
				camera_up);
		});
	m_compute_task_runner.Register(
		m_compute_climate_texture,
		[this](double current_time, double /*elapsed_time*/)
		{
			m_compute_climate_texture.Update(static_cast<float>(current_time));
		});
	m_compute_task_runner.Register(
		m_compute_meteorograph_texture,
		[this](double current_time, double elapsed_time)
		{
			const Backend::RenderShaderResource atmosphere_terrain =
				m_compute_water_surface_height_texture.HasBootstrappedState()
					? SimulationTerrainHeightResource()
					: BaseTerrainHeightResource();
			m_compute_meteorograph_texture.Update(
				static_cast<float>(current_time),
				static_cast<float>(elapsed_time),
				DebugMenu_GetComputeNoiseSettings(),
				atmosphere_terrain);
		});
	m_compute_task_runner.Register(
		m_compute_water_surface_height_texture,
		[this](double current_time, double elapsed_time)
		{
			if (m_pending_surface_water_reset)
			{
				m_compute_water_surface_height_texture.ResetState();
			}

			if (!m_compute_water_surface_height_texture.HasBootstrappedState())
			{
				m_compute_water_surface_height_texture.InitializeState(
					BaseTerrainHeightResource().shaderResourceView(),
					m_compute_meteorograph_texture.Resource().shaderResourceView(),
					m_active_water_surface_height,
					DebugMenu_GetSurfaceWaterSimulationSettings());
			}
			else
			{
				const SurfaceWaterSimulationSettings& water_sim_settings =
					DebugMenu_GetSurfaceWaterSimulationSettings();
				const bool inject_water_pulse =
					DebugMenu_ConsumeSurfaceWaterInjectionRequest();
				m_compute_water_surface_height_texture.Update(
					BaseTerrainHeightResource().shaderResourceView(),
					m_compute_meteorograph_texture.RainResource().shaderResourceView(),
					m_compute_meteorograph_texture.Resource().shaderResourceView(),
					m_active_water_surface_height,
					water_sim_settings,
					inject_water_pulse,
					static_cast<float>(current_time),
					static_cast<float>(elapsed_time));
			}
		});
	m_compute_task_runner.InitializeAll(Direct3D_GetDevice(), Direct3D_GetContext());
	m_compute_floating_light_points.SetSeeds(BuildFloatingWindLightSeeds());
	if (!m_compute_terrain_normal_texture.Initialize(Direct3D_GetDevice(), Direct3D_GetContext()))
	{
		return false;
	}
	if (!m_compute_terrain_classification_texture.Initialize(Direct3D_GetDevice(), Direct3D_GetContext()))
	{
		return false;
	}
	if (!m_compute_grass_data_texture.Initialize(Direct3D_GetDevice(), Direct3D_GetContext()))
	{
		return false;
	}
	m_has_last_terrain_material_settings = false;
	m_terrain_classification_last_update_time = -1000.0;
	m_active_water_surface_desc = ResolveActiveWaterSurfaceDesc();
	m_active_water_surface_height = m_active_water_surface_desc.height;
	m_compute_climate_texture.Update(0.0f);
	m_compute_meteorograph_texture.Update(
		0.0f,
		1.0f / 60.0f,
		DebugMenu_GetComputeNoiseSettings(),
		BaseTerrainHeightResource());
	m_compute_water_surface_height_texture.InitializeState(
		BaseTerrainHeightResource().shaderResourceView(),
		m_compute_meteorograph_texture.Resource().shaderResourceView(),
		m_active_water_surface_height,
		DebugMenu_GetSurfaceWaterSimulationSettings());
	m_compute_terrain_normal_texture.Update(SimulationTerrainHeightResource().shaderResourceView());
	TerrainMaterialSettings initial_terrain_material_settings = DebugMenu_GetTerrainMaterialSettings();
	initial_terrain_material_settings.water_height = m_active_water_surface_height;
	m_compute_terrain_classification_texture.Update(
		SimulationTerrainHeightResource().shaderResourceView(),
		ActiveTerrainNormalResource().shaderResourceView(),
		m_compute_water_surface_height_texture.WaterInteractionResource().shaderResourceView(),
		m_compute_water_surface_height_texture.ErosionDeltaResource().shaderResourceView(),
		m_compute_climate_texture.Resource().shaderResourceView(),
		initial_terrain_material_settings);
	m_compute_grass_data_texture.Update(
		ActiveTerrainNormalResource().shaderResourceView(),
		m_compute_terrain_classification_texture.VegetationSuitabilityResource().shaderResourceView(),
		m_compute_water_surface_height_texture.TerrainSurfaceDataResource().shaderResourceView(),
		m_compute_meteorograph_texture.Resource().shaderResourceView());
	MeshFieldRenderer::SetRenderHeightSRV(
		ActiveTerrainHeightResource().shaderResourceView());
	MeshFieldRenderer::SetRenderNormalSRV(ActiveTerrainNormalResource().shaderResourceView());
	PublishSharedComputeResources();
	return true;
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

void Application::ResetFrameState()
{
	m_exec_last_time = SystemTimer_GetTime();
	m_fps_last_time = m_exec_last_time;
	m_frame_count = 0;
	m_fps = 0.0;
	m_glitch_amount = 0.0f;
}

void Application::BeginFrame(double current_time, double elapsed_time)
{
	KeyLogger_Update();
	PadLogger_Update();
	DebugMenu_Begin();
	InstancingDebug_ResetStats();
	FrustumCullingDebug_ResetStats();
	m_scene_controller.Update(elapsed_time);
	SpriteAnim_Update(elapsed_time);
	MeshFieldRenderer::ApplyTerrainSettings(DebugMenu_GetTerrainSettings());
	m_pending_surface_water_reset = DebugMenu_ConsumeSurfaceWaterResetRequest();

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
		const ShaderReloadStatus reload_status = ShaderReload_ReloadAll();
		m_compute_task_runner.ReloadAll(Direct3D_GetDevice(), Direct3D_GetContext());
		DebugMenu_SetShaderReloadStatus(reload_status.succeeded, reload_status.message.c_str());
		if (!m_compute_climate_texture.IsValid())
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute climate reload failed");
		}
		if (!m_compute_noise_texture.IsValid())
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute noise reload failed");
		}
		if (!m_compute_meteorograph_texture.IsValid())
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute meteorograph reload failed");
		}
		if (m_compute_climate_texture.IsValid() &&
			m_compute_meteorograph_texture.IsValid())
		{
			m_compute_climate_texture.Update(0.0f);
			m_compute_meteorograph_texture.Update(
				0.0f,
				1.0f / 60.0f,
				DebugMenu_GetComputeNoiseSettings(),
				BaseTerrainHeightResource());
		}
		if (!m_compute_water_surface_height_texture.IsValid())
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute water surface height reload failed");
		}
		else
		{
			if (!m_compute_water_surface_height_texture.HasBootstrappedState())
			{
				m_compute_water_surface_height_texture.InitializeState(
					BaseTerrainHeightResource().shaderResourceView(),
					m_compute_meteorograph_texture.Resource().shaderResourceView(),
					m_active_water_surface_height,
					DebugMenu_GetSurfaceWaterSimulationSettings());
			}
			else
			{
				m_compute_water_surface_height_texture.Update(
					BaseTerrainHeightResource().shaderResourceView(),
					m_compute_meteorograph_texture.RainResource().shaderResourceView(),
					m_compute_meteorograph_texture.Resource().shaderResourceView(),
					m_active_water_surface_height,
					DebugMenu_GetSurfaceWaterSimulationSettings(),
					false,
					0.0f,
					1.0f / 60.0f);
			}
		}
		m_compute_terrain_normal_texture.Finalize();
		if (!m_compute_terrain_normal_texture.Initialize(Direct3D_GetDevice(), Direct3D_GetContext()))
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute terrain normal reload failed");
		}
		else
		{
			m_compute_terrain_normal_texture.Update(SimulationTerrainHeightResource().shaderResourceView());
		}
		m_compute_terrain_classification_texture.Finalize();
		if (!m_compute_terrain_classification_texture.Initialize(Direct3D_GetDevice(), Direct3D_GetContext()))
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute terrain surface data reload failed");
		}
		m_compute_grass_data_texture.Finalize();
		if (!m_compute_grass_data_texture.Initialize(Direct3D_GetDevice(), Direct3D_GetContext()))
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute grass data reload failed");
		}
		m_has_last_terrain_material_settings = false;
		m_terrain_classification_last_update_time = -1000.0;
	}

	m_active_water_surface_desc = ResolveActiveWaterSurfaceDesc();
	m_active_water_surface_height = m_active_water_surface_desc.height;
	m_compute_task_runner.Dispatch(current_time, elapsed_time);
	if (m_compute_terrain_normal_texture.IsValid())
	{
		m_compute_terrain_normal_texture.Update(SimulationTerrainHeightResource().shaderResourceView());
	}
	TerrainMaterialSettings terrain_material_settings = DebugMenu_GetTerrainMaterialSettings();
	terrain_material_settings.water_height = m_active_water_surface_height;
	if (m_compute_terrain_classification_texture.IsValid())
	{
		const bool first_classification_update = !m_has_last_terrain_material_settings;
		const bool material_settings_changed =
			!first_classification_update && m_last_terrain_material_settings != terrain_material_settings;
		const bool interval_elapsed =
			current_time - m_terrain_classification_last_update_time >=
			kTerrainClassificationUpdateIntervalSeconds;
		if (first_classification_update || material_settings_changed || interval_elapsed)
		{
			m_compute_terrain_classification_texture.Update(
				SimulationTerrainHeightResource().shaderResourceView(),
				ActiveTerrainNormalResource().shaderResourceView(),
				m_compute_water_surface_height_texture.WaterInteractionResource().shaderResourceView(),
				m_compute_water_surface_height_texture.ErosionDeltaResource().shaderResourceView(),
				m_compute_climate_texture.Resource().shaderResourceView(),
				terrain_material_settings);
			if (m_compute_grass_data_texture.IsValid())
			{
				m_compute_grass_data_texture.Update(
					ActiveTerrainNormalResource().shaderResourceView(),
					m_compute_terrain_classification_texture.VegetationSuitabilityResource().shaderResourceView(),
					m_compute_water_surface_height_texture.TerrainSurfaceDataResource().shaderResourceView(),
					m_compute_meteorograph_texture.Resource().shaderResourceView());
			}
			m_terrain_classification_last_update_time = current_time;
			m_last_terrain_material_settings = terrain_material_settings;
			m_has_last_terrain_material_settings = true;
		}
	}
	TerrainDataModel::SetMaterialSettings(terrain_material_settings);
	MeshFieldRenderer::SetRenderHeightSRV(
		ActiveTerrainHeightResource().shaderResourceView());
	MeshFieldRenderer::SetRenderNormalSRV(ActiveTerrainNormalResource().shaderResourceView());
	PublishSharedComputeResources();
	ShaderField_SetTerrainMaterialSettings(terrain_material_settings);
	Sprite_Begin();
}

Backend::RenderShaderResource Application::BaseTerrainHeightResource() const
{
	return MeshFieldRenderer::HeightResource();
}

Backend::RenderShaderResource Application::SimulationTerrainHeightResource() const
{
	if (UsingSharedTerrainWaterHeightfield())
	{
		return m_compute_water_surface_height_texture.Resource();
	}

	return BaseTerrainHeightResource();
}

Backend::RenderShaderResource Application::ActiveTerrainHeightResource() const
{
	if (UsingSharedTerrainWaterHeightfield())
	{
		return m_compute_water_surface_height_texture.Resource();
	}

	return TerrainDataModel::HeightResource();
}

Backend::RenderShaderResource Application::ActiveTerrainNormalResource() const
{
	return m_compute_terrain_normal_texture.Resource();
}

bool Application::UsingSharedTerrainWaterHeightfield() const
{
	return
		m_use_shared_terrain_water_heightfield &&
		m_compute_water_surface_height_texture.IsValid() &&
		m_compute_water_surface_height_texture.Resource().isValid();
}

WaterSurfaceDesc Application::ResolveActiveWaterSurfaceDesc() const
{
	WaterSurfaceDesc water_surface_desc = DebugMenu_GetWaterSurfaceSettings();
	water_surface_desc.enabled = true;
	water_surface_desc.center_x = 0.0f;
	water_surface_desc.center_z = 0.0f;
	water_surface_desc.height = ResolveActiveWaterSurfaceHeight();
	water_surface_desc.size_x = MeshFieldRenderer::FieldWidth();
	water_surface_desc.size_z = MeshFieldRenderer::FieldDepth();
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
	m_compute_shared_resource_registry.Clear();
	m_compute_shared_resource_registry.PublishShaderResource(
		ComputeSharedResourceId::BaseTerrainHeight,
		"BaseTerrainHeight",
		BaseTerrainHeightResource());
	m_terrain_water_state.Update(
		m_active_water_surface_height,
		m_active_water_surface_desc,
		ActiveTerrainHeightResource(),
		ActiveTerrainNormalResource(),
		m_compute_water_surface_height_texture.TerrainSurfaceDataResource(),
		m_compute_terrain_classification_texture.VegetationSuitabilityResource(),
		m_compute_grass_data_texture.Resource(),
		m_compute_water_surface_height_texture.SurfaceWaterResource(),
		m_compute_water_surface_height_texture.Resource(),
		m_compute_water_surface_height_texture.FlowResource(),
		m_compute_water_surface_height_texture.VelocityResource(),
		m_compute_water_surface_height_texture.SedimentResource(),
		m_compute_water_surface_height_texture.WaterInteractionResource(),
		m_compute_water_surface_height_texture.SoilMoistureResource(),
		m_compute_water_surface_height_texture.ErosionDeltaResource());
	m_terrain_water_state.PublishTo(m_compute_shared_resource_registry);
	m_compute_shared_resource_registry.PublishShaderResource(
		ComputeSharedResourceId::RainMap,
		"RainMap",
		m_compute_meteorograph_texture.RainResource());
	m_compute_shared_resource_registry.PublishShaderResource(
		ComputeSharedResourceId::ComputeNoise,
		"ComputeNoise",
		m_compute_noise_texture.Resource());
	m_compute_shared_resource_registry.PublishShaderResource(
		ComputeSharedResourceId::ClimateField,
		"ClimateField",
		m_compute_climate_texture.Resource());
	m_compute_shared_resource_registry.PublishShaderResource(
		ComputeSharedResourceId::MeteorographField,
		"MeteorographField",
		m_compute_meteorograph_texture.Resource());
}

void Application::RenderDebugText()
{
#if defined(DEBUG) || defined(_DEBUG)
	if (!m_debug_text)
	{
		return;
	}

	std::stringstream stream;
	stream << "fps:" << m_fps << std::endl;
	m_debug_text->SetText(stream.str().c_str(), { 1.0f, 0.0f, 1.0f, 1.0f });
	m_debug_text->Draw();
	m_debug_text->Clear();
#endif
}

RenderFrameContext Application::BuildFrameContext(double current_time, double elapsed_time) const
{
	RenderFrameContext frame_context{};
	frame_context.render_scene = m_scene_render_adapter.get();
	frame_context.globals.view_matrix = Camera_GetMatrix();
	frame_context.globals.projection_matrix = Camera_GetPerspectiveMatrix();
	frame_context.globals.camera_position = Camera_GetPosition();
	frame_context.globals.backbuffer_width = Direct3D_GetBackBufferWidth();
	frame_context.globals.backbuffer_height = Direct3D_GetBackBufferHeight();
	frame_context.globals.time_seconds = current_time;
	frame_context.globals.elapsed_time = elapsed_time;
	frame_context.globals.glitch_amount = m_glitch_amount;
	frame_context.resources.terrain_water = m_terrain_water_state.FrameState();
	frame_context.resources.base_terrain_height =
		m_compute_shared_resource_registry.GetShaderResource(ComputeSharedResourceId::BaseTerrainHeight);
	frame_context.resources.has_base_terrain_range = ComputeBaseTerrainHeightRange(
		frame_context.resources.base_terrain_min_height,
		frame_context.resources.base_terrain_max_height);
	frame_context.resources.terrain_height =
		ActiveTerrainHeightResource();
	frame_context.resources.has_terrain_heightfield_range =
		m_compute_water_surface_height_texture.ComputeTerrainHeightRange(
			frame_context.resources.terrain_heightfield_min_height,
			frame_context.resources.terrain_heightfield_max_height);
	frame_context.resources.terrain_heightfield_range_is_fallback = false;
	frame_context.resources.has_water_heightfield_range =
		m_compute_water_surface_height_texture.ComputeWaterHeightRange(
			frame_context.resources.water_heightfield_min_height,
			frame_context.resources.water_heightfield_max_height);
	frame_context.resources.water_heightfield_range_is_fallback = false;
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
				m_active_water_surface_height);
		frame_context.resources.water_heightfield_max_height =
			std::max(
				frame_context.resources.base_terrain_max_height,
				m_active_water_surface_height);
	}
	if (UsingSharedTerrainWaterHeightfield() &&
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
	frame_context.resources.terrain_vegetation_suitability =
		frame_context.resources.terrain_water.terrain_vegetation_suitability;
	frame_context.resources.grass_data = frame_context.resources.terrain_water.grass_data;
	frame_context.resources.rain_map =
		m_compute_shared_resource_registry.GetShaderResource(ComputeSharedResourceId::RainMap);
	frame_context.resources.surface_water = frame_context.resources.terrain_water.surface_water;
	frame_context.resources.water_surface_height = frame_context.resources.terrain_water.water_surface_height_texture;
	frame_context.resources.surface_water_flow = frame_context.resources.terrain_water.surface_water_flow;
	frame_context.resources.water_velocity = frame_context.resources.terrain_water.water_velocity;
	frame_context.resources.water_sediment = frame_context.resources.terrain_water.water_sediment;
	frame_context.resources.water_interaction_data =
		frame_context.resources.terrain_water.water_interaction_data;
	frame_context.resources.soil_moisture = frame_context.resources.terrain_water.soil_moisture;
	frame_context.resources.erosion_delta = frame_context.resources.terrain_water.erosion_delta;
	frame_context.resources.compute_noise =
		m_compute_shared_resource_registry.GetShaderResource(ComputeSharedResourceId::ComputeNoise);
	frame_context.resources.climate_field =
		m_compute_shared_resource_registry.GetShaderResource(ComputeSharedResourceId::ClimateField);
	frame_context.resources.meteorograph_field =
		m_compute_shared_resource_registry.GetShaderResource(ComputeSharedResourceId::MeteorographField);
	frame_context.resources.floating_light_instance_buffer =
		m_compute_floating_light_points.InstanceBuffer();
	frame_context.resources.floating_light_instance_count =
		m_compute_floating_light_points.InstanceCount();
	frame_context.resources.compute_shared_registry =
		const_cast<ComputeSharedResourceRegistry*>(&m_compute_shared_resource_registry);
	return frame_context;
}

void Application::FinalizeEngineSystems()
{
	DebugMenu_Finalize();
	m_has_last_terrain_material_settings = false;
	m_terrain_classification_last_update_time = -1000.0;
	m_active_water_surface_desc = WaterSurfaceDesc{};
	m_active_water_surface_height = 0.0f;
	m_terrain_water_state.Clear();
	m_compute_terrain_classification_texture.Finalize();
	m_compute_grass_data_texture.Finalize();
	m_compute_terrain_normal_texture.Finalize();
	MeshFieldRenderer::SetRenderHeightSRV(nullptr);
	MeshFieldRenderer::SetRenderNormalSRV(nullptr);
	m_compute_task_runner.FinalizeAll();
	Light_Finalize();
	MeshFieldRenderer::Finalize();
	Grid_Finalize();
	GrassPatch_Finalize();
	Cylinder_Finalize();
	Capsule_Finalize();
	Sphere_Finalize();
	Cube_Finalize();
	Collision::Debug::Finalize();
	m_scene_controller.Finalize();
	SpriteAnim_Finalize();
	Sprite3D_Finalize();
	Sprite_Finalize();
	Billboard_Finalize();
	Backend::DX11::Sampler::Finalize();
	TextureManager::Finalize();
	ShaderGlitch_Finalize();
	ShaderShadow_Finalize();
	ShaderPost_Finalize();
	ShaderToon_Finalize();
	Shader3D_Unlit_Finalize();
	ShaderGrassInstanced_Finalize();
	Shader3DInstanced_Finalize();
	Shader3D_Finalize();
	Shader_Finalize();
	Direct3D_Finalize();
	Mouse_Finalize();
}

void Application::UpdateFps(double current_time)
{
	const double elapsed_time = current_time - m_fps_last_time;
	if (elapsed_time < 1.0)
	{
		return;
	}

	m_fps = m_frame_count / elapsed_time;
	m_fps_last_time = current_time;
	m_frame_count = 0;
}

void Application::TickFrame(double current_time)
{
	const double elapsed_time = current_time - m_exec_last_time;
	m_exec_last_time = current_time;

	BeginFrame(current_time, elapsed_time);
	RenderDebugText();

	const RenderFrameContext frame_context = BuildFrameContext(current_time, elapsed_time);
	m_renderer->RenderFrame(frame_context);

	DebugMenu_SetInstancingStats(InstancingDebug_GetStats());
	DebugMenu_SetFrustumCullingStats(FrustumCullingDebug_GetStats());
	DebugMenu_SetPerformanceStats(static_cast<float>(m_fps),
								  m_fps > 0.0 ? static_cast<float>(1000.0 / m_fps) : 0.0f);

	m_scene_controller.Refresh();
	++m_frame_count;
}
