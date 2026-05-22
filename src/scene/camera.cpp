// ----------------------------------------------------
// カメラ制御 [camera.cpp]
// ====================================================
// Created by: Jerry
// Date: 2025-09-11
// Version: 1.0
// ----------------------------------------------------
#include "camera.h"
#include "direct3d.h"
#include <DirectXMath.h>
using namespace DirectX;
#include "key_logger.h"
#include "debug_text.h"
#include "mouse.h"
#include <sstream>
#include <windows.h>

static XMFLOAT3 g_CameraPos = {  0.0f, 0.0f, -5.0f }; // カメラの位置
static XMFLOAT3 g_CameraVector_Front = { 0.0f, 0.0f, 1.0f }; // 
static XMFLOAT3 g_CameraVector_Up = { 0.0f, 1.0f, 0.0f }; // 
static XMFLOAT3 g_CameraVector_Right = { 1.0f, 0.0f, 0.0f }; // 
float g_CameraMoveSpeed = 20.0f; // カメラの移動速度
static constexpr float g_CameraRotateSpeed = XMConvertToRadians(30.0f); // カメラの回転速度
static XMFLOAT4X4 g_CameraMatrix;
static XMFLOAT4X4 g_PerspectiveMatrix;
static float g_Fov = XMConvertToRadians(60);

static hal::DebugText* g_pDT = nullptr;

static ID3D11Buffer* g_pVSConstantBuffer1 = nullptr; // World Matrix
static ID3D11Buffer* g_pVSConstantBuffer2 = nullptr; // View Matrix

static int g_camPrevMouseX = 0;
static int g_camPrevMouseY = 0;
static int g_camPrevScrollVal = 0;

void Camera_Initialize(const DirectX::XMFLOAT3& position,
	const DirectX::XMFLOAT3& front,
	const DirectX::XMFLOAT3& right)
{
	Camera_Initialize();
	g_CameraPos = position;
	XMVECTOR f = XMVector3Normalize(XMLoadFloat3(&front));
	XMVECTOR r = XMVector3Normalize(XMLoadFloat3(&right) * XMVECTOR { 1.0f, 0.0f, 1.0f });
	XMVECTOR u = XMVector3Normalize(XMVector3Cross(f, r));

	XMStoreFloat3(&g_CameraVector_Front, f);
	XMStoreFloat3(&g_CameraVector_Right, r);
	XMStoreFloat3(&g_CameraVector_Up, u);
}

void Camera_Initialize()
{
	g_CameraPos = { 0.0f, 0.0f, -5.0f };
	g_CameraVector_Front = { 0.0f, 0.0f, 1.0f };
	g_CameraVector_Up = { 0.0f, 1.0f, 0.0f };
	g_CameraVector_Right = { 1.0f, 0.0f, 0.0f };
	g_Fov = XMConvertToRadians(60);

	XMStoreFloat4x4(&g_CameraMatrix, XMMatrixIdentity());
	XMStoreFloat4x4(&g_PerspectiveMatrix, XMMatrixIdentity());

	// 頂点シェーダー用定数バッファの作成
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(XMFLOAT4X4); // バッファのサイズ
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; // バインドフラグ
	Direct3D_GetDevice()->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer1);
	Direct3D_GetDevice()->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer2);

#if defined(DEBUG) || defined(_DEBUG)
	g_pDT = new hal::DebugText(Direct3D_GetDevice(), Direct3D_GetContext(),
		L"resource/texture/consolab_ascii_512.png", Direct3D_GetBackBufferWidth(), Direct3D_GetBackBufferHeight(),
		0.0f, 32.0f,
		0, 0,
		0.0f, 14.0f);
#endif
}

void Camera_Finalize()
{
	SAFE_RELEASE(g_pVSConstantBuffer2);
	SAFE_RELEASE(g_pVSConstantBuffer1);
	delete g_pDT;
}

