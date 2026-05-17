#ifndef COMPUTE_VISIBLE_WATER_TEXTURE_H
#define COMPUTE_VISIBLE_WATER_TEXTURE_H

#include "compute_task.h"
#include "render_shadow_map_resource.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ComputeShader;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11Buffer;

class ComputeVisibleWaterTexture : public ComputeTask
{
public:
	ComputeVisibleWaterTexture() = default;
	~ComputeVisibleWaterTexture() override = default;

	const char* DebugName() const override
	{
		return "ComputeVisibleWaterTexture";
	}

	ComputeTaskDispatchMode DispatchMode() const override
	{
		return ComputeTaskDispatchMode::EveryFrame;
	}

	ResourceSpan ReadResources() const override
	{
		static constexpr ComputeSharedResourceId kReadResources[] = {
			ComputeSharedResourceId::TerrainHeight,
			ComputeSharedResourceId::SurfaceWaterFlow
		};
		return kReadResources;
	}

	ResourceSpan WriteResources() const override
	{
		static constexpr ComputeSharedResourceId kWriteResources[] = {
			ComputeSharedResourceId::VisibleWater
		};
		return kWriteResources;
	}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
	void Finalize() override;
	void Update(
		ID3D11ShaderResourceView* terrain_height_srv,
		ID3D11ShaderResourceView* surface_water_flow_srv) const;
	bool IsValid() const override;
	Backend::RenderShaderResource Resource() const;

private:
	struct VisibleWaterConstants
	{
		float runoff_depth_min = 0.02f;
		float visible_depth_min = 0.10f;
		float standing_water_min = 0.22f;
		float slope_suppress_start = 0.08f;
		float slope_suppress_end = 0.34f;
		float flow_runoff_scale = 0.85f;
		float flow_visibility_suppress = 0.30f;
		float padding0 = 0.0f;
		unsigned int width = 0;
		unsigned int height = 0;
		unsigned int padding1 = 0;
		unsigned int padding2 = 0;
	};

	static constexpr unsigned int kTextureWidth = 257;
	static constexpr unsigned int kTextureHeight = 257;
	static constexpr unsigned int kThreadGroupSize = 8;

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;
	ID3D11ComputeShader* m_compute_shader = nullptr;
	ID3D11Texture2D* m_texture = nullptr;
	ID3D11ShaderResourceView* m_srv = nullptr;
	ID3D11UnorderedAccessView* m_uav = nullptr;
	ID3D11Buffer* m_constant_buffer = nullptr;
};

#endif // COMPUTE_VISIBLE_WATER_TEXTURE_H
