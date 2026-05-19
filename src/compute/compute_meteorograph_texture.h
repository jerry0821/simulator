#ifndef COMPUTE_METEOROGRAPH_TEXTURE_H
#define COMPUTE_METEOROGRAPH_TEXTURE_H

#include "compute_noise_texture.h"
#include "compute_task.h"
#include "render_shadow_map_resource.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ComputeShader;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11Buffer;

class ComputeMeteorographTexture : public ComputeTask
{
public:
	ComputeMeteorographTexture() = default;
	~ComputeMeteorographTexture() override = default;

	const char* DebugName() const override
	{
		return "ComputeMeteorographTexture";
	}

	ComputeTaskDispatchMode DispatchMode() const override
	{
		return ComputeTaskDispatchMode::EveryFrame;
	}

	ResourceSpan ReadResources() const override
	{
		static constexpr ComputeSharedResourceId kReadResources[] = {
			ComputeSharedResourceId::BaseTerrainHeight
		};
		return kReadResources;
	}

	ResourceSpan WriteResources() const override
	{
		static constexpr ComputeSharedResourceId kWriteResources[] = {
			ComputeSharedResourceId::RainMap,
			ComputeSharedResourceId::MeteorographField
		};
		return kWriteResources;
	}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
	void Finalize() override;

	void Update(
		float time_seconds,
		float delta_time_seconds,
		const ComputeNoiseSettings& settings,
		Backend::RenderShaderResource terrain_height) const;

	bool IsValid() const override;
	Backend::RenderShaderResource Resource() const;
	Backend::RenderShaderResource RainResource() const;

private:
	struct MeteorographConstants
	{
		float time_seconds = 0.0f;
		float delta_time_seconds = 1.0f / 60.0f;
		float wind_direction_x = 1.0f;
		float wind_direction_y = 0.0f;
		float wind_strength = 0.08f;
		float wind_cross_influence = 0.16f;
		float noise_scale = 12.0f;
		float pressure_scale = 0.0065f;
		float source_blend_rate = 0.20f;
		float propagation_scale = 0.55f;
		float humidity_advection = 0.08f;
		float temperature_relax = 0.10f;
		float rain_coupling = 0.22f;
		float rain_multiplier = 1.0f;
		float force_rain = 0.0f;
		float humidity_capacity = 0.86f;
		unsigned int width = 0;
		unsigned int height = 0;
		unsigned int initialize_state = 1u;
		unsigned int padding0 = 0u;
	};

	static constexpr unsigned int kTextureWidth = 256;
	static constexpr unsigned int kTextureHeight = 256;
	static constexpr unsigned int kThreadGroupSize = 8;

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;
	ID3D11ComputeShader* m_compute_shader = nullptr;
	ID3D11Texture2D* m_textures[2] = { nullptr, nullptr };
	ID3D11ShaderResourceView* m_srvs[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* m_uavs[2] = { nullptr, nullptr };
	ID3D11Texture2D* m_rain_texture = nullptr;
	ID3D11ShaderResourceView* m_rain_srv = nullptr;
	ID3D11UnorderedAccessView* m_rain_uav = nullptr;
	ID3D11Buffer* m_constant_buffer = nullptr;
	mutable unsigned int m_current_index = 0u;
	mutable bool m_has_state = false;
};

#endif // COMPUTE_METEOROGRAPH_TEXTURE_H
