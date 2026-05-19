#ifndef COMPUTE_WATER_SURFACE_HEIGHT_TEXTURE_H
#define COMPUTE_WATER_SURFACE_HEIGHT_TEXTURE_H

#include "compute_task.h"
#include "compute_texture_dimensions.h"
#include "render_shadow_map_resource.h"
#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ComputeShader;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11Buffer;
struct SurfaceWaterSimulationSettings;

class ComputeWaterSurfaceHeightTexture : public ComputeTask
{
public:
	ComputeWaterSurfaceHeightTexture() = default;
	~ComputeWaterSurfaceHeightTexture() override = default;

	const char* DebugName() const override
	{
		return "ComputeWaterSurfaceHeightTexture";
	}

	ComputeTaskDispatchMode DispatchMode() const override
	{
		return ComputeTaskDispatchMode::EveryFrame;
	}

	ResourceSpan ReadResources() const override
	{
		static constexpr ComputeSharedResourceId kReadResources[] = {
			ComputeSharedResourceId::BaseTerrainHeight,
			ComputeSharedResourceId::RainMap
		};
		return kReadResources;
	}

	ResourceSpan WriteResources() const override
	{
		static constexpr ComputeSharedResourceId kWriteResources[] = {
			ComputeSharedResourceId::WaterSurfaceHeight,
			ComputeSharedResourceId::WaterVelocity,
			ComputeSharedResourceId::WaterSediment,
			ComputeSharedResourceId::ErosionDelta
		};
		return kWriteResources;
	}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
	void Finalize() override;
	void ResetState();
	void InitializeState(
		ID3D11ShaderResourceView* terrain_height_srv,
		float water_surface_height,
		const SurfaceWaterSimulationSettings& settings) const;
	void Update(
		ID3D11ShaderResourceView* terrain_height_srv,
		ID3D11ShaderResourceView* rain_map_srv,
		float water_surface_height,
		const SurfaceWaterSimulationSettings& settings,
		bool inject_water_pulse,
		float delta_time_seconds) const;
	bool IsValid() const override;
	bool HasBootstrappedState() const
	{
		return m_has_bootstrapped_state;
	}
	Backend::RenderShaderResource Resource() const;
	Backend::RenderShaderResource FlowResource() const;
	Backend::RenderShaderResource VelocityResource() const;
	Backend::RenderShaderResource SedimentResource() const;
	Backend::RenderShaderResource ErosionDeltaResource() const;
	bool ComputeTerrainHeightRange(float& out_min_height, float& out_max_height) const;
	bool ComputeWaterHeightRange(float& out_min_height, float& out_max_height) const;

private:
	struct WaterSurfaceHeightConstants
	{
		float water_surface_height = 0.0f;
		float minimum_depth_for_surface = 0.004f;
		float flow_rate = 0.24f;
		float max_outflow_fraction = 0.55f;
		float evaporation_rate = 0.01f;
		float accumulation_rate = 0.0f;
		float seepage_rate = 0.004f;
		float basin_fade = 6.0f;
		float field_width = 512.0f;
		float field_depth = 512.0f;
		float injection_center_x = 46.0f;
		float injection_center_z = 118.0f;
		float injection_radius = 14.0f;
		float injection_amount = 0.90f;
		float delta_time_seconds = 1.0f / 60.0f;
		unsigned int width = 0;
		unsigned int height = 0;
		unsigned int initialize_from_water_level = 0u;
		unsigned int injection_enabled = 0u;
		unsigned int padding1 = 0u;
		unsigned int padding2 = 0u;
		unsigned int padding3 = 0u;
		unsigned int padding4 = 0u;
		unsigned int padding5 = 0u;
	};

	static constexpr unsigned int kTextureWidth = ComputeTextureDimensions::kHydrologyResolution;
	static constexpr unsigned int kTextureHeight = ComputeTextureDimensions::kHydrologyResolution;
	static constexpr unsigned int kThreadGroupSize = 8;

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;
	ID3D11ComputeShader* m_compute_shader = nullptr;
	ID3D11Texture2D* m_textures[2] = { nullptr, nullptr };
	ID3D11ShaderResourceView* m_srvs[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* m_uavs[2] = { nullptr, nullptr };
	ID3D11Texture2D* m_flow_textures[2] = { nullptr, nullptr };
	ID3D11ShaderResourceView* m_flow_srvs[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* m_flow_uavs[2] = { nullptr, nullptr };
	ID3D11Texture2D* m_velocity_textures[2] = { nullptr, nullptr };
	ID3D11ShaderResourceView* m_velocity_srvs[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* m_velocity_uavs[2] = { nullptr, nullptr };
	ID3D11Texture2D* m_sediment_textures[2] = { nullptr, nullptr };
	ID3D11ShaderResourceView* m_sediment_srvs[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* m_sediment_uavs[2] = { nullptr, nullptr };
	ID3D11Texture2D* m_erosion_delta_texture = nullptr;
	ID3D11ShaderResourceView* m_erosion_delta_srv = nullptr;
	ID3D11UnorderedAccessView* m_erosion_delta_uav = nullptr;
	ID3D11Texture2D* m_readback_texture = nullptr;
	ID3D11Buffer* m_constant_buffer = nullptr;
	mutable unsigned int m_current_index = 0u;
	mutable bool m_has_bootstrapped_state = false;
	mutable bool m_cpu_height_data_ready = false;
	mutable std::vector<float> m_terrain_height_samples{};
	mutable std::vector<float> m_water_height_samples{};
};

#endif // COMPUTE_WATER_SURFACE_HEIGHT_TEXTURE_H
