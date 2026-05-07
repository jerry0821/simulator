// --------------------------------------------
// ビルボード [billboard.h]
// ============================================
// Created by: Jerry
// Date: 2025-11-14
//---------------------------------------------
#ifndef BILLBOARD_H
#define BILLBOARD_H

#include <DirectXMath.h>
#include "collision.h"

void Billboard_Initialize();
void Billboard_Finalize(void);

void BillBoard_SetViewMatrix(const DirectX::XMFLOAT4X4& view);

void Billboard_Draw(int texId, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT2& scale, 
	const DirectX::XMFLOAT2& pivot = { 0.0f,0.0f });
void Billboard_Draw(int texId, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT2& scale,
	const DirectX::XMFLOAT4& text_cut, const DirectX::XMFLOAT2& pivot = { 0.0f,0.0f });
void Billboard_Draw(int texId, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT2& scale,
	const DirectX::XMFLOAT4& uv_rect, const DirectX::XMFLOAT4& color);

void Billboard_DrawAnim(int texId, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT2& scale,
	int currentFrame, int splitX, int splitY, const DirectX::XMFLOAT4& color = { 1,1,1,1 });


#endif // BILLBOARD_H
