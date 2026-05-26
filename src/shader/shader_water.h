#ifndef SHADER_WATER_H
#define SHADER_WATER_H

#include <d3d11.h>
#include <DirectXMath.h>

bool ShaderWater_Initialize();
void ShaderWater_Finalize();

void ShaderWater_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
void ShaderWater_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void ShaderWater_SetProjMatrix(const DirectX::XMMATRIX& matrix);
void ShaderWater_SetMaterialColor(const DirectX::XMFLOAT4& material_color);
void ShaderWater_SetCameraPosition(const DirectX::XMFLOAT3& camera_position);
void ShaderWater_SetSurfaceSettings(float highlight_strength);
void ShaderWater_SetTerrainHeight(ID3D11ShaderResourceView* terrain_height_srv);
void ShaderWater_SetWaterVelocity(ID3D11ShaderResourceView* water_velocity_srv);
void ShaderWater_SetWaterSediment(ID3D11ShaderResourceView* water_sediment_srv);
void ShaderWater_SetTerrainNormal(ID3D11ShaderResourceView* terrain_normal_srv);
void ShaderWater_SetSceneDepth(ID3D11ShaderResourceView* scene_depth_srv);
void ShaderWater_Begin();
void ShaderWater_End();

#endif
