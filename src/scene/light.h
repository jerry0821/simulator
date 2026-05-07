// ----------------------------------------------------
// ÉâÉCÉgÇÃê›íË [light.h]
// ====================================================
// Created by: Jerry
// Date: 2025-09-30
// Version: 1.0
// ----------------------------------------------------
#ifndef LIGHT_H
#define LIGHT_H

#include <d3d11.h>
#include <DirectXMath.h>

void Light_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Light_Finalize();
void Light_SetAmbient(const DirectX::XMFLOAT3& color);
void Light_SetDirectionalWorld(const DirectX::XMFLOAT4& world_directional,
	const DirectX::XMFLOAT4& color);

void Light_SetSpecularWorld(const DirectX::XMFLOAT3& camera_position,
	float power, const DirectX::XMFLOAT4& color);

void Light_SetPointLightCount(int count);
void Light_SetPointLight(int n,const DirectX::XMFLOAT3& position,
	float range, const DirectX::XMFLOAT3& color);

DirectX::XMMATRIX Light_GetLightViewProjectionMatrix();

#endif // LIGHT_H