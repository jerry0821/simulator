// ----------------------------------------------------
// シェーダー3D(ライトなし)  [shader3d_unlit.h]
// ====================================================
// Created by: Jerry
// Date: 2025-11-21
// ----------------------------------------------------
#ifndef SHADER3D_UNLIT_H
#define	SHADER3D_UNLIT_H

#include <d3d11.h>
#include <DirectXMath.h>

bool Shader3D_Unlit_Initialize();
void Shader3D_Unlit_Finalize();
			 
void Shader3D_Unlit_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
void Shader3D_Unlit_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void Shader3D_Unlit_SetProjMatrix(const DirectX::XMMATRIX& matrix);
			 
void Shader3D_Unlit_SetMaterialColor(const DirectX::XMFLOAT4& material_color);
			 
void Shader3D_Unlit_Begin();

#endif // SHADER3D_H

