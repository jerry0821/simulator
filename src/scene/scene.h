// ----------------------------------------------------
// 画面遷移制御 [scene.h]
// ====================================================
// Created by: Yasuda Atsushi
// Date: 2025-07-10
// Version: 1.0
// ----------------------------------------------------
#ifndef SCENE_H
#define SCENE_H

#include "game.h"

struct RenderFrameContext;
struct WaterSurfaceDesc;

enum Scene
{
	SCENE_TITLE, // シーンが増えたらここに足していく
	SCENE_GAME,
	SCENE_GAMEOVER,
	SCENE_RESULT,
	SCENE_MAX
};

class SceneController
{
public:
	SceneController() = default;

	void Initialize();
	void Finalize();
	void Update(double elapsed_time);
	void Draw(const RenderFrameContext& frame_context);
	void DrawShadow(const RenderFrameContext& frame_context);
	void DrawDepthPrePass(const RenderFrameContext& frame_context);
	bool GetWaterSurfaceDesc(WaterSurfaceDesc& out_desc);
	void DrawTransparency(const RenderFrameContext& frame_context);
	void DrawParticles(const RenderFrameContext& frame_context);
	void Refresh();
	void Change(Scene scene);

private:
	void InitializeCurrentScene();
	void FinalizeCurrentScene();

	Scene m_current_scene = SCENE_GAME;
	Scene m_next_scene = SCENE_GAME;
	GameController m_game_controller{};
};

#endif // SCENE_H
