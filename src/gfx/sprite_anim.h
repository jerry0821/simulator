//  --------------------------------------------
// スプライトアニメーション描画 [spriteanim.h]
// ============================================
// Created by: Yasuda Atsushi
// Date: 2025-06-17
// Version: 1.0
// --------------------------------------------
#ifndef SPRITE_ANIM_H
#define SPRITE_ANIM_H

#include <DirectXMath.h>

void SpriteAnim_Initialize();
void SpriteAnim_Finalize();


void SpriteAnim_Update(double elapsed_time);
void SpriteAnim_Draw(int playid, float dx, float dy, float dw, float dh);
void BillBoardAnim_Draw(int playid, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT2& scale, const DirectX::XMFLOAT2& pivot);

int SpriteAnim_RegisterPattern(
	int texId,
	int pattern_Max,
	int h_pattern_Max,
	double seconds_per_pattern,
	const DirectX::XMUINT2& pattern_size,
	const DirectX::XMUINT2& start_position,
	bool is_looped = true);

int SpriteAnim_CreatePlayer(int anim_pattern_id);

bool SpriteAnim_IsStopped(int index);

void SpriteAnim_DestroyPlayer(int index);

#endif // SPRITE_ANIM_H