// --------------------------------------------
// スプライトアニメーション描画 [spriteanim.h]
// ============================================
// Created by: Yasuda Atsushi
// Date: 2025-06-17
// Version: 1.0
// --------------------------------------------

#include "sprite_anim.h"
#include "sprite.h"
#include "texture.h"
#include <DirectXMath.h>
using namespace DirectX; // ヘッダーには書くなよ cppならいい
#include "billboard.h"


struct AnimPatternData
{
	int m_TextureId = -1; // テクスチャID
	int m_PatternMax = 0; // パターン数
	int m_HPatternMax = 0; // 横のパターン数(Holizone)
	XMUINT2 m_StartPosition {0, 0}; // アニメーションのスタート座標
	XMUINT2 m_PatternSize { 0, 0 }; // 1パターンのサイズ
	double m_seconds_per_pattern = 0.1;
	bool m_IsLooped = false; // ループするか
};

struct AnimPlayData
{
	int m_PatternId = -1; // アニメーションパターンID
	int m_PatternNum = 0; // 現在再生中のパターン
	double m_AccumulatedTime = 0.0; // 累積時間
	bool m_IsStopped = false; // 再生中かどうか
};

static constexpr int ANIM_PATTERN_MAX = 128;
static AnimPatternData g_AnimPattern[ANIM_PATTERN_MAX];
static constexpr int ANIM_PLAY_MAX = 256;
static AnimPlayData g_AnimPlay[ANIM_PLAY_MAX];



void SpriteAnim_Initialize()
{
	//アニメーションパターン管理情報を初期化（すべて利用していない）状況にする
	for (AnimPatternData& data : g_AnimPattern)
	{
		data.m_TextureId = -1;
	}

	for (AnimPlayData& data : g_AnimPlay)
	{
		data.m_PatternId = -1;
		data.m_IsStopped = false;
	}
}

void SpriteAnim_Finalize()
{
}


void SpriteAnim_Update(double elapsed_time)
{
	for (int i = 0; i < ANIM_PLAY_MAX; i++)
	{
		if (g_AnimPlay[i].m_PatternId < 0) continue; // データが入ってない

		AnimPatternData* pAnimPatternData = &g_AnimPattern[g_AnimPlay[i].m_PatternId];

		if (g_AnimPlay[i].m_AccumulatedTime >= pAnimPatternData ->m_seconds_per_pattern){
			g_AnimPlay[i].m_PatternNum++;

			if (g_AnimPlay[i].m_PatternNum >= pAnimPatternData->m_PatternMax)
			{
				if (pAnimPatternData->m_IsLooped)
				{
					g_AnimPlay[i].m_PatternNum = 0;
				}
				else
				{
					g_AnimPlay[i].m_PatternNum = pAnimPatternData->m_PatternMax - 1;
					g_AnimPlay[i].m_IsStopped = true; // 再生が止まった
				}
			}

			g_AnimPlay[i].m_AccumulatedTime -= pAnimPatternData->m_seconds_per_pattern;
		}
		g_AnimPlay[i].m_AccumulatedTime += elapsed_time;
	}
}

void SpriteAnim_Draw(int playid, float dx, float dy, float dw, float dh)
{
	int anim_pattern_id = g_AnimPlay[playid].m_PatternId;
	AnimPatternData* pAnimPatternData = &g_AnimPattern[anim_pattern_id];

	Sprite_Draw(pAnimPatternData->m_TextureId,
		dx, dy, dw, dh,
		pAnimPatternData->m_StartPosition.x
		+ pAnimPatternData->m_PatternSize.x
		* (g_AnimPlay[playid].m_PatternNum % pAnimPatternData->m_HPatternMax),
		pAnimPatternData->m_StartPosition.y
		+ pAnimPatternData->m_PatternSize.y
		* (g_AnimPlay[playid].m_PatternNum / pAnimPatternData->m_HPatternMax),
		pAnimPatternData->m_PatternSize.x,
		pAnimPatternData->m_PatternSize.y
	);
}

void BillBoardAnim_Draw(int playid, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT2& scale, const DirectX::XMFLOAT2& pivot)
{
	int anim_pattern_id = g_AnimPlay[playid].m_PatternId;
	AnimPatternData* pAnimPatternData = &g_AnimPattern[anim_pattern_id];

	Billboard_Draw(pAnimPatternData->m_TextureId,
		position,scale,
		{
			pAnimPatternData->m_StartPosition.x
			+ pAnimPatternData->m_PatternSize.x
			* (float)(g_AnimPlay[playid].m_PatternNum % pAnimPatternData->m_HPatternMax),
			pAnimPatternData->m_StartPosition.y
			+ pAnimPatternData->m_PatternSize.y
			* (float)(g_AnimPlay[playid].m_PatternNum / pAnimPatternData->m_HPatternMax),
			(float)pAnimPatternData->m_PatternSize.x,
			(float)pAnimPatternData->m_PatternSize.y
		},
		pivot
	);
}

int SpriteAnim_RegisterPattern(int texId, int pattern_Max, int h_pattern_Max, double seconds_per_pattern, const DirectX::XMUINT2& pattern_size, const DirectX::XMUINT2& start_position, bool is_looped)
{
	for (int i = 0; i < ANIM_PATTERN_MAX; i++)
	{
		// 空いてる場所を探す
		if (g_AnimPattern[i].m_TextureId >= 0) continue;

		g_AnimPattern[i].m_TextureId = texId;
		g_AnimPattern[i].m_PatternMax = pattern_Max;
		g_AnimPattern[i].m_HPatternMax = h_pattern_Max;
		g_AnimPattern[i].m_seconds_per_pattern = seconds_per_pattern;
		g_AnimPattern[i].m_PatternSize = pattern_size;
		g_AnimPattern[i].m_StartPosition = start_position;
		g_AnimPattern[i].m_IsLooped = is_looped;

		return i; // これが大事
	}

	return -1; // 容量がないよ～～～～
}

int SpriteAnim_CreatePlayer(int anim_pattern_id)
{
	for (int i = 0; i < ANIM_PLAY_MAX; i++) {

		if (g_AnimPlay[i].m_PatternId >= 0) continue;

		g_AnimPlay[i].m_PatternId = anim_pattern_id;
		g_AnimPlay[i].m_AccumulatedTime = 0.0;
		g_AnimPlay[i].m_PatternNum = 0;

		g_AnimPlay[i].m_IsStopped = false; // 再生中にする

		return i;
	}
	return -1;
}

bool SpriteAnim_IsStopped(int index)
{
	return g_AnimPlay[index].m_IsStopped; // 停止中かどうかを返す
}

void SpriteAnim_DestroyPlayer(int index)
{
	g_AnimPlay[index].m_PatternId = -1;
	g_AnimPlay[index].m_IsStopped = false;
	g_AnimPlay[index].m_PatternNum = 0;
	g_AnimPlay[index].m_AccumulatedTime = 0.0;
}
