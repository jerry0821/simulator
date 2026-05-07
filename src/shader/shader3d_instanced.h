#ifndef SHADER3D_INSTANCED_H
#define SHADER3D_INSTANCED_H

#include <DirectXMath.h>

bool Shader3DInstanced_Initialize();
void Shader3DInstanced_Finalize();

void Shader3DInstanced_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void Shader3DInstanced_SetProjMatrix(const DirectX::XMMATRIX& matrix);
void Shader3DInstanced_SetMaterialColor(const DirectX::XMFLOAT4& material_color);
void Shader3DInstanced_Begin();

#endif // SHADER3D_INSTANCED_H
