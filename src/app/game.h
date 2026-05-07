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
#include "map_editor.h"
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
	// Small runtime state for game-side rendering.
	int m_test_texture = -1;
	MapController m_map_controller{};
	MapEditorController m_map_editor_controller{m_map_controller};
};

#endif // GAME_H

