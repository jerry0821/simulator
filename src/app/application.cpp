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
#include "compute_erosion_delta_texture.h"
#include "compute_final_terrain_height_texture.h"
#include "compute_meteorograph_texture.h"
#include "compute_noise_texture.h"
#include "compute_rain_map_texture.h"
#include "compute_shared_resource_registry.h"
#include "compute_soil_moisture_texture.h"
#include "compute_surface_water_texture.h"
#include "compute_task.h"
#include "compute_visible_water_texture.h"
#include "compute_water_surface_height_texture.h"
#include "compute_water_mask_texture.h"
#include "compute_wind_field_texture.h"
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
	m_debug_text.reset();
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
		m_compute_wind_field_texture,
		[this](double current_time, double /*elapsed_time*/)
		{
			m_compute_wind_field_texture.Update(
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
				ActiveTerrainHeightResource().shaderResourceView(),
				m_compute_wind_field_texture.Resource().shaderResourceView(),
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
		m_compute_rain_map_texture,
		[this](double current_time, double /*elapsed_time*/)
		{
			const auto& compute_settings = DebugMenu_GetComputeNoiseSettings();
			m_compute_rain_map_texture.Update(
				m_compute_climate_texture.Resource().shaderResourceView(),
				m_compute_wind_field_texture.Resource().shaderResourceView(),
				compute_settings.rain_multiplier,
				compute_settings.force_rain,
				static_cast<float>(current_time));
		});
	m_compute_task_runner.Register(
		m_compute_surface_water_texture,
		[this](double current_time, double /*elapsed_time*/)
		{
			const float water_height = m_active_water_surface_desc.height;
			const SurfaceWaterSimulationSettings& water_sim_settings =
				DebugMenu_GetSurfaceWaterSimulationSettings();
			if (DebugMenu_ConsumeSurfaceWaterResetRequest())
			{
				m_compute_surface_water_texture.ClearState();
			}
			const bool inject_water_pulse = DebugMenu_ConsumeSurfaceWaterInjectionRequest();
			m_compute_surface_water_texture.Update(
				ActiveTerrainHeightResource().shaderResourceView(),
				m_compute_rain_map_texture.Resource().shaderResourceView(),
				m_compute_wind_field_texture.Resource().shaderResourceView(),
				water_height,
				water_sim_settings,
				inject_water_pulse,
				static_cast<float>(current_time));
		});
	m_compute_task_runner.Register(
		m_compute_water_surface_height_texture,
		[this](double /*current_time*/, double /*elapsed_time*/)
		{
			m_compute_water_surface_height_texture.Update(
				ActiveTerrainHeightResource().shaderResourceView(),
				m_compute_surface_water_texture.Resource().shaderResourceView(),
				m_active_water_surface_height);
		});
	m_compute_task_runner.Register(
		m_compute_visible_water_texture,
		[this](double /*current_time*/, double /*elapsed_time*/)
		{
			m_compute_visible_water_texture.Update(
				ActiveTerrainHeightResource().shaderResourceView(),
				m_compute_surface_water_texture.Resource().shaderResourceView(),
				m_compute_surface_water_texture.FlowResource().shaderResourceView());
		});
	m_compute_task_runner.Register(
		m_compute_water_mask_texture,
		[this](double /*current_time*/, double /*elapsed_time*/)
		{
			m_compute_water_mask_texture.Update(
				m_compute_visible_water_texture.Resource().shaderResourceView());
		});
	m_compute_task_runner.Register(
		m_compute_soil_moisture_texture,
		[this](double /*current_time*/, double /*elapsed_time*/)
		{
			m_compute_soil_moisture_texture.Update(
				m_compute_rain_map_texture.Resource().shaderResourceView(),
				m_compute_surface_water_texture.Resource().shaderResourceView(),
				m_compute_water_mask_texture.Resource().shaderResourceView());
		});
	m_compute_task_runner.Register(
		m_compute_erosion_delta_texture,
		[this](double /*current_time*/, double /*elapsed_time*/)
		{
			m_compute_erosion_delta_texture.Update(
				ActiveTerrainHeightResource().shaderResourceView(),
				m_compute_surface_water_texture.Resource().shaderResourceView(),
				m_compute_surface_water_texture.FlowResource().shaderResourceView());
		});
	m_compute_task_runner.Register(
		m_compute_meteorograph_texture,
		[this](double /*current_time*/, double /*elapsed_time*/)
		{
			m_compute_meteorograph_texture.Update(
				Camera_GetPosition().x,
				Camera_GetPosition().z,
				m_compute_climate_texture.Resource(),
			m_compute_wind_field_texture.Resource());
		});
	m_compute_task_runner.InitializeAll(Direct3D_GetDevice(), Direct3D_GetContext());
	m_compute_floating_light_points.SetSeeds(BuildFloatingWindLightSeeds());
	if (!m_compute_final_terrain_height_texture.Initialize(Direct3D_GetDevice(), Direct3D_GetContext()))
	{
		return false;
	}
	if (!m_compute_terrain_classification_texture.Initialize(Direct3D_GetDevice(), Direct3D_GetContext()))
	{
		return false;
	}
	m_has_last_terrain_material_settings = false;
	m_terrain_classification_last_update_time = -1000.0;
	m_active_water_surface_desc = ResolveActiveWaterSurfaceDesc();
	m_active_water_surface_height = m_active_water_surface_desc.height;
	TerrainDataModel::SetComputedHeightSource(&m_compute_final_terrain_height_texture);
	m_compute_final_terrain_height_texture.Update(
		MeshFieldRenderer::HeightSRV(),
		m_compute_erosion_delta_texture.Resource().shaderResourceView());
	TerrainMaterialSettings initial_terrain_material_settings = DebugMenu_GetTerrainMaterialSettings();
	initial_terrain_material_settings.water_height = m_active_water_surface_height;
	m_compute_terrain_classification_texture.Update(
		ActiveTerrainHeightResource().shaderResourceView(),
		m_compute_water_mask_texture.Resource().shaderResourceView(),
		m_compute_soil_moisture_texture.Resource().shaderResourceView(),
		m_compute_erosion_delta_texture.Resource().shaderResourceView(),
		m_compute_climate_texture.Resource().shaderResourceView(),
		initial_terrain_material_settings);
	MeshFieldRenderer::SetRenderHeightSRV(ActiveTerrainHeightResource().shaderResourceView());
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
		if (!m_compute_rain_map_texture.IsValid())
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute rain map reload failed");
		}
		if (!m_compute_meteorograph_texture.IsValid())
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute meteorograph reload failed");
		}
		if (!m_compute_surface_water_texture.IsValid())
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute surface water reload failed");
		}
		if (!m_compute_soil_moisture_texture.IsValid())
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute soil moisture reload failed");
		}
		if (!m_compute_visible_water_texture.IsValid())
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute visible water reload failed");
		}
		if (!m_compute_erosion_delta_texture.IsValid())
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute erosion delta reload failed");
		}
		m_compute_final_terrain_height_texture.Finalize();
		if (!m_compute_final_terrain_height_texture.Initialize(Direct3D_GetDevice(), Direct3D_GetContext()))
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute final terrain height reload failed");
		}
		else
		{
			m_compute_final_terrain_height_texture.Update(
				MeshFieldRenderer::HeightSRV(),
				m_compute_erosion_delta_texture.Resource().shaderResourceView());
		}
		m_compute_terrain_classification_texture.Finalize();
		if (!m_compute_terrain_classification_texture.Initialize(Direct3D_GetDevice(), Direct3D_GetContext()))
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute terrain classification reload failed");
		}
		m_has_last_terrain_material_settings = false;
		m_terrain_classification_last_update_time = -1000.0;
		if (!m_compute_wind_field_texture.IsValid())
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute wind field reload failed");
		}
		if (!m_compute_water_surface_height_texture.IsValid())
		{
			DebugMenu_SetShaderReloadStatus(false, "Compute water surface height reload failed");
		}
	}

	m_active_water_surface_desc = ResolveActiveWaterSurfaceDesc();
	m_active_water_surface_height = m_active_water_surface_desc.height;
	m_compute_task_runner.Dispatch(current_time, elapsed_time);
	if (m_compute_final_terrain_height_texture.IsValid())
	{
		m_compute_final_terrain_height_texture.Update(
			MeshFieldRenderer::HeightSRV(),
			m_compute_erosion_delta_texture.Resource().shaderResourceView());
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
				ActiveTerrainHeightResource().shaderResourceView(),
				m_compute_water_mask_texture.Resource().shaderResourceView(),
				m_compute_soil_moisture_texture.Resource().shaderResourceView(),
				m_compute_erosion_delta_texture.Resource().shaderResourceView(),
				m_compute_climate_texture.Resource().shaderResourceView(),
				terrain_material_settings);
			m_terrain_classification_last_update_time = current_time;
			m_last_terrain_material_settings = terrain_material_settings;
			m_has_last_terrain_material_settings = true;
		}
	}
	TerrainDataModel::SetMaterialSettings(terrain_material_settings);
	MeshFieldRenderer::SetRenderHeightSRV(ActiveTerrainHeightResource().shaderResourceView());
	PublishSharedComputeResources();
	ShaderField_SetTerrainMaterialSettings(terrain_material_settings);
	Sprite_Begin();
}

