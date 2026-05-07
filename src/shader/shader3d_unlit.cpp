// ----------------------------------------------------
// シェーダー3D(ライトなし)  [shader3d_unlit.cpp]
// ====================================================
// Created by: Jerry
// Date: 2025-11-21
// ----------------------------------------------------

#include "shader3d_unlit.h"
#include <d3d11.h>
#include "debug_ostream.h"
#include <DirectXMath.h>
using namespace DirectX;
#include "direct3d.h"
#include <fstream>
#include "sampler.h"


static ID3D11VertexShader* g_pVertexShader = nullptr;
static ID3D11InputLayout* g_pInputLayout = nullptr;
static ID3D11Buffer* g_pVSConstantBuffer0 = nullptr; // world
static ID3D11Buffer* g_pVSConstantBuffer1 = nullptr; // view
static ID3D11Buffer* g_pVSConstantBuffer2 = nullptr; // proj
static ID3D11Buffer* g_pPSConstantBuffer0 = nullptr; // Material
static ID3D11PixelShader* g_pPixelShader = nullptr;


bool Shader3D_Unlit_Initialize()
{
	HRESULT hr; // 戻り値格納用


	// 事前コンパイル済み頂点シェーダーの読み込み
	std::ifstream ifs_vs("resource/shader/shader_vertex_3d_unlit.cso", std::ios::binary);

	if (!ifs_vs) {
		MessageBox(nullptr, "頂点シェーダーの読み込みに失敗しました\n\nshader_vertex_3d_unlit.cso", "エラー", MB_OK);
		return false;
	}

	// ファイルサイズを取得
	ifs_vs.seekg(0, std::ios::end); // ファイルポインタを末尾に移動
	std::streamsize filesize = ifs_vs.tellg(); // ファイルポインタの位置を取得（つまりファイルサイズ）
	ifs_vs.seekg(0, std::ios::beg); // ファイルポインタを先頭に戻す

	// バイナリデータを格納するためのバッファを確保
	unsigned char* vsbinary_pointer = new unsigned char[filesize];

	ifs_vs.read((char*)vsbinary_pointer, filesize); // バイナリデータを読み込む
	ifs_vs.close(); // ファイルを閉じる

	// 頂点シェーダーの作成
	hr = Direct3D_GetDevice()->CreateVertexShader(vsbinary_pointer, filesize, nullptr, &g_pVertexShader);

	if (FAILED(hr)) {
		hal::dout << "Shader3D_Unlit_Initialize() : 頂点シェーダーの作成に失敗しました" << std::endl;
		delete[] vsbinary_pointer; // メモリリークしないようにバイナリデータのバッファを解放
		return false;
	}


	// 頂点レイアウトの定義
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,		0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",	  0, DXGI_FORMAT_R32G32B32_FLOAT,		0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",	  0, DXGI_FORMAT_R32G32B32A32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,			0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	UINT num_elements = ARRAYSIZE(layout); // 配列の要素数を取得

	// 頂点レイアウトの作成
	hr = Direct3D_GetDevice()->CreateInputLayout(layout, num_elements, vsbinary_pointer, filesize, &g_pInputLayout);

	delete[] vsbinary_pointer; // バイナリデータのバッファを解放

	if (FAILED(hr)) {
		hal::dout << "Shader3D_Unlit_Initialize() : 頂点レイアウトの作成に失敗しました" << std::endl;
		return false;
	}


	// 頂点シェーダー用定数バッファの作成
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(XMFLOAT4X4); // バッファのサイズ
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; // バインドフラグ

	Direct3D_GetDevice()->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer0);
	Direct3D_GetDevice()->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer1);
	Direct3D_GetDevice()->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer2);


	// 事前コンパイル済みピクセルシェーダーの読み込み
	std::ifstream ifs_ps("resource/shader/shader_pixel_3d_unlit.cso", std::ios::binary);
	if (!ifs_ps) {
		MessageBox(nullptr, "ピクセルシェーダーの読み込みに失敗しました\n\nshader_pixel_3d.cso", "エラー", MB_OK);
		return false;
	}

	ifs_ps.seekg(0, std::ios::end);
	filesize = ifs_ps.tellg();
	ifs_ps.seekg(0, std::ios::beg);

	unsigned char* psbinary_pointer = new unsigned char[filesize];
	ifs_ps.read((char*)psbinary_pointer, filesize);
	ifs_ps.close();

	// ピクセルシェーダーの作成
	hr = Direct3D_GetDevice()->CreatePixelShader(psbinary_pointer, filesize, nullptr, &g_pPixelShader);

	delete[] psbinary_pointer; // バイナリデータのバッファを解放

	if (FAILED(hr)) {
		hal::dout << "Shader3D_Unlit_Initialize() : ピクセルシェーダーの作成に失敗しました" << std::endl;
		return false;
	}

	// ピクセルシェーダー用定数バッファの作成
	buffer_desc.ByteWidth = sizeof(XMFLOAT4); // バッファのサイズ

	Direct3D_GetDevice()->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer0);

	return true;
}

