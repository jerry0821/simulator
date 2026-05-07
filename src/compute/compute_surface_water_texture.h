#ifndef COMPUTE_SURFACE_WATER_TEXTURE_H
#define COMPUTE_SURFACE_WATER_TEXTURE_H

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
			ComputeSharedResourceId::SurfaceWaterFlow,
			ComputeSharedResourceId::SurfaceWaterFlowPreview
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
		float time_seconds);
	bool IsValid() const override;
	Backend::RenderShaderResource Resource() const;
	Backend::RenderShaderResource FlowResource() const;
	Backend::RenderShaderResource FlowPreviewResource() const;

private:
	struct SurfaceWaterConstants
	{
		float water_height = 0.0f;
		float accumulation_rate = 0.060f;
		float evaporation_rate = 0.006f;
		float seepage_rate = 0.006f;
		float basin_fade = 7.0f;
		float downhill_flow_rate = 0.28f;
		float flow_damping = 0.92f;
		float max_outflow_fraction = 0.72f;
		float field_width = 512.0f;
		float field_depth = 512.0f;
		float injection_center_x = 46.0f;
		float injection_center_z = 118.0f;
		float injection_radius = 14.0f;
		float injection_amount = 0.90f;
		float time_seconds = 0.0f;
		unsigned int width = 0;
		unsigned int height = 0;
		unsigned int injection_enabled = 0u;
		unsigned int padding0 = 0u;
		unsigned int padding1 = 0u;
	};

	static constexpr unsigned int kTextureWidth = 128;
	static constexpr unsigned int kTextureHeight = 128;
	static constexpr unsigned int kThreadGroupSize = 8;

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;
	ID3D11ComputeShader* m_compute_shader = nullptr;
	ID3D11Texture2D* m_textures[2] = { nullptr, nullptr };
	ID3D11ShaderResourceView* m_srvs[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* m_uavs[2] = { nullptr, nullptr };
	ID3D11Texture2D* m_flow_texture = nullptr;
	ID3D11ShaderResourceView* m_flow_srv = nullptr;
	ID3D11UnorderedAccessView* m_flow_uav = nullptr;
	ID3D11Texture2D* m_flow_preview_texture = nullptr;
	ID3D11ShaderResourceView* m_flow_preview_srv = nullptr;
	ID3D11UnorderedAccessView* m_flow_preview_uav = nullptr;
	ID3D11Buffer* m_constant_buffer = nullptr;
	unsigned int m_current_index = 0;
};

#endif // COMPUTE_SURFACE_WATER_TEXTURE_H
