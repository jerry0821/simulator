// --------------------------------------------
// スプライト表示 [sprite.h]
// ============================================
// Created by: Yasuda Atsushi
// Date: 2025-06-18
// Version: 1.0
//---------------------------------------------
#ifndef SPRITE_H
#define SPRITE_H

#include <d3d11.h>
#include <DirectXMath.h>

void Sprite_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Sprite_Finalize(void);

void Sprite_Begin();

// テクスチャ全表示
void Sprite_Draw(
	int texid,
	float dx,
	float dy,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f }
);

// テクスチャ全表示（表示サイズ変更可能）
void Sprite_Draw(
	int texid,
	float dx,
	float dy,
	float dw,
	float dh,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f }
);

// UVカット
void Sprite_Draw(
	int texid,
	float dx,
	float dy,
	int px,
	int py,
	int pw,
	int ph,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f }
);

// UVカット（表示サイズ変更可能）
void Sprite_Draw(
	int texid,
	float dx,
	float dy,
	float dw,
	float dh,
	int px,
	int py,
	int pw,
	int ph,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f }
);

// UVカット（表示サイズ変更可能）回転
void Sprite_Draw(
	int texid,
	float dx,
	float dy,
	float dw,
	float dh,
	int px,
	int py,
	int pw,
	int ph,
	float angle,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f }
);

// テクスチャ全表示（テクスチャ指定なし、表示サイズ変更可能）
void Sprite_Draw(
	float dx,
	float dy,
	float dw,
	float dh,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f }
);


void Sprite_DrawSRV(ID3D11ShaderResourceView* pSRV, float x, float y, float w, float h);

#endif // SPRITE_H