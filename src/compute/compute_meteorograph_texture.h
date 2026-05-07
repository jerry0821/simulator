#ifndef COMPUTE_METEOROGRAPH_TEXTURE_H
#define COMPUTE_METEOROGRAPH_TEXTURE_H

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
		return ComputeTaskDispatchMode::FixedFrequency;
	}

	double FixedFrequencySeconds() const override
	{
		return 0.10;
	}

	ResourceSpan ReadResources() const override
	{
		static constexpr ComputeSharedResourceId kReadResources[] = {
			ComputeSharedResourceId::ClimateField,
			ComputeSharedResourceId::WindField
		};
		return kReadResources;
	}

	ResourceSpan WriteResources() const override
	{
		static constexpr ComputeSharedResourceId kWriteResources[] = {
			ComputeSharedResourceId::MeteorographField
		};
		return kWriteResources;
	}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
	void Finalize() override;

	void Update(
		float camera_world_x,
		float camera_world_z,
		Backend::RenderShaderResource climate_field,
		Backend::RenderShaderResource wind_field) const;

	bool IsValid() const override;
	Backend::RenderShaderResource Resource() const;

private:
	struct MeteorographConstants
	{
		float camera_world_x = 0.0f;
		float camera_world_z = 0.0f;
		float world_min_x = -640.0f;
		float world_min_z = -640.0f;
		float world_max_x = 640.0f;
		float world_max_z = 640.0f;
		float arrow_grid_cols = 20.0f;
		float arrow_grid_rows = 20.0f;
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

#endif // COMPUTE_METEOROGRAPH_TEXTURE_H
