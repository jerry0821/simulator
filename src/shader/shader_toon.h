// ----------------------------------------------------
// シェーダートゥーン  [shader_toon.h]
// ====================================================
// Created by: Jerry
// Date: 2025-12-13
// ----------------------------------------------------
#ifndef SHADER_TOON_H
#define	SHADER_TOON_H

#include <d3d11.h>
#include <DirectXMath.h>

bool ShaderToon_Initialize();
void ShaderToon_Finalize();

//void ShaderToon_SetTransforms(const DirectX::XMMATRIX& world, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection);

void ShaderToon_SetWorldMatrix(const DirectX::XMMATRIX& world);

void ShaderToon_SetViewMatrix(const DirectX::XMMATRIX& view);

void ShaderToon_SetProjMatrix(const DirectX::XMMATRIX& proj);

void ShaderToon_SetMaterialColor(const DirectX::XMFLOAT4& color, int stepcount);

void ShaderToon_SetOutlineColor(const DirectX::XMFLOAT4& color = { 0.0f, 0.0f, 0.0f, 1.0f });

void ShaderToon_SetOutlineWidth(float width);

void ShaderToon_Begin_Outline();
void ShaderToon_Begin_Main();



#endif // SHADER_TOON_H
