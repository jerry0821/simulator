// ----------------------------------------------------
// ゲーム本体 [game.h]
// ====================================================
// Created by: Yasuda Atsushi
// Date: 2025-06-27
// Version: 1.0
// ----------------------------------------------------
#ifndef GAME_H
#define GAME_H

#include "map.h"
#include <DirectXMath.h>
#include <vector>

struct RenderFrameContext;
struct WaterSurfaceDesc;
struct MODEL;

class GameController
{
public:
	void Initialize();
	void Finalize();

	void Update(double elapsed_time);
	void Draw(const RenderFrameContext& frame_context);
	void DrawDepthPrePass(const RenderFrameContext& frame_context);
	bool GetWaterSurfaceDesc(WaterSurfaceDesc& out_desc);
	void DrawTransparency(const RenderFrameContext& frame_context);
	void DrawParticles(const RenderFrameContext& frame_context);
	void DrawShadow(const RenderFrameContext& frame_context);

private:
	// Sphere State
	DirectX::XMFLOAT3 m_sphere_pos = { 46.0f, 30.0f, 118.0f };
	float m_sphere_vel_y = 0.0f;
	float m_sphere_yaw = 0.0f;
	bool m_third_person_mode = false;

	// Small runtime state for game-side rendering.
	int m_test_texture = -1;
	MapController m_map_controller{};
};

#endif // GAME_H

