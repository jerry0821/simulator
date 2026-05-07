#ifndef SHADER_GRASS_INSTANCED_H
#define SHADER_GRASS_INSTANCED_H

#include <d3d11.h>
#include <DirectXMath.h>

bool ShaderGrassInstanced_Initialize();
void ShaderGrassInstanced_Finalize();

void ShaderGrassInstanced_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void ShaderGrassInstanced_SetProjMatrix(const DirectX::XMMATRIX& matrix);
void ShaderGrassInstanced_SetMaterialColor(const DirectX::XMFLOAT4& material_color);
void ShaderGrassInstanced_SetWindField(ID3D11ShaderResourceView* wind_field_srv);
void ShaderGrassInstanced_SetWindSettings(float time_seconds, float field_uv_scale, float bend_scale);
void ShaderGrassInstanced_Begin();
void ShaderGrassInstanced_Clear();

#endif // SHADER_GRASS_INSTANCED_H
