// ----------------------------------------------------
// Scene routing [scene.cpp]
// ====================================================
// Created by: Jerry
// Date: 2026-03-25
// ----------------------------------------------------

#include "scene.h"

#include "render_frame_context.h"
#include "render_water_surface.h"

void SceneController::Initialize()
{
	InitializeCurrentScene();
}

void SceneController::Finalize()
{
	FinalizeCurrentScene();
}

void SceneController::Update(double elapsed_time)
{
	switch (m_current_scene)
	{
	case SCENE_TITLE:
		break;
	case SCENE_GAME:
		m_game_controller.Update(elapsed_time);
		break;
	case SCENE_GAMEOVER:
		break;
	case SCENE_RESULT:
		break;
	}
}

void SceneController::Draw(const RenderFrameContext& frame_context)
{
	switch (m_current_scene)
	{
	case SCENE_TITLE:
		break;
	case SCENE_GAME:
		m_game_controller.Draw(frame_context);
		break;
	case SCENE_GAMEOVER:
		break;
	case SCENE_RESULT:
		break;
	}
}

void SceneController::DrawShadow(const RenderFrameContext& frame_context)
{
	switch (m_current_scene)
	{
	case SCENE_TITLE:
		break;
	case SCENE_GAME:
		m_game_controller.DrawShadow(frame_context);
		break;
	case SCENE_GAMEOVER:
		break;
	case SCENE_RESULT:
		break;
	}
}

void SceneController::DrawDepthPrePass(const RenderFrameContext& frame_context)
{
	switch (m_current_scene)
	{
	case SCENE_TITLE:
		break;
	case SCENE_GAME:
		m_game_controller.DrawDepthPrePass(frame_context);
		break;
	case SCENE_GAMEOVER:
		break;
	case SCENE_RESULT:
		break;
	}
}

bool SceneController::GetWaterSurfaceDesc(WaterSurfaceDesc& out_desc)
{
	switch (m_current_scene)
	{
	case SCENE_TITLE:
		break;
	case SCENE_GAME:
		return m_game_controller.GetWaterSurfaceDesc(out_desc);
	case SCENE_GAMEOVER:
		break;
	case SCENE_RESULT:
		break;
	}

	return false;
}

void SceneController::DrawTransparency(const RenderFrameContext& frame_context)
{
	switch (m_current_scene)
	{
	case SCENE_TITLE:
		break;
	case SCENE_GAME:
		m_game_controller.DrawTransparency(frame_context);
		break;
	case SCENE_GAMEOVER:
		break;
	case SCENE_RESULT:
		break;
	}
}

void SceneController::DrawParticles(const RenderFrameContext& frame_context)
{
	switch (m_current_scene)
	{
	case SCENE_TITLE:
		break;
	case SCENE_GAME:
		m_game_controller.DrawParticles(frame_context);
		break;
	case SCENE_GAMEOVER:
		break;
	case SCENE_RESULT:
		break;
	}
}

void SceneController::Refresh()
{
	if (m_current_scene == m_next_scene)
	{
		return;
	}

	FinalizeCurrentScene();
	m_current_scene = m_next_scene;
	InitializeCurrentScene();
}

void SceneController::Change(Scene scene)
{
	m_next_scene = scene;
}

void SceneController::InitializeCurrentScene()
{
	switch (m_current_scene)
	{
	case SCENE_TITLE:
		break;
	case SCENE_GAME:
		m_game_controller.Initialize();
		break;
	case SCENE_GAMEOVER:
		break;
	case SCENE_RESULT:
		break;
	}
}

void SceneController::FinalizeCurrentScene()
{
	switch (m_current_scene)
	{
	case SCENE_TITLE:
		break;
	case SCENE_GAME:
		m_game_controller.Finalize();
		break;
	case SCENE_GAMEOVER:
		break;
	case SCENE_RESULT:
		break;
	}
}
