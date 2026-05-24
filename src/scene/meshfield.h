// ----------------------------------------------------
// Terrain mesh field [meshfield.h]
// ====================================================
// Created by: Jerry
// Date: 2025-09-19
// Version: 1.0
// ----------------------------------------------------
#ifndef MESHFIELD_H
#define MESHFIELD_H

#include <d3d11.h>
#include <DirectXMath.h>

#include "render_shadow_map_resource.h"

struct TerrainSettings
{
	float base_frequency = 0.0031f;
	float base_height = 24.0f;
	float detail_frequency = 1.7f;
	float detail_height = 3.2f;
	float ridge_frequency = 0.78f;
	float ridge_height = 26.0f;
	float continent_height = 6.5f;
	float lake_center_z = 0.0f;
	float lake_radius_x = 32.0f;
	float lake_radius_z = 32.0f;
	float lake_depth = 0.0f;

	bool operator==(const TerrainSettings& other) const
	{
		return base_frequency == other.base_frequency &&
			base_height == other.base_height &&
			detail_frequency == other.detail_frequency &&
			detail_height == other.detail_height &&
			ridge_frequency == other.ridge_frequency &&
			ridge_height == other.ridge_height &&
			continent_height == other.continent_height &&
			lake_center_z == other.lake_center_z &&
			lake_radius_x == other.lake_radius_x &&
			lake_radius_z == other.lake_radius_z &&
			lake_depth == other.lake_depth;
	}

	bool operator!=(const TerrainSettings& other) const
	{
		return !(*this == other);
	}
};

class MeshFieldRenderer
{
public:
	static void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
	static void Finalize();
	static void Draw();
	static void SetFlatMode(bool is_flat);
	static void DrawMeshOnly();
	static float GetHeight(float x, float z);
	static void ApplyTerrainSettings(const TerrainSettings& settings);
	static const TerrainSettings& GetTerrainSettings();
	static float GetSuggestedWaterHeight();
	static float FieldWidth();
	static float FieldDepth();
	static Backend::RenderShaderResource HeightResource();
	static ID3D11ShaderResourceView* HeightSRV();
	static void SetRenderHeightSRV(ID3D11ShaderResourceView* srv);
	static void SetRenderNormalSRV(ID3D11ShaderResourceView* srv);
};

void MeshField_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void MeshField_Finalize(void);
void MeshField_Draw();
void MeshField_SetFlatMode(bool isFlat);

void MeshField_DrawMeshOnly();

float MeshField_GetHeight(float x, float z);

#endif // MESHFIELD_H

