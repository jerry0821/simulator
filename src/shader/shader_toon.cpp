// ----------------------------------------------------
// シェーダートゥーン  [shader_toon.cpp]
// ====================================================
// Created by: Jerry
// Date: 2025-12-13
// ----------------------------------------------------
#include "shader_toon.h"
using namespace DirectX;
#include "direct3d.h"
#include "debug_ostream.h"
#include <fstream>
#include "shader3d.h"
#include "sampler.h"
#include <vector>

static ID3D11VertexShader* g_pVS_Outline = nullptr;
static ID3D11VertexShader* g_pVS_Main = nullptr;
static ID3D11PixelShader* g_pPS_Outline = nullptr;
static ID3D11PixelShader* g_pPS_Main = nullptr;


static ID3D11InputLayout* g_pInputLayout = nullptr;

static ID3D11RasterizerState* g_pRS_CullFront = nullptr; // 輪郭用 (表面を消す＝裏面を描く)
static ID3D11RasterizerState* g_pRS_CullBack = nullptr;  // 本体用

static ID3D11Buffer* g_pVSConstantBuffer0 = nullptr; // b0: World + Outline
static ID3D11Buffer* g_pVSConstantBuffer1 = nullptr; // b1: View
static ID3D11Buffer* g_pVSConstantBuffer2 = nullptr;
//static ID3D11Buffer* g_pCB_VS = nullptr; // Vertex Shader用 (b0)
static ID3D11Buffer* g_pCB_PS = nullptr; // Pixel Shader用 (b0)

static float g_OutlineWidth = 0.02f;
int g_ToonStepCount = 4;

static XMFLOAT4 g_OutlineColor = {0.0f, 0.0f, 0.0f, 1.0f};

struct CB_VS_DATA {
	XMFLOAT4X4 mWorld;     // World (ライティング計算用)
	float      outlineWidth; // 輪郭線の太さ
	float      padding[3];   // アライメント調整 (16バイト境界)
};

struct CB_PS_DATA {
	XMFLOAT4   color;      // マテリアルカラー
	XMFLOAT4   outlineColor; // 輪郭線の色
	int   stepcount;   // ライト方向
	float      padding[3];
};

static CB_PS_DATA g_PSData = {
	{1,1,1,1}, // color
	{0,0,0,1}, // outlineColor (預設黑色)
	4,         // stepcount
	{0,0,0}    // padding
};

static bool LoadShaderFile(const char* filename, std::vector<char>& buffer)
{
	std::ifstream ifs(filename, std::ios::binary);
	if (!ifs) {
		// エラーメッセージは呼び出し元で出すか、ここで出す
		return false;
	}
	ifs.seekg(0, std::ios::end);
	size_t size = (size_t)ifs.tellg();
	ifs.seekg(0, std::ios::beg);
	buffer.resize(size);
	ifs.read(buffer.data(), size);
	return true;
}