void Camera_Update(double elapsed_time)
{
	XMVECTOR front = XMLoadFloat3(&g_CameraVector_Front);
	XMVECTOR up = XMLoadFloat3(&g_CameraVector_Up);
	XMVECTOR right = XMLoadFloat3(&g_CameraVector_Right);
	XMVECTOR position = XMLoadFloat3(&g_CameraPos);
	const float dt = static_cast<float>(elapsed_time);

	// Mouse input
	Mouse_State ms{};
	Mouse_GetState(&ms);

	int mouseDX = ms.x - g_camPrevMouseX;
	int mouseDY = ms.y - g_camPrevMouseY;

	bool altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

	// Alt + Right Mouse Button = Orbit/Rotate Camera
	if (altHeld && ms.rightButton) {
		float yawAngle = mouseDX * 0.005f;
		float pitchAngle = mouseDY * 0.005f;

		// Yaw (rotate around world Y)
		XMMATRIX yawRot = XMMatrixRotationY(yawAngle);
		front = XMVector3Normalize(XMVector3TransformNormal(front, yawRot));
		right = XMVector3Normalize(XMVector3TransformNormal(right, yawRot));
		up = XMVector3Normalize(XMVector3TransformNormal(up, yawRot));

		// Pitch (rotate around local right axis)
		XMMATRIX pitchRot = XMMatrixRotationAxis(right, pitchAngle);
		front = XMVector3Normalize(XMVector3TransformNormal(front, pitchRot));
		up = XMVector3Normalize(XMVector3Cross(front, right));
	}

	// Alt + Left Mouse Button = Pan Camera
	if (altHeld && ms.leftButton) {
		float panSpeed = 0.02f;
		position += -right * (float)mouseDX * panSpeed;
		position += up * (float)mouseDY * panSpeed;
	}

	// Scroll Wheel = Zoom (Move along front axis)
	int scrollDelta = ms.scrollWheelValue - g_camPrevScrollVal;
	if (scrollDelta != 0) {
		float zoomSpeed = 0.005f;
		position += front * (float)scrollDelta * zoomSpeed;
	}

	// WASD = move camera on the horizontal plane using the current facing direction.
	{
		XMVECTOR moveFront = XMVectorSet(XMVectorGetX(front), 0.0f, XMVectorGetZ(front), 0.0f);
		if (XMVectorGetX(XMVector3LengthSq(moveFront)) > 1.0e-6f)
		{
			moveFront = XMVector3Normalize(moveFront);
		}
		else
		{
			moveFront = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		}

		XMVECTOR moveRight = XMVectorSet(XMVectorGetX(right), 0.0f, XMVectorGetZ(right), 0.0f);
		if (XMVectorGetX(XMVector3LengthSq(moveRight)) > 1.0e-6f)
		{
			moveRight = XMVector3Normalize(moveRight);
		}
		else
		{
			moveRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		}

		float moveScale = g_CameraMoveSpeed * dt;
		if (KeyLogger_IsPressed(KK_LEFTSHIFT) || KeyLogger_IsPressed(KK_RIGHTSHIFT))
		{
			moveScale *= 2.0f;
		}

		if (KeyLogger_IsPressed(KK_W))
		{
			position += moveFront * moveScale;
		}
		if (KeyLogger_IsPressed(KK_S))
		{
			position -= moveFront * moveScale;
		}
		if (KeyLogger_IsPressed(KK_D))
		{
			position += moveRight * moveScale;
		}
		if (KeyLogger_IsPressed(KK_A))
		{
			position -= moveRight * moveScale;
		}
	}

	// --- Keyboard fallback controls (kept for convenience) ---
	if (KeyLogger_IsPressed(KK_DOWN)) {
		XMMATRIX rotation = XMMatrixRotationAxis(right, g_CameraRotateSpeed * dt);
		front = XMVector3Normalize(XMVector3TransformNormal(front, rotation));
		up = XMVector3Normalize(XMVector3Cross(front, right));
	}
	if (KeyLogger_IsPressed(KK_UP)) {
		XMMATRIX rotation = XMMatrixRotationAxis(right, -g_CameraRotateSpeed * dt);
		front = XMVector3Normalize(XMVector3TransformNormal(front, rotation));
		up = XMVector3Normalize(XMVector3Cross(front, right));
	}
	if (KeyLogger_IsPressed(KK_RIGHT)) {
		XMMATRIX rotation = XMMatrixRotationY(g_CameraRotateSpeed * dt);
		up = XMVector3Normalize(XMVector3TransformNormal(up, rotation));
		right = XMVector3Normalize(XMVector3TransformNormal(right, rotation));
		front = XMVector3Normalize(XMVector3Cross(right, up));
	}
	if (KeyLogger_IsPressed(KK_LEFT)) {
		XMMATRIX rotation = XMMatrixRotationY(-g_CameraRotateSpeed * dt);
		up = XMVector3Normalize(XMVector3TransformNormal(up, rotation));
		right = XMVector3Normalize(XMVector3TransformNormal(right, rotation));
		front = XMVector3Normalize(XMVector3Cross(right, up));
	}

	// Save mouse state for next frame
	g_camPrevMouseX = ms.x;
	g_camPrevMouseY = ms.y;
	g_camPrevScrollVal = ms.scrollWheelValue;

	// 各種更新結果を保存
	XMStoreFloat3(&g_CameraPos, position);
	XMStoreFloat3(&g_CameraVector_Front, front);
	XMStoreFloat3(&g_CameraVector_Up, up);
	XMStoreFloat3(&g_CameraVector_Right, right);

	// ビュー座標変換行列の作成
	XMMATRIX mtxView = XMMatrixLookAtLH(
		position,
		position + front,
		up);
	
	XMStoreFloat4x4(&g_CameraMatrix, mtxView);

	float aspectRatio = (float)Direct3D_GetBackBufferWidth() / (float)Direct3D_GetBackBufferHeight();
	float nearZ = 0.1f;
	float farZ = 5000.0f;
	XMMATRIX mtxPerspective = XMMatrixPerspectiveFovLH(g_Fov, aspectRatio, nearZ, farZ);

	XMStoreFloat4x4(&g_PerspectiveMatrix, mtxPerspective);

}

