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
void ShaderWater_SetSharedHeightfield(ID3D11ShaderResourceView* terrain_height_srv);
void ShaderWater_Begin();
void ShaderWater_End();

#endif