bool ShaderToon_Initialize()
{
	HRESULT hr; // 戻り値格納用


	//Outline用 シェーダー作成
	// Vertex Shader
	std::vector<char> shaderBuffer;

	if (!LoadShaderFile("resource/shader/shader_vertex_toon_outline.cso", shaderBuffer)) {
		MessageBox(nullptr, "shader_vertex_toon_outline.cso load failed", "Error", MB_OK);
		return false;
	}
	hr = Direct3D_GetDevice()->CreateVertexShader(shaderBuffer.data(), shaderBuffer.size(), nullptr, &g_pVS_Outline);
	if (FAILED(hr)) return false;

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	hr = Direct3D_GetDevice()->CreateInputLayout(layout, ARRAYSIZE(layout), shaderBuffer.data(), shaderBuffer.size(), &g_pInputLayout);
	if (FAILED(hr)) return false;

	// Pixel Shader
	if (!LoadShaderFile("resource/shader/shader_pixel_toon_outline.cso", shaderBuffer)) return false;
	hr = Direct3D_GetDevice() ->CreatePixelShader(shaderBuffer.data(), shaderBuffer.size(), nullptr, &g_pPS_Outline);
	if (FAILED(hr)) return false;


	//Main(本体)用 シェーダー作成
	// Vertex Shader
	if (!LoadShaderFile("resource/shader/shader_vertex_toon.cso", shaderBuffer)) return false;
	hr = Direct3D_GetDevice()->CreateVertexShader(shaderBuffer.data(), shaderBuffer.size(), nullptr, &g_pVS_Main);
	if (FAILED(hr)) return false;

	// Pixel Shader
	if (!LoadShaderFile("resource/shader/shader_pixel_toon.cso", shaderBuffer)) return false;
	hr = Direct3D_GetDevice()->CreatePixelShader(shaderBuffer.data(), shaderBuffer.size(), nullptr, &g_pPS_Main);
	if (FAILED(hr)) return false;

	
	// 定数バッファ作成
	D3D11_BUFFER_DESC cbDesc{};
	cbDesc.ByteWidth = sizeof(CB_VS_DATA);
	cbDesc.Usage = D3D11_USAGE_DEFAULT;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = Direct3D_GetDevice()->CreateBuffer(&cbDesc, nullptr, &g_pVSConstantBuffer0);
	if (FAILED(hr)) return false;

	// 2. b1: View
	cbDesc.ByteWidth = sizeof(XMFLOAT4X4);
	hr = Direct3D_GetDevice()->CreateBuffer(&cbDesc, nullptr, &g_pVSConstantBuffer1);
	if (FAILED(hr)) return false;

	// 3. b2: Projection
	cbDesc.ByteWidth = sizeof(XMFLOAT4X4);
	hr = Direct3D_GetDevice()->CreateBuffer(&cbDesc, nullptr, &g_pVSConstantBuffer2);
	if (FAILED(hr)) return false;

	cbDesc.ByteWidth = sizeof(CB_PS_DATA);
	hr = Direct3D_GetDevice()->CreateBuffer(&cbDesc, nullptr, &g_pCB_PS);
	if (FAILED(hr)) return false;




	// ラスタライザーステート作成 
	D3D11_RASTERIZER_DESC rsDesc{};
	rsDesc.FillMode = D3D11_FILL_SOLID;
	rsDesc.DepthClipEnable = TRUE;
	rsDesc.MultisampleEnable = FALSE;

	// Cull Front (輪郭用：表面をカリング＝裏面だけ描画)
	rsDesc.CullMode = D3D11_CULL_FRONT;
	hr = Direct3D_GetDevice()->CreateRasterizerState(&rsDesc, &g_pRS_CullFront);
	if (FAILED(hr)) return false;

	// Cull Back (本体用：裏面をカリング＝表面だけ描画 / デフォルト)
	rsDesc.CullMode = D3D11_CULL_BACK;
	hr = Direct3D_GetDevice()->CreateRasterizerState(&rsDesc, &g_pRS_CullBack);
	if (FAILED(hr)) return false;

	return true;
}

void ShaderToon_Finalize()
{
	SAFE_RELEASE(g_pRS_CullBack);
	SAFE_RELEASE(g_pRS_CullFront);
	SAFE_RELEASE(g_pCB_PS);
	SAFE_RELEASE(g_pVSConstantBuffer2);
	SAFE_RELEASE(g_pVSConstantBuffer1);
	SAFE_RELEASE(g_pVSConstantBuffer0);
	SAFE_RELEASE(g_pInputLayout);
	SAFE_RELEASE(g_pPS_Main);
	SAFE_RELEASE(g_pVS_Main);
	SAFE_RELEASE(g_pPS_Outline);
	SAFE_RELEASE(g_pVS_Outline);
}

//void ShaderToon_SetTransforms(const DirectX::XMMATRIX& world, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection)
//{
//	CB_VS_DATA data = {}; 
//
//	XMMATRIX wvp = world * view * projection;
//
//	XMStoreFloat4x4(&data.mWVP, XMMatrixTranspose(wvp)); 
//	XMStoreFloat4x4(&data.mWorld, XMMatrixTranspose(world));  
//
//	data.outlineWidth = g_OutlineWidth;
//
//	Direct3D_GetContext()->UpdateSubresource(g_pCB_VS, 0, nullptr, &data, 0, 0);
//}

