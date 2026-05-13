#ifndef COMPUTE_TERRAIN_NORMAL_TEXTURE_H
#define COMPUTE_TERRAIN_NORMAL_TEXTURE_H

#include "compute_task.h"
#include "render_shadow_map_resource.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ComputeShader;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11Buffer;

class ComputeTerrainNormalTexture : public ComputeTask
{
public:
	ComputeTerrainNormalTexture() = default;
	~ComputeTerrainNormalTexture() override = default;

	const char* DebugName() const override
	{
		return "ComputeTerrainNormalTexture";
	}

	ComputeTaskDispatchMode DispatchMode() const override
	{
		return ComputeTaskDispatchMode::Manual;
	}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
	void Finalize() override;
	void Update(ID3D11ShaderResourceView* terrain_height_srv);
	bool IsValid() const override;
	Backend::RenderShaderResource Resource() const;

private:
	struct TerrainNormalConstants
	{
		float field_width = 512.0f;
		float field_depth = 512.0f;
		unsigned int width = 0;
		unsigned int height = 0;
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

#endif // COMPUTE_TERRAIN_NORMAL_TEXTURE_H
