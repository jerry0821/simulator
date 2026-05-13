#ifndef COMPUTE_TERRAIN_CLASSIFICATION_TEXTURE_H
#define COMPUTE_TERRAIN_CLASSIFICATION_TEXTURE_H

#include "render_shadow_map_resource.h"
#include "terrain_surface_settings.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ComputeShader;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11Buffer;

class ComputeTerrainClassificationTexture
{
public:
	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
	void Finalize();
	void Update(
		ID3D11ShaderResourceView* terrain_height_srv,
		ID3D11ShaderResourceView* terrain_normal_srv,
		ID3D11ShaderResourceView* water_interaction_srv,
		ID3D11ShaderResourceView* erosion_delta_srv,
		ID3D11ShaderResourceView* climate_srv,
		const TerrainMaterialSettings& material_settings);
	bool IsValid() const;
	Backend::RenderShaderResource Resource() const;
	Backend::RenderShaderResource VegetationSuitabilityResource() const;

private:
	struct TerrainClassificationConstants
	{
		float grass_slope_min = 0.55f;
		float grass_slope_max = 0.88f;
		float grass_noise_strength = 0.18f;
		float grass_height_start = 7.5f;
		float grass_height_end = 16.0f;
		float rock_slope_start = 0.45f;
		float rock_slope_end = 0.82f;
		float rock_height_start = 9.0f;
		float rock_height_end = 18.0f;
		float stone_noise_scale = 0.035f;
		float shoreline_offset_start = 0.55f;
		float shoreline_offset_end = 5.5f;
		float lowland_height_start = 26.0f;
		float lowland_height_end = 58.0f;
		float grass_coverage_min = 0.28f;
		float water_height = 0.0f;
		float field_width = 512.0f;
		float field_depth = 512.0f;
		float sample_offset = 2.2f;
		float wetness_gain = 1.0f;
		unsigned int width = 0;
		unsigned int height = 0;
		unsigned int padding0 = 0;
		unsigned int padding1 = 0;
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
	ID3D11Texture2D* m_vegetation_texture = nullptr;
	ID3D11ShaderResourceView* m_vegetation_srv = nullptr;
	ID3D11UnorderedAccessView* m_vegetation_uav = nullptr;
	ID3D11Buffer* m_constant_buffer = nullptr;
};

#endif // COMPUTE_TERRAIN_CLASSIFICATION_TEXTURE_H