void ShaderToon_SetWorldMatrix(const DirectX::XMMATRIX& world)
{
	CB_VS_DATA data = {}; 

	XMStoreFloat4x4(&data.mWorld, XMMatrixTranspose(world));
	data.outlineWidth = g_OutlineWidth;

	// Update b0
	Direct3D_GetContext()->UpdateSubresource(g_pVSConstantBuffer0, 0, nullptr, &data, 0, 0);
}

void ShaderToon_SetViewMatrix(const DirectX::XMMATRIX& view)
{
	XMFLOAT4X4 transpose;

	XMStoreFloat4x4(&transpose, XMMatrixTranspose(view));
	Direct3D_GetContext()->UpdateSubresource(g_pVSConstantBuffer1, 0, nullptr, &transpose, 0, 0);
}

void ShaderToon_SetProjMatrix(const DirectX::XMMATRIX& proj)
{
	XMFLOAT4X4 transpose;

	XMStoreFloat4x4(&transpose, XMMatrixTranspose(proj));
	Direct3D_GetContext()->UpdateSubresource(g_pVSConstantBuffer2, 0, nullptr, &transpose, 0, 0);
}



void ShaderToon_SetMaterialColor(const DirectX::XMFLOAT4& color, int stepcount)
{
	g_PSData.color = color;
	g_PSData.stepcount = stepcount;
	Direct3D_GetContext()->UpdateSubresource(g_pCB_PS, 0, nullptr, &g_PSData, 0, 0);
}

void ShaderToon_SetOutlineColor(const DirectX::XMFLOAT4& color) 
{
	g_PSData.outlineColor = color;

	Direct3D_GetContext()->UpdateSubresource(g_pCB_PS, 0, nullptr, &g_PSData, 0, 0);
}

void ShaderToon_SetOutlineWidth(float width)
{
	g_OutlineWidth = width;
}


void ShaderToon_Begin_Outline()
{
	// 1. シェーダー設定 
	Direct3D_GetContext()->VSSetShader(g_pVS_Outline, nullptr, 0);
	Direct3D_GetContext()->PSSetShader(g_pPS_Outline, nullptr, 0);

	// 2. レイアウト設定
	Direct3D_GetContext()->IASetInputLayout(g_pInputLayout);

	// 3. 定数バッファ設定
	ID3D11Buffer* pBuffers[3] = {
		g_pVSConstantBuffer0, // b0
		g_pVSConstantBuffer1, // b1
		g_pVSConstantBuffer2  // b2
	};
	Direct3D_GetContext()->VSSetConstantBuffers(0, 3, pBuffers); // b0
	Direct3D_GetContext()->PSSetConstantBuffers(0, 1, &g_pCB_PS); // b0

	// 4. ラスタライザ設定 (Front Face Culling -> 裏面を描く)
	Direct3D_GetContext()->RSSetState(g_pRS_CullFront);
}

void ShaderToon_Begin_Main()
{
	// 1. シェーダー設定 
	Direct3D_GetContext()->VSSetShader(g_pVS_Main, nullptr, 0);
	Direct3D_GetContext()->PSSetShader(g_pPS_Main, nullptr, 0);

	// 2. レイアウト設定 (同じなので省略可だが安全のため)
	Direct3D_GetContext()->IASetInputLayout(g_pInputLayout);

	// 3. 定数バッファ設定
	ID3D11Buffer* pBuffers[3] = {
		g_pVSConstantBuffer0, // b0
		g_pVSConstantBuffer1, // b1
		g_pVSConstantBuffer2  // b2
	};
	Direct3D_GetContext()->VSSetConstantBuffers(0, 3, pBuffers);
	Direct3D_GetContext()->PSSetConstantBuffers(0, 1, &g_pCB_PS);

	// 4. ラスタライザ設定 (Back Face Culling -> 表面を描く)
	Direct3D_GetContext()->RSSetState(g_pRS_CullBack);
}
