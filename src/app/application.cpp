#include "application.h"

#include "system_timer.h"

bool Application::Initialize(HINSTANCE instance_handle, int show_command)
{
	if (!InitializePlatform(instance_handle, show_command))
	{
		return false;
	}

	if (!InitializeEngineSystems())
	{
		return false;
	}

	InitializeRuntimeObjects();
	InitializeDebugTools();
	ResetFrameState();

	return true;
}

int Application::Run()
{
	MSG message{};

	do
	{
		if (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		else
		{
			const double current_time = SystemTimer_GetTime();
			UpdateFps(current_time);
			TickFrame(current_time);
		}
	} while (message.message != WM_QUIT);

	return static_cast<int>(message.wParam);
}

void Application::Shutdown()
{
#if defined(DEBUG) || defined(_DEBUG)
	m_debug_text.reset();
#endif
	m_scene_render_adapter.reset();
	m_renderer.reset();

	FinalizeEngineSystems();
}

