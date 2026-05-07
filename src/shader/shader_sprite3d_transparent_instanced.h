#ifndef SHADER_SPRITE3D_TRANSPARENT_INSTANCED_H
#define SHADER_SPRITE3D_TRANSPARENT_INSTANCED_H

#include <d3d11.h>
#include <DirectXMath.h>

bool ShaderSprite3D_TransparentInstanced_Initialize();
void ShaderSprite3D_TransparentInstanced_Finalize();

void ShaderSprite3D_TransparentInstanced_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void ShaderSprite3D_TransparentInstanced_SetProjMatrix(const DirectX::XMMATRIX& matrix);
void ShaderSprite3D_TransparentInstanced_SetMaterialColor(const DirectX::XMFLOAT4& material_color);
void ShaderSprite3D_TransparentInstanced_Begin();
void ShaderSprite3D_TransparentInstanced_Clear();

#endif // SHADER_SPRITE3D_TRANSPARENT_INSTANCED_H
