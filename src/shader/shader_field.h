/*==============================================================================

   フィールドシェーダー [shader_field.h]
														 Author : Youhei Sato
														 Date   : 2025/05/15
--------------------------------------------------------------------------------

==============================================================================*/
#ifndef SHADER_FIELD_H
#define	SHADER_FIELD_H

#include <d3d11.h>
#include <DirectXMath.h>

#include "render_shadow_map_resource.h"
#include "terrain_surface_settings.h"

bool ShaderField_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void ShaderField_Finalize();

void ShaderField_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
void ShaderField_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void ShaderField_SetProjectionMatrix(const DirectX::XMMATRIX& matrix);

void ShaderField_SetMaterialColor(const DirectX::XMFLOAT4& color);

void ShaderField_Begin();

void ShaderField_SetHeightMap(ID3D11ShaderResourceView* pSRV);
void ShaderField_SetTerrainNormalMap(ID3D11ShaderResourceView* pSRV);
void ShaderField_SetTerrainVegetationSuitabilityMap(ID3D11ShaderResourceView* pSRV);
void ShaderField_SetTerrainSurfaceDataMap(ID3D11ShaderResourceView* pSRV);

void ShaderField_SetLightViewProj(const DirectX::XMMATRIX& matrix); // 太陽の行列用
void ShaderField_SetShadowMap(const Backend::RenderShadowMapResource& shadow_map_resource);
void ShaderField_SetMeteorographMap(ID3D11ShaderResourceView* pSRV);
void ShaderField_SetTerrainMaterialSettings(const TerrainMaterialSettings& settings);
void ShaderField_SetTerrainSurfacePresentationEnabled(bool enabled);

#endif // SHADER_Field_H

