#ifndef TERRAIN_DATA_MODEL_H
#define TERRAIN_DATA_MODEL_H

#include <DirectXMath.h>

#include "render_shadow_map_resource.h"
#include "terrain_surface_settings.h"

struct ID3D11ShaderResourceView;
class ComputeFinalTerrainHeightTexture;

struct TerrainMaterialClassification
{
	float grass_coverage = 0.0f;
	float vegetation_suitability = 0.0f;
	float grass_flatness = 0.0f;
	float shoreline = 0.0f;
	float lowland = 0.0f;
	float rock = 0.0f;
	bool grass_habitat = false;
};

struct TerrainSurfaceSample
{
	float height = 0.0f;
	DirectX::XMFLOAT3 normal{ 0.0f, 1.0f, 0.0f };
	float normal_y = 1.0f;
	TerrainMaterialClassification material{};
};

class TerrainDataModel
{
public:
	static void SetComputedHeightSource(const ComputeFinalTerrainHeightTexture* computed_height_source);
	static void ClearComputedHeightSource();
	static void SetMaterialSettings(const TerrainMaterialSettings& material_settings);
	static Backend::RenderShaderResource HeightResource();
	static ID3D11ShaderResourceView* HeightSRV();
	static float SampleHeightWorld(float world_x, float world_z);
	static DirectX::XMFLOAT3 SampleNormalWorld(float world_x, float world_z, float sample_offset);
	static float SampleNormalYWorld(float world_x, float world_z, float sample_offset);
	static TerrainMaterialClassification ClassifyMaterialWorld(float world_x, float world_z, float height, float normal_y);
	static TerrainSurfaceSample SampleSurfaceWorld(float world_x, float world_z, float normal_sample_offset);
	static bool IsGrassHabitatWorld(float world_x, float world_z, float height, float normal_y);
	static float ActiveWaterSurfaceHeight();
	static float SuggestedWaterHeight();
};

#endif // TERRAIN_DATA_MODEL_H
