#ifndef COMPUTE_NOISE_TEXTURE_H
#define COMPUTE_NOISE_TEXTURE_H

#include "compute_task.h"
#include "render_shadow_map_resource.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ComputeShader;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11Buffer;

struct ComputeNoiseSettings
{
	float noise_scale = 32.0f;
	float wind_direction_x = 0.85f;
	float wind_direction_y = 0.35f;
	float wind_strength = 0.06f;
	float wind_cross_influence = 0.55f;
	float flow_speed_x0 = 0.06f;
	float flow_speed_y0 = 0.03f;
	float flow_speed_x1 = -0.04f;
	float flow_speed_y1 = 0.05f;
	float band_strength = 0.10f;
	float contrast = 1.0f;
	float rain_multiplier = 1.0f;
	float force_rain = 0.0f;
};

class ComputeNoiseTexture : public ComputeTask
{
public:
	ComputeNoiseTexture() = default;
	~ComputeNoiseTexture() override = default;

	const char* DebugName() const override
	{
		return "ComputeNoiseTexture";
	}

	ComputeTaskDispatchMode DispatchMode() const override
	{
		return ComputeTaskDispatchMode::EveryFrame;
	}

	ResourceSpan WriteResources() const override
	{
		static constexpr ComputeSharedResourceId kWriteResources[] = {
			ComputeSharedResourceId::ComputeNoise
		};
		return kWriteResources;
	}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
	void Finalize() override;

	void Update(float time_seconds, const ComputeNoiseSettings& settings) const;

	bool IsValid() const override;
	Backend::RenderShaderResource Resource() const;

private:
	struct NoiseConstants
	{
		float time_seconds = 0.0f;
		float noise_scale = 32.0f;
		float flow_speed_x0 = 0.06f;
		float flow_speed_y0 = 0.03f;
		float flow_speed_x1 = -0.04f;
		float flow_speed_y1 = 0.05f;
		float band_strength = 0.10f;
		float contrast = 1.0f;
		unsigned int width = 0;
		unsigned int height = 0;
		unsigned int padding0 = 0;
		unsigned int padding1 = 0;
	};

	static constexpr unsigned int kTextureWidth = 256;
	static constexpr unsigned int kTextureHeight = 256;
	static constexpr unsigned int kThreadGroupSize = 8;

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;
	ID3D11ComputeShader* m_compute_shader = nullptr;
	ID3D11Texture2D* m_texture = nullptr;
	ID3D11ShaderResourceView* m_srv = nullptr;
	ID3D11UnorderedAccessView* m_uav = nullptr;
	ID3D11Buffer* m_constant_buffer = nullptr;
};

#endif // COMPUTE_NOISE_TEXTURE_H
