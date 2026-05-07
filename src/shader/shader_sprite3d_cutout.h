// ----------------------------------------------------
// 3D sprite cutout shader [shader_sprite3d_cutout.h]
// ====================================================
#ifndef SHADER_SPRITE3D_CUTOUT_H
#define SHADER_SPRITE3D_CUTOUT_H

#include <d3d11.h>
#include <DirectXMath.h>

bool ShaderSprite3D_Cutout_Initialize();
void ShaderSprite3D_Cutout_Finalize();

void ShaderSprite3D_Cutout_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
void ShaderSprite3D_Cutout_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void ShaderSprite3D_Cutout_SetProjMatrix(const DirectX::XMMATRIX& matrix);
void ShaderSprite3D_Cutout_SetMaterialColor(const DirectX::XMFLOAT4& material_color);
void ShaderSprite3D_Cutout_SetWindField(ID3D11ShaderResourceView* wind_field_srv);
void ShaderSprite3D_Cutout_SetWindSettings(float time_seconds,
										   const DirectX::XMFLOAT2& wind_direction,
										   float wind_strength,
										   float world_min_x,
										   float world_max_x,
										   float world_min_z,
										   float world_max_z);
void ShaderSprite3D_Cutout_Begin();
void ShaderSprite3D_Cutout_Clear();

#endif // SHADER_SPRITE3D_CUTOUT_H
