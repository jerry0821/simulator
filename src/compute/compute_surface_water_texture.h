#ifndef COMPUTE_SURFACE_WATER_TEXTURE_H
#define COMPUTE_SURFACE_WATER_TEXTURE_H

#include "compute_task.h"
#include "compute_texture_dimensions.h"
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
			ComputeSharedResourceId::WaterSurfaceHeight,
			ComputeSharedResourceId::WaterVelocity,
			ComputeSharedResourceId::WaterSediment,
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
		ID3D11ShaderResourceView* water_surface_height_srv,
		ID3D11ShaderResourceView* authoritative_flow_srv,
		ID3D11ShaderResourceView* authoritative_velocity_srv,
		ID3D11ShaderResourceView* authoritative_sediment_srv,
		ID3D11ShaderResourceView* wind_field_srv,
		float time_seconds);
	bool IsValid() const override;
	Backend::RenderShaderResource Resource() const;
	Backend::RenderShaderResource FlowResource() const;
	Backend::RenderShaderResource FlowPreviewResource() const;

private:
	struct SurfaceWaterConstants
	{
		float time_seconds = 0.0f;
		float field_width = 512.0f;
		float field_depth = 512.0f;
		unsigned int width = 0;
		unsigned int height = 0;
		unsigned int padding0 = 0u;
		unsigned int padding1 = 0u;
		unsigned int padding2 = 0u;
	};

	static constexpr unsigned int kTextureWidth = ComputeTextureDimensions::kHydrologyResolution;
	static constexpr unsigned int kTextureHeight = ComputeTextureDimensions::kHydrologyResolution;
	static constexpr unsigned int kThreadGroupSize = 8;

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;
	ID3D11ComputeShader* m_compute_shader = nullptr;
	ID3D11Texture2D* m_surface_texture = nullptr;
	ID3D11ShaderResourceView* m_surface_srv = nullptr;
	ID3D11UnorderedAccessView* m_surface_uav = nullptr;
	ID3D11Texture2D* m_flow_texture = nullptr;
	ID3D11ShaderResourceView* m_flow_srv = nullptr;
	ID3D11UnorderedAccessView* m_flow_uav = nullptr;
	ID3D11Texture2D* m_flow_preview_texture = nullptr;
	ID3D11ShaderResourceView* m_flow_preview_srv = nullptr;
	ID3D11UnorderedAccessView* m_flow_preview_uav = nullptr;
	ID3D11Buffer* m_constant_buffer = nullptr;
};

#endif // COMPUTE_SURFACE_WATER_TEXTURE_H
