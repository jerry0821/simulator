#ifndef COMPUTE_SURFACE_WATER_TEXTURE_H
#define COMPUTE_SURFACE_WATER_TEXTURE_H

#include <vector>

#include "compute_task.h"
#include "render_shadow_map_resource.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ComputeShader;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11Buffer;
struct SurfaceWaterSimulationSettings;

class ComputeSurfaceWaterTexture : public ComputeTask
{
public:
	ComputeSurfaceWaterTexture() = default;
	~ComputeSurfaceWaterTexture() override = default;

	const char* DebugName() const override
	{
		return "ComputeSurfaceWaterTexture";
	}

	ComputeTaskDispatchMode DispatchMode() const override
	{
		return ComputeTaskDispatchMode::EveryFrame;
	}

	ResourceSpan ReadResources() const override
	{
		static constexpr ComputeSharedResourceId kReadResources[] = {
			ComputeSharedResourceId::TerrainHeight,
			ComputeSharedResourceId::RainMap,
			ComputeSharedResourceId::WindField
		};
		return kReadResources;
	}

	ResourceSpan WriteResources() const override
	{
		static constexpr ComputeSharedResourceId kWriteResources[] = {
			ComputeSharedResourceId::SurfaceWater,
			ComputeSharedResourceId::WaterSurfaceHeight,
			ComputeSharedResourceId::SurfaceWaterFlow,
			ComputeSharedResourceId::SurfaceWaterFlowPreview,
			ComputeSharedResourceId::WaterVelocity,
			ComputeSharedResourceId::WaterSediment
		};
		return kWriteResources;
	}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
	void Finalize() override;
	void ClearState() const;
	void Update(
		ID3D11ShaderResourceView* terrain_height_srv,
		ID3D11ShaderResourceView* rain_map_srv,
		ID3D11ShaderResourceView* wind_field_srv,
		float water_height,
		const SurfaceWaterSimulationSettings& settings,
		bool inject_water_pulse,
		float time_seconds,
		float delta_time_seconds);
	bool HasBootstrappedState() const
	{
		return m_has_bootstrapped_state;
	}
	bool IsValid() const override;
	Backend::RenderShaderResource Resource() const;
	Backend::RenderShaderResource HeightfieldResource() const;
	Backend::RenderShaderResource FlowResource() const;
	Backend::RenderShaderResource FlowPreviewResource() const;
	Backend::RenderShaderResource VelocityResource() const;
	Backend::RenderShaderResource SedimentResource() const;
	bool ComputeWaterAmountRange(float& out_min_amount, float& out_max_amount) const;
	bool ComputeFlowMagnitudeRange(float& out_min_magnitude, float& out_max_magnitude) const;
	bool ComputeTerrainHeightRange(float& out_min_height, float& out_max_height) const;
	bool ComputeWaterHeightRange(float& out_min_height, float& out_max_height) const;
	bool ComputeWaterDepthRange(float& out_min_depth, float& out_max_depth) const;
	bool SampleHeightCell(
		unsigned int x,
		unsigned int y,
		float& out_terrain_height,
		float& out_water_height,
		float& out_water_depth) const;
	bool SampleCell(unsigned int x, unsigned int y, float& out_surface_water_amount, float& out_flow_sum) const;

private:
	struct SurfaceWaterConstants
	{
		float water_height = 0.0f;
		float accumulation_rate = 0.040f;
		float evaporation_rate = 0.0450f;
		float seepage_rate = 0.0040f;
		float basin_fade = 7.0f;
		float downhill_flow_rate = 0.48f;
		float flow_damping = 0.70f;
		float max_outflow_fraction = 0.68f;
		float field_width = 512.0f;
		float field_depth = 512.0f;
		float injection_center_x = 46.0f;
		float injection_center_z = 118.0f;
		float injection_radius = 14.0f;
		float injection_amount = 0.90f;
		float time_seconds = 0.0f;
		float delta_time_seconds = 1.0f / 60.0f;
		unsigned int width = 0;
		unsigned int height = 0;
		unsigned int presentation_only = 0u;
		unsigned int injection_enabled = 0u;
		unsigned int initialize_from_water_level = 0u;
		unsigned int padding1 = 0u;
		unsigned int padding2 = 0u;
	};

	static constexpr unsigned int kTextureWidth = 257;
	static constexpr unsigned int kTextureHeight = 257;
	static constexpr unsigned int kThreadGroupSize = 8;

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;
	ID3D11ComputeShader* m_compute_shader = nullptr;
	ID3D11Texture2D* m_textures[2] = { nullptr, nullptr };
	ID3D11ShaderResourceView* m_srvs[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* m_uavs[2] = { nullptr, nullptr };
	ID3D11Texture2D* m_heightfield_textures[2] = { nullptr, nullptr };
	ID3D11ShaderResourceView* m_heightfield_srvs[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* m_heightfield_uavs[2] = { nullptr, nullptr };
	ID3D11Texture2D* m_flow_textures[2] = { nullptr, nullptr };
	ID3D11ShaderResourceView* m_flow_srvs[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* m_flow_uavs[2] = { nullptr, nullptr };
	ID3D11Texture2D* m_flow_preview_texture = nullptr;
	ID3D11ShaderResourceView* m_flow_preview_srv = nullptr;
	ID3D11UnorderedAccessView* m_flow_preview_uav = nullptr;
	ID3D11Texture2D* m_velocity_texture = nullptr;
	ID3D11ShaderResourceView* m_velocity_srv = nullptr;
	ID3D11UnorderedAccessView* m_velocity_uav = nullptr;
	ID3D11Texture2D* m_sediment_texture = nullptr;
	ID3D11ShaderResourceView* m_sediment_srv = nullptr;
	ID3D11UnorderedAccessView* m_sediment_uav = nullptr;
	ID3D11Texture2D* m_readback_surface_texture = nullptr;
	ID3D11Texture2D* m_readback_heightfield_texture = nullptr;
	ID3D11Texture2D* m_readback_flow_texture = nullptr;
	ID3D11Buffer* m_constant_buffer = nullptr;
	unsigned int m_current_index = 0;
	mutable bool m_has_bootstrapped_state = false;
	mutable bool m_cpu_debug_data_ready = false;
	mutable std::vector<float> m_surface_water_amount_samples{};
	mutable std::vector<float> m_flow_magnitude_samples{};
	mutable std::vector<float> m_terrain_height_samples{};
	mutable std::vector<float> m_water_height_samples{};
};

#endif // COMPUTE_SURFACE_WATER_TEXTURE_H
