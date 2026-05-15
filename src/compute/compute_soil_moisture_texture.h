#ifndef COMPUTE_SOIL_MOISTURE_TEXTURE_H
#define COMPUTE_SOIL_MOISTURE_TEXTURE_H

#include "compute_task.h"
#include "render_shadow_map_resource.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ComputeShader;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11Buffer;

class ComputeSoilMoistureTexture : public ComputeTask
{
public:
	ComputeSoilMoistureTexture() = default;
	~ComputeSoilMoistureTexture() override = default;

	const char* DebugName() const override
	{
		return "ComputeSoilMoistureTexture";
	}

	ComputeTaskDispatchMode DispatchMode() const override
	{
		return ComputeTaskDispatchMode::FixedFrequency;
	}

	double FixedFrequencySeconds() const override
	{
		return 0.25;
	}

	ResourceSpan ReadResources() const override
	{
		static constexpr ComputeSharedResourceId kReadResources[] = {
			ComputeSharedResourceId::RainMap,
			ComputeSharedResourceId::SurfaceWater,
			ComputeSharedResourceId::WaterMask
		};
		return kReadResources;
	}

	ResourceSpan WriteResources() const override
	{
		static constexpr ComputeSharedResourceId kWriteResources[] = {
			ComputeSharedResourceId::SoilMoisture
		};
		return kWriteResources;
	}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
	void Finalize() override;
	void Update(
		ID3D11ShaderResourceView* rain_map_srv,
		ID3D11ShaderResourceView* surface_water_srv,
		ID3D11ShaderResourceView* water_mask_srv);
	bool IsValid() const override;
	Backend::RenderShaderResource Resource() const;

private:
	struct SoilMoistureConstants
	{
		float rain_absorption = 0.16f;
		float wetness_absorption = 0.14f;
		float shoreline_absorption = 0.10f;
		float evaporation_rate = 0.045f;
		float diffusion_rate = 0.16f;
		float saturation_decay = 0.12f;
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

#endif // COMPUTE_SOIL_MOISTURE_TEXTURE_H
