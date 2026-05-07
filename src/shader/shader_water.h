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
void ShaderWater_SetSurfaceSettings(float fresnel_power,
									float highlight_strength,
									float time_seconds,
									float surface_center_x,
									float surface_center_z,
									float surface_size_x,
									float surface_size_z);
void ShaderWater_SetWaterSurfaceHeight(ID3D11ShaderResourceView* water_surface_height_srv);
void ShaderWater_SetSurfaceWater(ID3D11ShaderResourceView* surface_water_srv);
void ShaderWater_SetFlowField(ID3D11ShaderResourceView* flow_field_srv);
void ShaderWater_SetSceneDepth(ID3D11ShaderResourceView* scene_depth_srv);
void ShaderWater_Begin();
void ShaderWater_End();

#endif