void Shader3D_Unlit_Finalize()
{
	SAFE_RELEASE(g_pPixelShader);
	SAFE_RELEASE(g_pPSConstantBuffer0);
	SAFE_RELEASE(g_pVSConstantBuffer2);
	SAFE_RELEASE(g_pVSConstantBuffer1);
	SAFE_RELEASE(g_pVSConstantBuffer0);
	SAFE_RELEASE(g_pInputLayout);
	SAFE_RELEASE(g_pVertexShader);
}

void Shader3D_Unlit_SetWorldMatrix(const DirectX::XMMATRIX& matrix)
{
	// 定数バッファ格納用行列の構造体を定義
	XMFLOAT4X4 transpose;

	// 行列を転置して定数バッファ格納用行列に変換
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));

	// 定数バッファに行列をセット
	Direct3D_GetContext()->UpdateSubresource(g_pVSConstantBuffer0, 0, nullptr, &transpose, 0, 0);
}

void Shader3D_Unlit_SetViewMatrix(const DirectX::XMMATRIX& matrix)
{
	XMFLOAT4X4 transpose;
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));
	Direct3D_GetContext()->UpdateSubresource(g_pVSConstantBuffer1, 0, nullptr, &transpose, 0, 0);
}

void Shader3D_Unlit_SetProjMatrix(const DirectX::XMMATRIX& matrix)
{
	XMFLOAT4X4 transpose;
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));
	Direct3D_GetContext()->UpdateSubresource(g_pVSConstantBuffer2, 0, nullptr, &transpose, 0, 0);
}

void Shader3D_Unlit_SetMaterialColor(const DirectX::XMFLOAT4& material_color)
{
	//定数バッファに行列をセット
	Direct3D_GetContext()->UpdateSubresource(g_pPSConstantBuffer0, 0, nullptr, &material_color, 0, 0);
}

void Shader3D_Unlit_Begin()
{	
	// 頂点シェーダーとピクセルシェーダーを描画パイプラインに設定
	Direct3D_GetContext()->VSSetShader(g_pVertexShader, nullptr, 0);
	Direct3D_GetContext()->PSSetShader(g_pPixelShader, nullptr, 0);

	// 頂点レイアウトを描画パイプラインに設定
	Direct3D_GetContext()->IASetInputLayout(g_pInputLayout);

	// 定数バッファを描画パイプラインに設定
	ID3D11Buffer* pBuffers[3] = {
		g_pVSConstantBuffer0,
		g_pVSConstantBuffer1,
		g_pVSConstantBuffer2
	};
	Direct3D_GetContext()->VSSetConstantBuffers(0, 3, pBuffers);
	Direct3D_GetContext()->PSSetConstantBuffers(0, 1, &g_pPSConstantBuffer0);

	// Unlit textured objects still need a sampler.
	Backend::DX11::Sampler::SetAnisotropicFilter();
}