const DirectX::XMFLOAT4X4& Camera_GetMatrix()
{
	return g_CameraMatrix;
}

const DirectX::XMFLOAT4X4& Camera_GetPerspectiveMatrix()
{
	return g_PerspectiveMatrix;
}

const DirectX::XMFLOAT3& Camera_GetPosition()
{
	return g_CameraPos;
}

const DirectX::XMFLOAT3& Camera_GetVector_Front()
{
	return g_CameraVector_Front;
}

float Camera_GetFov()
{
	return g_Fov;
}

void Camera_SetMatrix(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection)
{
	// 定数バッファビュー設定
	XMFLOAT4X4 v,p;
	XMStoreFloat4x4(&v, XMMatrixTranspose(view));
	XMStoreFloat4x4(&p, XMMatrixTranspose(projection));

	Direct3D_GetContext()->UpdateSubresource(g_pVSConstantBuffer1, 0, nullptr, &v, 0, 0);
	Direct3D_GetContext()->VSSetConstantBuffers(1, 1, &g_pVSConstantBuffer1);

	Direct3D_GetContext()->UpdateSubresource(g_pVSConstantBuffer2, 0, nullptr, &p, 0, 0);
	Direct3D_GetContext()->VSSetConstantBuffers(2, 1, &g_pVSConstantBuffer2);
}

void Camera_DebugDraw()
{
#if defined(DEBUG) || defined(_DEBUG)
	std::stringstream ss;
	ss<<std::endl;
	ss << "Camera Position	   	: x =" << g_CameraPos.x;
	ss << " y = " << g_CameraPos.y;
	ss << " z = " << g_CameraPos.z << std::endl;

	ss << "CameraVector Front	: x =" << g_CameraVector_Front.x;
	ss << " y = " << g_CameraVector_Front.y;
	ss << " z = " << g_CameraVector_Front.z << std::endl;

	ss << "CameraVector Right	: x =" << g_CameraVector_Right.x;
	ss << " y = " << g_CameraVector_Right.y;
	ss << " z = " << g_CameraVector_Right.z << std::endl;

	ss << "CameraVector Up	   	: x =" << g_CameraVector_Up.x;
	ss << " y = " << g_CameraVector_Up.y;
	ss << " z = " << g_CameraVector_Up.z << std::endl;

	ss << "CameraVector Fov	   	: x =" << g_Fov << std::endl;

	g_pDT->SetText(ss.str().c_str(), { 0.0f,1.0f,0.0f,1.0f });
	g_pDT->Draw();
	g_pDT->Clear();
#endif
}
