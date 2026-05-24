// ----------------------------------------------------
// ライトの設定 [light.h]
// ====================================================
// Created by: Jerry
// Date: 2025-09-30
// Version: 1.0
// ----------------------------------------------------

#include "light.h"
using namespace DirectX;
#include "direct3d.h"


static ID3D11Buffer* g_pPSConstantBuffer1 = nullptr; // ambient
static ID3D11Buffer* g_pPSConstantBuffer2 = nullptr; // directional
static ID3D11Buffer* g_pPSConstantBuffer3 = nullptr; // specular
static ID3D11Buffer* g_pPSConstantBuffer4 = nullptr; // point

// 注意！初期化で外部から設定されるもの。Release不要。
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

static XMFLOAT4 g_CurrentLightDir = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);

// 並行光源
struct DirectionalLight 
{
	XMFLOAT4 Directional;
	XMFLOAT4 Color;
};

// 鏡面反射光源
struct SpecularLight
{
	XMFLOAT3 CameraPosition;
	float Power;
	XMFLOAT4 Color;
};

// 点光源(ポイントライト)
struct PointLight
{
	XMFLOAT3 LightPosition;
	float Range;
	XMFLOAT4 Color;
	//float SpecularPower;
	//XMFLOAT3 SpecularColor;
};

struct PointLightList
{
	PointLight light[4];
	int count;
	XMFLOAT3 dummy;
};

static PointLightList g_PointLights{};
static SpecularLight g_CurrentSpecularLight{
	XMFLOAT3(0.0f, 0.0f, 0.0f),
	20.0f,
	XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f)
};

void Light_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	// デバイスとデバイスコンテキストの保存
	g_pDevice = pDevice;
	g_pContext = pContext;

	// 頂点シェーダー用定数バッファの作成
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; // バインドフラグ

	buffer_desc.ByteWidth = sizeof(XMFLOAT4); // バッファのサイズ
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer1); // ambient

	buffer_desc.ByteWidth = sizeof(DirectionalLight); // バッファのサイズ
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer2); // directional

	buffer_desc.ByteWidth = sizeof(SpecularLight); // バッファのサイズ
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer3); // specular

	buffer_desc.ByteWidth = sizeof(PointLightList); // バッファのサイズ
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer4); // point

}

void Light_Finalize()
{
	SAFE_RELEASE(g_pPSConstantBuffer4);
	SAFE_RELEASE(g_pPSConstantBuffer3);
	SAFE_RELEASE(g_pPSConstantBuffer2);
	SAFE_RELEASE(g_pPSConstantBuffer1);
}

void Light_SetAmbient(const DirectX::XMFLOAT3& color)
{
	const XMFLOAT4 padded_color(color.x, color.y, color.z, 1.0f);
	// 定数バッファアンビエントをセット
	g_pContext->UpdateSubresource(g_pPSConstantBuffer1, 0, nullptr, &padded_color, 0, 0);
	g_pContext->PSSetConstantBuffers(1, 1, &g_pPSConstantBuffer1);
}


void Light_SetDirectionalWorld(const DirectX::XMFLOAT4& world_directional, const DirectX::XMFLOAT4& color)
{
	g_CurrentLightDir = world_directional;
	DirectionalLight d_light{ world_directional,color };

	g_pContext->UpdateSubresource(g_pPSConstantBuffer2, 0, nullptr, &d_light, 0, 0);
	g_pContext->PSSetConstantBuffers(2, 1, &g_pPSConstantBuffer2);
}

void Light_SetSpecularWorld(const DirectX::XMFLOAT3& camera_position, float power, const DirectX::XMFLOAT4& color)
{
	g_CurrentSpecularLight = SpecularLight{ camera_position,power,color };
	g_pContext->UpdateSubresource(g_pPSConstantBuffer3, 0, nullptr, &g_CurrentSpecularLight, 0, 0);
	g_pContext->PSSetConstantBuffers(3, 1, &g_pPSConstantBuffer3);
}

void Light_SetCameraPosition(const DirectX::XMFLOAT3& camera_position)
{
	g_CurrentSpecularLight.CameraPosition = camera_position;
	g_pContext->UpdateSubresource(g_pPSConstantBuffer3, 0, nullptr, &g_CurrentSpecularLight, 0, 0);
	g_pContext->PSSetConstantBuffers(3, 1, &g_pPSConstantBuffer3);
}

void Light_SetPointLightCount(int count)
{
	g_PointLights.count = count;

	g_pContext->UpdateSubresource(g_pPSConstantBuffer4, 0, nullptr, &g_PointLights, 0, 0);
	g_pContext->PSSetConstantBuffers(4, 1, &g_pPSConstantBuffer4);

}

void Light_SetPointLight(int n, const DirectX::XMFLOAT3& position, float range, const DirectX::XMFLOAT3& color)
{
	g_PointLights.light[n].LightPosition = position;
	g_PointLights.light[n].Range = range;
	g_PointLights.light[n].Color = XMFLOAT4(color.x, color.y, color.z, 1.0f);

	g_pContext->UpdateSubresource(g_pPSConstantBuffer4, 0, nullptr, &g_PointLights, 0, 0);
	g_pContext->PSSetConstantBuffers(4, 1, &g_pPSConstantBuffer4);

}

DirectX::XMMATRIX Light_GetLightViewProjectionMatrix() {
	XMVECTOR lightDir = XMLoadFloat4(&g_CurrentLightDir);
	lightDir = XMVector3Normalize(lightDir);

	XMVECTOR targetPos = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR lightPos = targetPos - (lightDir * 100.0f);

	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

	float sceneSize = 160.0f;

	XMMATRIX lightProj = XMMatrixOrthographicLH(sceneSize, sceneSize, 1.0f, 1000.0f);

	return lightView * lightProj;
}
