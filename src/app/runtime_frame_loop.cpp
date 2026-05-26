#include "application.h"

#include "debug_menu.h"
#include "frustum_culling_debug.h"
#include "instancing_debug.h"
#include "system_timer.h"

void Application::ResetFrameState()
{
	m_exec_last_time = SystemTimer_GetTime();
	m_fps_last_time = m_exec_last_time;
	m_frame_count = 0;
	m_fps = 0.0;
	m_glitch_amount = 0.0f;
}

void Application::TickFrame(double current_time)
{
	const double elapsed_time = current_time - m_exec_last_time;
	m_exec_last_time = current_time;

	BeginFrame(current_time, elapsed_time);
	RenderCurrentFrame(current_time, elapsed_time);
	RenderDebugText();

	DebugMenu_SetInstancingStats(InstancingDebug_GetStats());
	DebugMenu_SetFrustumCullingStats(FrustumCullingDebug_GetStats());
	DebugMenu_SetPerformanceStats(static_cast<float>(m_fps),
								  m_fps > 0.0 ? static_cast<float>(1000.0 / m_fps) : 0.0f);

	m_scene_controller.Refresh();
	++m_frame_count;
}
