#ifndef COMPUTE_WATER_SURFACE_HEIGHT_TEXTURE_H
#define COMPUTE_WATER_SURFACE_HEIGHT_TEXTURE_H

#include "compute_task.h"
#include "render_shadow_map_resource.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ComputeShader;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11Buffer;

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
			ComputeSharedResourceId::TerrainHeight,
			ComputeSharedResourceId::SurfaceWater
		};
		return kReadResources;
	}

	ResourceSpan WriteResources() const override
	{
		static constexpr ComputeSharedResourceId kWriteResources[] = {
			ComputeSharedResourceId::WaterSurfaceHeight
		};
		return kWriteResources;
	}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
	void Finalize() override;
	void Update(
		ID3D11ShaderResourceView* terrain_height_srv,
		ID3D11ShaderResourceView* surface_water_srv,
		float water_surface_height) const;
	bool IsValid() const override;
	Backend::RenderShaderResource Resource() const;

private:
	struct WaterSurfaceHeightConstants
	{
		float water_surface_height = 0.0f;
		float standing_water_flatten_start = 0.22f;
		float standing_water_flatten_end = 0.82f;
		float minimum_depth_for_surface = 0.004f;
		unsigned int width = 0;
		unsigned int height = 0;
		unsigned int padding0 = 0u;
		unsigned int padding1 = 0u;
	};

	static constexpr unsigned int kTextureWidth = 128;
	static constexpr unsigned int kTextureHeight = 128;
	static constexpr unsigned int kThreadGroupSize = 8;

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;
	ID3D11ComputeShader* m_compute_shader = nullptr;
	ID3D11Texture2D* m_texture = nullptr;
	ID3D11ShaderResourceView* m_srv = nullptr;
	ID3D11UnorderedAccessView* m_uav = nullptr;
	ID3D11Buffer* m_constant_buffer = nullptr;
};

#endif // COMPUTE_WATER_SURFACE_HEIGHT_TEXTURE_H
