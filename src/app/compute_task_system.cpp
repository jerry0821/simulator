#include "application.h"

#include <random>

#include <DirectXMath.h>

#include "camera.h"
#include "debug_menu.h"
#include "direct3d.h"
#include "meshfield.h"
#include "shader_reload.h"

namespace
{
constexpr bool kEnableGrassSystems = true;
constexpr bool kEnableTerrainClassificationCompute = true;
constexpr double kTerrainClassificationUpdateIntervalSeconds = 1.0 / 6.0;

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

void Application::ConfigureComputeTasks()
{
	m_simulation.compute_task_runner.Clear();
	m_simulation.compute_task_runner.Register(
		m_simulation.compute_noise_texture,
		[this](double current_time, double /*elapsed_time*/)
		{
			m_simulation.compute_noise_texture.Update(
				static_cast<float>(current_time),
				DebugMenu_GetComputeNoiseSettings());
		});
	m_simulation.compute_task_runner.Register(
		m_simulation.compute_floating_light_points,
		[this](double current_time, double /*elapsed_time*/)
		{
			const DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&Camera_GetMatrix());
			const DirectX::XMFLOAT3 camera_position = Camera_GetPosition();
			const DirectX::XMFLOAT3 camera_front = Camera_GetVector_Front();
			const DirectX::XMMATRIX inverse_view = DirectX::XMMatrixInverse(nullptr, view);
			const DirectX::XMFLOAT3 camera_right = ExtractMatrixAxis(inverse_view, 0);
			const DirectX::XMFLOAT3 camera_up = ExtractMatrixAxis(inverse_view, 1);
			m_simulation.compute_floating_light_points.Update(
				static_cast<float>(current_time),
				SimulationTerrainHeightResource().shaderResourceView(),
				m_simulation.compute_meteorograph_texture.Resource().shaderResourceView(),
				camera_position,
				camera_front,
				camera_right,
				camera_up);
		});
	m_simulation.compute_task_runner.Register(
		m_simulation.compute_meteorograph_texture,
		[this](double current_time, double elapsed_time)
		{
			const Backend::RenderShaderResource atmosphere_terrain =
				m_simulation.compute_water_surface_height_texture.HasBootstrappedState()
					? SimulationTerrainHeightResource()
					: BaseTerrainHeightResource();
			m_simulation.compute_meteorograph_texture.Update(
				static_cast<float>(current_time),
				static_cast<float>(elapsed_time),
				DebugMenu_GetComputeNoiseSettings(),
				atmosphere_terrain);
		});
	m_simulation.compute_task_runner.Register(
		m_simulation.compute_water_surface_height_texture,
		[this](double current_time, double elapsed_time)
		{
			if (m_simulation.pending_surface_water_reset)
			{
				m_simulation.compute_water_surface_height_texture.ResetState();
			}

			if (!m_simulation.compute_water_surface_height_texture.HasBootstrappedState())
			{
				m_simulation.compute_water_surface_height_texture.InitializeState(
					BaseTerrainHeightResource().shaderResourceView(),
					m_simulation.compute_meteorograph_texture.Resource().shaderResourceView(),
					m_simulation.active_water_surface_height,
					DebugMenu_GetSurfaceWaterSimulationSettings());
			}
			else
			{
				const SurfaceWaterSimulationSettings& water_sim_settings =
					DebugMenu_GetSurfaceWaterSimulationSettings();
				const bool inject_water_pulse =
					DebugMenu_ConsumeSurfaceWaterInjectionRequest();
				m_simulation.compute_water_surface_height_texture.Update(
					BaseTerrainHeightResource().shaderResourceView(),
					m_simulation.compute_meteorograph_texture.RainResource().shaderResourceView(),
					m_simulation.compute_meteorograph_texture.Resource().shaderResourceView(),
					m_simulation.active_water_surface_height,
					water_sim_settings,
					inject_water_pulse,
					static_cast<float>(current_time),
					static_cast<float>(elapsed_time));
			}
		});
}

bool Application::InitializeComputeResources()
{
	m_simulation.compute_task_runner.InitializeAll(Direct3D_GetDevice(), Direct3D_GetContext());
	m_simulation.compute_floating_light_points.SetSeeds(BuildFloatingWindLightSeeds());
	if (kEnableTerrainClassificationCompute &&
		!m_simulation.compute_terrain_classification_texture.Initialize(Direct3D_GetDevice(), Direct3D_GetContext()))
	{
		return false;
	}
	if (kEnableGrassSystems &&
		!m_simulation.compute_grass_data_texture.Initialize(Direct3D_GetDevice(), Direct3D_GetContext()))
	{
		return false;
	}
	m_simulation.has_last_terrain_material_settings = false;
	m_simulation.terrain_classification_last_update_time = -1000.0;
	m_simulation.active_water_surface_desc = ResolveActiveWaterSurfaceDesc();
	m_simulation.active_water_surface_height = m_simulation.active_water_surface_desc.height;
	m_simulation.compute_meteorograph_texture.Update(
		0.0f,
		1.0f / 60.0f,
		DebugMenu_GetComputeNoiseSettings(),
		BaseTerrainHeightResource());
	m_simulation.compute_water_surface_height_texture.InitializeState(
		BaseTerrainHeightResource().shaderResourceView(),
		m_simulation.compute_meteorograph_texture.Resource().shaderResourceView(),
		m_simulation.active_water_surface_height,
		DebugMenu_GetSurfaceWaterSimulationSettings());
	RefreshDerivedComputeResources(0.0);
	MeshFieldRenderer::SetRenderHeightSRV(
		ActiveTerrainHeightResource().shaderResourceView());
	MeshFieldRenderer::SetRenderNormalSRV(ActiveTerrainNormalResource().shaderResourceView());
	PublishSharedComputeResources();
	return true;
}

