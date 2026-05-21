#ifndef TERRAIN_HEIGHT_FIELD_H
#define TERRAIN_HEIGHT_FIELD_H

#include <d3d11.h>

#include "render_shadow_map_resource.h"

struct TerrainSettings;

class TerrainHeightField
{
public:
	static bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
	static void Finalize();
	static void SetFlatMode(bool is_flat);
	static bool IsFlatMode();
	static void ApplyTerrainSettings(const TerrainSettings& settings);
	static const TerrainSettings& GetTerrainSettings();
	static float GetSuggestedWaterHeight();
	static float FieldWidth();
	static float FieldDepth();
	static Backend::RenderShaderResource HeightResource();
	static ID3D11ShaderResourceView* HeightSRV();
	static float GetHeight(float x, float z);
};

#endif // TERRAIN_HEIGHT_FIELD_H
