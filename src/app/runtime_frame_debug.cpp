#include "application.h"

#include <sstream>

#include "debug_text.h"

void Application::RenderDebugText()
{
#if defined(DEBUG) || defined(_DEBUG)
	if (!m_debug_text)
	{
		return;
	}

	std::stringstream stream;
	stream << "fps:" << m_fps << std::endl;
	m_debug_text->SetText(stream.str().c_str(), { 1.0f, 0.0f, 1.0f, 1.0f });
	m_debug_text->Draw();
	m_debug_text->Clear();
#endif
}

void Application::UpdateFps(double current_time)
{
	const double elapsed_time = current_time - m_fps_last_time;
	if (elapsed_time < 1.0)
	{
		return;
	}

	m_fps = m_frame_count / elapsed_time;
	m_fps_last_time = current_time;
	m_frame_count = 0;
}
