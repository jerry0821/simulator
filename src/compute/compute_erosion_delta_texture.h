#ifndef COMPUTE_EROSION_DELTA_TEXTURE_H
#define COMPUTE_EROSION_DELTA_TEXTURE_H

#include "compute_task.h"
#include "render_shadow_map_resource.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ComputeShader;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11Buffer;

class ComputeErosionDeltaTexture : public ComputeTask
{
public:
	ComputeErosionDeltaTexture() = default;
	~ComputeErosionDeltaTexture() override = default;

	const char* DebugName() const override
	{
		return "ComputeErosionDeltaTexture";
	}

	ComputeTaskDispatchMode DispatchMode() const override
	{
		return ComputeTaskDispatchMode::FixedFrequency;
	}

	double FixedFrequencySeconds() const override
	{
		return 0.50;
	}

	ResourceSpan ReadResources() const override
	{
		static constexpr ComputeSharedResourceId kReadResources[] = {
			ComputeSharedResourceId::TerrainHeight,
			ComputeSharedResourceId::SurfaceWater,
			ComputeSharedResourceId::WaterVelocity,
			ComputeSharedResourceId::WaterSediment
		};
		return kReadResources;
	}

	ResourceSpan WriteResources() const override
	{
		static constexpr ComputeSharedResourceId kWriteResources[] = {
			ComputeSharedResourceId::ErosionDelta
		};
		return kWriteResources;
	}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
	void Finalize() override;
	void Update(
		ID3D11ShaderResourceView* terrain_height_srv,
		ID3D11ShaderResourceView* surface_water_srv,
		ID3D11ShaderResourceView* water_velocity_srv,
		ID3D11ShaderResourceView* water_sediment_srv);
	bool IsValid() const override;
	Backend::RenderShaderResource Resource() const;

private:
	struct ErosionDeltaConstants
	{
		float erosion_rate = 0.028f;
		float deposition_rate = 0.018f;
		float thermal_rate = 0.045f;
		float relaxation_rate = 0.006f;
		float max_erosion = 0.12f;
		float max_deposition = 0.09f;
		float thermal_threshold = 0.42f;
		float hydraulic_bias = 0.56f;
		unsigned int width = 0;
		unsigned int height = 0;
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
	ID3D11Texture2D* m_output_texture = nullptr;
	ID3D11ShaderResourceView* m_output_srv = nullptr;
	ID3D11Buffer* m_constant_buffer = nullptr;
	unsigned int m_current_index = 0;
};

#endif // COMPUTE_EROSION_DELTA_TEXTURE_H
