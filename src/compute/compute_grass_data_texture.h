#ifndef COMPUTE_GRASS_DATA_TEXTURE_H
#define COMPUTE_GRASS_DATA_TEXTURE_H

#include "compute_texture_dimensions.h"
#include "render_shadow_map_resource.h"

struct ID3D11Buffer;
struct ID3D11ComputeShader;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;
struct ID3D11UnorderedAccessView;

class ComputeGrassDataTexture
{
public:
	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
	void Finalize();
	void Update(
		ID3D11ShaderResourceView* terrain_normal_srv,
		ID3D11ShaderResourceView* terrain_height_srv,
		ID3D11ShaderResourceView* terrain_vegetation_suitability_srv);
	bool IsValid() const;
	Backend::RenderShaderResource Resource() const;

private:
	struct GrassDataConstants
	{
		float field_width = ComputeTextureDimensions::kWorldSideLength;
		float field_depth = ComputeTextureDimensions::kWorldSideLength;
		unsigned int width = 0;
		unsigned int height = 0;
	};

	static constexpr unsigned int kTextureWidth = ComputeTextureDimensions::kGrassDataResolution;
	static constexpr unsigned int kTextureHeight = ComputeTextureDimensions::kGrassDataResolution;
	static constexpr unsigned int kThreadGroupSize = 8;

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;
	ID3D11ComputeShader* m_compute_shader = nullptr;
	ID3D11Texture2D* m_texture = nullptr;
	ID3D11ShaderResourceView* m_srv = nullptr;
	ID3D11UnorderedAccessView* m_uav = nullptr;
	ID3D11Buffer* m_constant_buffer = nullptr;
};

#endif // COMPUTE_GRASS_DATA_TEXTURE_H