Backend::RenderShaderResource Application::ActiveTerrainHeightResource() const
{
	return TerrainDataModel::HeightResource();
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
		water_surface_desc.ripple_color = { 0.26f, 0.60f, 0.82f, 0.16f };
		water_surface_desc.highlight_color = { 0.60f, 0.82f, 0.96f, 0.10f };
		water_surface_desc.ripple_strength = 1.65f;
		water_surface_desc.wind_influence = 1.85f;
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
	m_terrain_water_state.Update(
		m_active_water_surface_height,
		m_active_water_surface_desc,
		ActiveTerrainHeightResource(),
		m_compute_terrain_classification_texture.Resource(),
		m_compute_terrain_classification_texture.VegetationSuitabilityResource(),
		m_compute_surface_water_texture.Resource(),
		m_compute_water_surface_height_texture.Resource(),
		m_compute_surface_water_texture.FlowResource(),
		m_compute_surface_water_texture.FlowPreviewResource(),
		m_compute_visible_water_texture.Resource(),
		m_compute_water_mask_texture.Resource(),
		m_compute_soil_moisture_texture.Resource(),
		m_compute_erosion_delta_texture.Resource());
	m_terrain_water_state.PublishTo(m_compute_shared_resource_registry);
	m_compute_shared_resource_registry.PublishShaderResource(
		ComputeSharedResourceId::RainMap,
		"RainMap",
		m_compute_rain_map_texture.Resource());
	m_compute_shared_resource_registry.PublishShaderResource(
		ComputeSharedResourceId::ComputeNoise,
		"ComputeNoise",
		m_compute_noise_texture.Resource());
	m_compute_shared_resource_registry.PublishShaderResource(
		ComputeSharedResourceId::WindField,
		"WindField",
		m_compute_wind_field_texture.Resource());
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
	frame_context.resources.terrain_height = frame_context.resources.terrain_water.terrain_height;
	frame_context.resources.terrain_classification =
		frame_context.resources.terrain_water.terrain_classification;
	frame_context.resources.terrain_vegetation_suitability =
		frame_context.resources.terrain_water.terrain_vegetation_suitability;
	frame_context.resources.rain_map =
		m_compute_shared_resource_registry.GetShaderResource(ComputeSharedResourceId::RainMap);
	frame_context.resources.surface_water = frame_context.resources.terrain_water.surface_water;
	frame_context.resources.water_surface_height = frame_context.resources.terrain_water.water_surface_height_texture;
	frame_context.resources.surface_water_flow = frame_context.resources.terrain_water.surface_water_flow;
	frame_context.resources.surface_water_flow_preview =
		frame_context.resources.terrain_water.surface_water_flow_preview;
	frame_context.resources.visible_water = frame_context.resources.terrain_water.visible_water;
	frame_context.resources.water_mask = frame_context.resources.terrain_water.water_mask;
	frame_context.resources.soil_moisture = frame_context.resources.terrain_water.soil_moisture;
	frame_context.resources.erosion_delta = frame_context.resources.terrain_water.erosion_delta;
	frame_context.resources.compute_noise =
		m_compute_shared_resource_registry.GetShaderResource(ComputeSharedResourceId::ComputeNoise);
	frame_context.resources.wind_field =
		m_compute_shared_resource_registry.GetShaderResource(ComputeSharedResourceId::WindField);
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
	TerrainDataModel::ClearComputedHeightSource();
	m_has_last_terrain_material_settings = false;
	m_terrain_classification_last_update_time = -1000.0;
	m_active_water_surface_desc = WaterSurfaceDesc{};
	m_active_water_surface_height = 0.0f;
	m_terrain_water_state.Clear();
	m_compute_terrain_classification_texture.Finalize();
	m_compute_final_terrain_height_texture.Finalize();
	MeshFieldRenderer::SetRenderHeightSRV(nullptr);
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
