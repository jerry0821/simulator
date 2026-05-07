#include "scene_render_adapter.h"

#include "debug_menu.h"
#include "render_frame_context.h"
#include "render_water_surface.h"
#include "scene.h"

SceneRenderAdapter::SceneRenderAdapter(SceneController& scene_controller)
	: m_scene_controller(scene_controller)
{
}

void SceneRenderAdapter::drawShadow(const RenderFrameContext& frame_context)
{
	m_scene_controller.DrawShadow(frame_context);
}

void SceneRenderAdapter::drawDepthPrePass(const RenderFrameContext& frame_context)
{
	m_scene_controller.DrawDepthPrePass(frame_context);
}

void SceneRenderAdapter::drawForwardOpaque(const RenderFrameContext& frame_context)
{
	m_scene_controller.Draw(frame_context);
}

bool SceneRenderAdapter::getWaterSurfaceDesc(WaterSurfaceDesc& out_desc)
{
	return m_scene_controller.GetWaterSurfaceDesc(out_desc);
}

void SceneRenderAdapter::drawTransparency(const RenderFrameContext& frame_context)
{
	m_scene_controller.DrawTransparency(frame_context);
}

void SceneRenderAdapter::drawParticles(const RenderFrameContext& frame_context)
{
	m_scene_controller.DrawParticles(frame_context);
}

void SceneRenderAdapter::drawUI(const RenderFrameContext& frame_context)
{
	DebugMenu_Draw(&frame_context);
	DebugMenu_End();
}
