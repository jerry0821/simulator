// --------------------------------------------
// スプライト表示 [sprite.cpp]
// ============================================
// Created by: Yasuda Atsushi
// Date: 2025-06-18
// Version: 1.0
//---------------------------------------------
#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;
#include "direct3d.h"
#include "shader.h"
#include "debug_ostream.h"
#include "sprite.h"
#include "texture.h"
#include "shader3d.h"
#include "shader_billboard.h"

static constexpr int NUM_VERTEX = 4; // 頂点数

static ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ
static ID3D11ShaderResourceView* g_pTexture = nullptr;	// テクスチャバッファ

// 注意！初期化で外部から設定されるもの。Release不要。
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

// 頂点構造体
struct Vertex
{
	XMFLOAT3 position;	// 頂点座標
	XMFLOAT4 color;		// 色
	XMFLOAT2 uv;	// テクスチャー
};

void Sprite_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	// デバイスとデバイスコンテキストのチェック
	if (!pDevice || !pContext) {
		hal::dout << "Sprite_Initialize() : 与えられたデバイスかコンテキストが不正です" << std::endl;
		return;
	}

	// デバイスとデバイスコンテキストの保存
	g_pDevice = pDevice;
	g_pContext = pContext;

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(Vertex) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	g_pDevice->CreateBuffer(&bd, NULL, &g_pVertexBuffer);

	
}

void Sprite_Finalize(void)
{
	SAFE_RELEASE(g_pTexture);
	SAFE_RELEASE(g_pVertexBuffer);
}

void Sprite_Begin()
{
	// 頂点シェーダーに変換行列を設定
	const float SCREEN_WIDTH = (float)Direct3D_GetBackBufferWidth();
	const float SCREEN_HEIGHT = (float)Direct3D_GetBackBufferHeight();

	Shader_SetProjectionMatrix(XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f));
}

void Sprite_Draw(int texid, float dx, float dy, const XMFLOAT4& color)
{
	// シェーダーを描画パイプラインに設定
	Shader_Begin();

	// 頂点バッファをロックする
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得
	Vertex* v = (Vertex*)msr.pData;

	// 頂点情報を書き込み
	unsigned int dw = Texture_Width(texid);
	unsigned int dh = Texture_Height(texid);

	// 画面の左上から右下に向かう線分を描画する -> 時計回り
	v[0].position = { dx,		dy,		 0.0f };	// LT
	v[1].position = { dx + dw,  dy,		 0.0f };	// RT
	v[2].position = { dx,		dy + dh, 0.0f };	// LB
	v[3].position = { dx + dw,  dy + dh, 0.0f };	// RB

	v[0].color = color;
	v[1].color = color;
	v[2].color = color;
	v[3].color = color;

	v[0].uv = { 0.0f, 0.0f };
	v[1].uv = { 1.0f, 0.0f };
	v[2].uv = { 0.0f, 1.0f };
	v[3].uv = { 1.0f, 1.0f };

	// 頂点バッファのロックを解除
	g_pContext->Unmap(g_pVertexBuffer, 0);

	// ワールド変換行列を設定
	Shader_SetWorldMatrix(XMMatrixIdentity());

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	

	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// テクスチャの設定
	Texture_SetTexture(texid);

	// ポリゴン描画命令発行
	g_pContext->Draw(NUM_VERTEX, 0);
}

void Sprite_Draw(int texid, float dx, float dy, float dw, float dh, const XMFLOAT4& color)
{
	// シェーダーを描画パイプラインに設定
	Shader_Begin();

	// 頂点バッファをロックする
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得
	Vertex* v = (Vertex*)msr.pData;

	// 画面の左上から右下に向かう線分を描画する -> 時計回り
	v[0].position = { dx,		dy,		 0.0f };	// LT
	v[1].position = { dx + dw,  dy,		 0.0f };	// RT
	v[2].position = { dx,		dy + dh, 0.0f };	// LB
	v[3].position = { dx + dw,  dy + dh, 0.0f };	// RB

	v[0].color = color;
	v[1].color = color;
	v[2].color = color;
	v[3].color = color;

	v[0].uv = { 0.0f, 0.0f };
	v[1].uv = { 1.0f, 0.0f };
	v[2].uv = { 0.0f, 1.0f };
	v[3].uv = { 1.0f, 1.0f };

	// 頂点バッファのロックを解除
	g_pContext->Unmap(g_pVertexBuffer, 0);

	// ワールド変換行列を設定
	Shader_SetWorldMatrix(XMMatrixIdentity());

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);



	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// テクスチャの設定
	Texture_SetTexture(texid);

	// ポリゴン描画命令発行
	g_pContext->Draw(NUM_VERTEX, 0);
}

