#ifndef COMPUTE_RAIN_MAP_TEXTURE_H
#define COMPUTE_RAIN_MAP_TEXTURE_H

#include "compute_task.h"
#include "render_shadow_map_resource.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ComputeShader;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11Buffer;

class ComputeRainMapTexture : public ComputeTask
{
public:
	ComputeRainMapTexture() = default;
	~ComputeRainMapTexture() override = default;

	const char* DebugName() const override
	{
		return "ComputeRainMapTexture";
	}

	ComputeTaskDispatchMode DispatchMode() const override
	{
		return ComputeTaskDispatchMode::EveryFrame;
	}

	ResourceSpan ReadResources() const override
	{
		static constexpr ComputeSharedResourceId kReadResources[] = {
			ComputeSharedResourceId::MeteorographField
		};
		return kReadResources;
	}

	ResourceSpan WriteResources() const override
	{
		static constexpr ComputeSharedResourceId kWriteResources[] = {
			ComputeSharedResourceId::RainMap
		};
		return kWriteResources;
	}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
	void Finalize() override;
	void Update(
		ID3D11ShaderResourceView* meteorograph_srv,
		float rain_multiplier,
		float force_rain,
		float time_seconds) const;
	bool IsValid() const override;
	Backend::RenderShaderResource Resource() const;

private:
	struct RainMapConstants
	{
		float time_seconds = 0.0f;
		float rain_threshold = 0.28f;
		float rain_contrast = 2.15f;
		float rain_advection = 1.0f;
		float rain_multiplier = 1.0f;
		float force_rain = 0.0f;
		unsigned int width = 0;
		unsigned int height = 0;
		unsigned int padding0 = 0u;
		unsigned int padding1 = 0u;
		unsigned int padding2 = 0u;
		unsigned int padding3 = 0u;
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

#endif // COMPUTE_RAIN_MAP_TEXTURE_H