void Application::ReloadComputeResources()
{
	const ShaderReloadStatus reload_status = ShaderReload_ReloadAll();
	m_simulation.compute_task_runner.ReloadAll(Direct3D_GetDevice(), Direct3D_GetContext());
	DebugMenu_SetShaderReloadStatus(reload_status.succeeded, reload_status.message.c_str());
	if (!m_simulation.compute_noise_texture.IsValid())
	{
		DebugMenu_SetShaderReloadStatus(false, "Compute noise reload failed");
	}
	if (!m_simulation.compute_meteorograph_texture.IsValid())
	{
		DebugMenu_SetShaderReloadStatus(false, "Compute meteorograph reload failed");
	}
	if (m_simulation.compute_meteorograph_texture.IsValid())
	{
		m_simulation.compute_meteorograph_texture.Update(
			0.0f,
			1.0f / 60.0f,
			DebugMenu_GetComputeNoiseSettings(),
			BaseTerrainHeightResource());
	}
	if (!m_simulation.compute_water_surface_height_texture.IsValid())
	{
		DebugMenu_SetShaderReloadStatus(false, "Compute water surface height reload failed");
	}
	else if (!m_simulation.compute_water_surface_height_texture.HasBootstrappedState())
	{
		m_simulation.compute_water_surface_height_texture.InitializeState(
			BaseTerrainHeightResource().shaderResourceView(),
			m_simulation.compute_meteorograph_texture.Resource().shaderResourceView(),
			m_simulation.active_water_surface_height,
			DebugMenu_GetSurfaceWaterSimulationSettings());
	}
	else
	{
		m_simulation.compute_water_surface_height_texture.Update(
			BaseTerrainHeightResource().shaderResourceView(),
			m_simulation.compute_meteorograph_texture.RainResource().shaderResourceView(),
			m_simulation.compute_meteorograph_texture.Resource().shaderResourceView(),
			m_simulation.active_water_surface_height,
			DebugMenu_GetSurfaceWaterSimulationSettings(),
			false,
			0.0f,
			1.0f / 60.0f);
	}
	m_simulation.compute_terrain_classification_texture.Finalize();
	if (kEnableTerrainClassificationCompute &&
		!m_simulation.compute_terrain_classification_texture.Initialize(Direct3D_GetDevice(), Direct3D_GetContext()))
	{
		DebugMenu_SetShaderReloadStatus(false, "Compute terrain surface data reload failed");
	}
	m_simulation.compute_grass_data_texture.Finalize();
	if (kEnableGrassSystems &&
		!m_simulation.compute_grass_data_texture.Initialize(Direct3D_GetDevice(), Direct3D_GetContext()))
	{
		DebugMenu_SetShaderReloadStatus(false, "Compute grass data reload failed");
	}
	m_simulation.has_last_terrain_material_settings = false;
	m_simulation.terrain_classification_last_update_time = -1000.0;
}

void Application::DispatchComputeFrame(double current_time, double elapsed_time)
{
	m_simulation.compute_task_runner.Dispatch(current_time, elapsed_time);
	RefreshDerivedComputeResources(current_time);
}

void Application::RefreshDerivedComputeResources(double current_time)
{
	TerrainMaterialSettings terrain_material_settings = DebugMenu_GetTerrainMaterialSettings();
	terrain_material_settings.water_height = m_simulation.active_water_surface_height;
	if (kEnableTerrainClassificationCompute && m_simulation.compute_terrain_classification_texture.IsValid())
	{
		const bool first_classification_update = !m_simulation.has_last_terrain_material_settings;
		const bool material_settings_changed =
			!first_classification_update &&
			m_simulation.last_terrain_material_settings != terrain_material_settings;
		const bool interval_elapsed =
			current_time - m_simulation.terrain_classification_last_update_time >=
			kTerrainClassificationUpdateIntervalSeconds;
		if (first_classification_update || material_settings_changed || interval_elapsed)
		{
			m_simulation.compute_terrain_classification_texture.Update(
				SimulationTerrainHeightResource().shaderResourceView(),
				ActiveTerrainNormalResource().shaderResourceView(),
				m_simulation.compute_water_surface_height_texture.WaterInteractionResource().shaderResourceView(),
				m_simulation.compute_water_surface_height_texture.ErosionDeltaResource().shaderResourceView(),
				m_simulation.compute_meteorograph_texture.Resource().shaderResourceView(),
				terrain_material_settings);
			if (kEnableGrassSystems && m_simulation.compute_grass_data_texture.IsValid())
			{
				m_simulation.compute_grass_data_texture.Update(
					ActiveTerrainNormalResource().shaderResourceView(),
					m_simulation.compute_water_surface_height_texture.Resource().shaderResourceView(),
					m_simulation.compute_terrain_classification_texture.VegetationSuitabilityResource().shaderResourceView());
			}
			m_simulation.terrain_classification_last_update_time = current_time;
			m_simulation.last_terrain_material_settings = terrain_material_settings;
			m_simulation.has_last_terrain_material_settings = true;
		}
	}
}