void Sprite_Draw(int texid, float dx, float dy, int px, int py, int pw, int ph, const XMFLOAT4& color)
{
	// シェーダーを描画パイプラインに設定
	Shader_Begin();

	// 頂点バッファをロックする
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得
	Vertex* v = (Vertex*)msr.pData;

	// 頂点情報を書き込み

	// 画面の左上から右下に向かう線分を描画する -> 時計回り
	v[0].position = { dx,		dy,		 0.0f };	// LT
	v[1].position = { dx + pw,  dy,		 0.0f };	// RT
	v[2].position = { dx,		dy + ph, 0.0f };	// LB
	v[3].position = { dx + pw,  dy + ph, 0.0f };	// RB

	v[0].color = color;
	v[1].color = color;
	v[2].color = color;
	v[3].color = color;

	float tw = (float)Texture_Width(texid);
	float th = (float)Texture_Height(texid);

	float u0 = px / tw;
	float v0 = py / th;
	float u1 = (px + pw) / tw;
	float v1 = (py + ph) / th;

	v[0].uv = { u0, v0 };
	v[1].uv = { u1, v0 };
	v[2].uv = { u0, v1 };
	v[3].uv = { u1, v1 };

	// 頂点バッファのロックを解除
	g_pContext->Unmap(g_pVertexBuffer, 0);

	// ワールド変換行列を設定
	Shader_SetWorldMatrix(XMMatrixIdentity());

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);



	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// テクスチャの設定
	Texture_SetTexture(texid);

	// ポリゴン描画命令発行
	g_pContext->Draw(NUM_VERTEX, 0);
}

void Sprite_Draw(int texid, float dx, float dy, float dw, float dh, int px, int py, int pw, int ph, const XMFLOAT4& color)
{
	// シェーダーを描画パイプラインに設定
	Shader_Begin();

	// 頂点バッファをロックする
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得
	Vertex* v = (Vertex*)msr.pData;

	// 頂点情報を書き込み

	// 画面の左上から右下に向かう線分を描画する -> 時計回り
	v[0].position = { dx,		dy,		 0.0f };	// LT
	v[1].position = { dx + dw,  dy,		 0.0f };	// RT
	v[2].position = { dx,		dy + dh, 0.0f };	// LB
	v[3].position = { dx + dw,  dy + dh, 0.0f };	// RB

	v[0].color = color;
	v[1].color = color;
	v[2].color = color;
	v[3].color = color;

	float tw = (float)Texture_Width(texid);
	float th = (float)Texture_Height(texid);

	float u0 = px / tw;
	float v0 = py / th;
	float u1 = (px + pw) / tw;
	float v1 = (py + ph) / th;

	v[0].uv = { u0, v0 };
	v[1].uv = { u1, v0 };
	v[2].uv = { u0, v1 };
	v[3].uv = { u1, v1 };

	// 頂点バッファのロックを解除
	g_pContext->Unmap(g_pVertexBuffer, 0);

	// ワールド変換行列を設定
	Shader_SetWorldMatrix(XMMatrixIdentity());

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);



	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// テクスチャの設定
	Texture_SetTexture(texid);

	// ポリゴン描画命令発行
	g_pContext->Draw(NUM_VERTEX, 0);
}

