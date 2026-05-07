#ifndef COMPUTE_CLIMATE_TEXTURE_H
#define COMPUTE_CLIMATE_TEXTURE_H

#include "compute_task.h"
#include "render_shadow_map_resource.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ComputeShader;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11Buffer;

class ComputeClimateTexture : public ComputeTask
{
public:
	ComputeClimateTexture() = default;
	~ComputeClimateTexture() override = default;

	const char* DebugName() const override
	{
		return "ComputeClimateTexture";
	}

	ComputeTaskDispatchMode DispatchMode() const override
	{
		return ComputeTaskDispatchMode::FixedFrequency;
	}

	double FixedFrequencySeconds() const override
	{
		return 0.20;
	}

	ResourceSpan WriteResources() const override
	{
		static constexpr ComputeSharedResourceId kWriteResources[] = {
			ComputeSharedResourceId::ClimateField
		};
		return kWriteResources;
	}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
	void Finalize() override;

	void Update(float time_seconds) const;

	bool IsValid() const override;
	Backend::RenderShaderResource Resource() const;

private:
	struct ClimateConstants
	{
		float time_seconds = 0.0f;
		float temperature_scale = 4.5f;
		float humidity_scale = 5.5f;
		float rainfall_scale = 7.0f;
		float padding_x = 0.0f;
		float padding_y = 0.0f;
		float padding_z = 0.0f;
		float padding_w = 0.0f;
		unsigned int width = 0;
		unsigned int height = 0;
		unsigned int padding0 = 0;
		unsigned int padding1 = 0;
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

#endif // COMPUTE_CLIMATE_TEXTURE_H
