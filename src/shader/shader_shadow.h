#ifndef SHADER_SHADOW_H
#define SHADER_SHADOW_H

#include <DirectXMath.h>

bool ShaderShadow_Initialize();
void ShaderShadow_Finalize();

void ShaderShadow_Begin();
void ShaderShadow_End();
void ShaderShadow_SetWorldMatrix(const DirectX::XMMATRIX& world);
void ShaderShadow_SetViewProjection(const DirectX::XMMATRIX& view_projection);
void ShaderShadow_SetLightViewProjection(const DirectX::XMMATRIX& light_view_projection);

#endif // SHADER_SHADOW_H