void Sprite_Draw(int texid, float dx, float dy, float dw, float dh, int px, int py, int pw, int ph, float angle, const XMFLOAT4& color)
{
	// シェーダーを描画パイプラインに設定
	Shader_Begin();

	// 頂点バッファをロックする
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得
	Vertex* v = (Vertex*)msr.pData;

	// 頂点情報を書き込み

	// 画面の左上から右下に向かう線分を描画する -> 時計回り
	v[0].position = { -0.5f, -0.5f, 0.0f };	// LT
	v[1].position = { +0.5f, -0.5f, 0.0f };	// RT
	v[2].position = { -0.5f, +0.5f, 0.0f };	// LB
	v[3].position = { +0.5f, +0.5f, 0.0f };	// RB

	v[0].color = color;
	v[1].color = color;
	v[2].color = color;
	v[3].color = color;

	float tw = (float)Texture_Width(texid);
	float th = (float)Texture_Height(texid);

	float u0 = px / tw;
	float v0 = py / th;
	float u1 = (px + pw) / tw;
	float v1 = (py + ph) / th;

	v[0].uv = { u0, v0 };
	v[1].uv = { u1, v0 };
	v[2].uv = { u0, v1 };
	v[3].uv = { u1, v1 };

	// 頂点バッファのロックを解除
	g_pContext->Unmap(g_pVertexBuffer, 0);


	// 回転行列をシェーダーに設定
	XMMATRIX scale		 = XMMatrixScaling(dw, dh, 1.0f); // 先にscale
	XMMATRIX rotation	 = XMMatrixRotationZ(angle);
	// XMMATRIX translation = XMMatrixTranslation(dx + dw * 0.5, dy + dh * 0.5, 0.0f);
	XMMATRIX translation = XMMatrixTranslation(dx, dy, 0.0f);

	Shader_SetWorldMatrix(scale * rotation * translation);


	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);



	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// テクスチャの設定
	Texture_SetTexture(texid);

	// ポリゴン描画命令発行
	g_pContext->Draw(NUM_VERTEX, 0);
}

void Sprite_Draw(float dx, float dy, float dw, float dh, const DirectX::XMFLOAT4& color)
{
	// シェーダーを描画パイプラインに設定
	Shader_Begin();

	// 頂点バッファをロックする
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得
	Vertex* v = (Vertex*)msr.pData;

	// 画面の左上から右下に向かう線分を描画する -> 時計回り
	v[0].position = { dx,		dy,		 0.0f };	// LT
	v[1].position = { dx + dw,  dy,		 0.0f };	// RT
	v[2].position = { dx,		dy + dh, 0.0f };	// LB
	v[3].position = { dx + dw,  dy + dh, 0.0f };	// RB

	v[0].color = color;
	v[1].color = color;
	v[2].color = color;
	v[3].color = color;

	v[0].uv = { 0.0f, 0.0f };
	v[1].uv = { 1.0f, 0.0f };
	v[2].uv = { 0.0f, 1.0f };
	v[3].uv = { 1.0f, 1.0f };

	// 頂点バッファのロックを解除
	g_pContext->Unmap(g_pVertexBuffer, 0);

	// ワールド変換行列を設定
	Shader_SetWorldMatrix(XMMatrixIdentity());

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);



	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// テクスチャの設定
	//Texture_SetTexture(texid);

	// ポリゴン描画命令発行
	g_pContext->Draw(NUM_VERTEX, 0);
}

void Sprite_DrawSRV(ID3D11ShaderResourceView* pSRV, float dx, float dy, float dw, float dh)
{
	// シェーダーを描画パイプラインに設定
	Shader_Begin();

	// 頂点バッファをロックする
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得
	Vertex* v = (Vertex*)msr.pData;

	// 画面の左上から右下に向かう線分を描画する -> 時計回り
	v[0].position = { dx,		dy,		 0.0f };	// LT
	v[1].position = { dx + dw,  dy,		 0.0f };	// RT
	v[2].position = { dx,		dy + dh, 0.0f };	// LB
	v[3].position = { dx + dw,  dy + dh, 0.0f };	// RB

	v[0].uv = { 0.0f, 0.0f };
	v[1].uv = { 1.0f, 0.0f };
	v[2].uv = { 0.0f, 1.0f };
	v[3].uv = { 1.0f, 1.0f };

	v[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	v[1].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	v[2].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	v[3].color = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 頂点バッファのロックを解除
	g_pContext->Unmap(g_pVertexBuffer, 0);

	// ワールド変換行列を設定
	Shader_SetWorldMatrix(XMMatrixIdentity());



	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);



	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// srvの設定
	if (pSRV)
	{
		Direct3D_GetContext()->PSSetShaderResources(0, 1, &pSRV);
	}

	// ポリゴン描画命令発行
	g_pContext->Draw(NUM_VERTEX, 0);

}
