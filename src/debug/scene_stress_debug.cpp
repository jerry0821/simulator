#include "scene_stress_debug.h"

namespace
{
bool g_SceneStressEnabled = false;
}

bool SceneStressDebug_IsEnabled()
{
	return g_SceneStressEnabled;
}

void SceneStressDebug_SetEnabled(bool enabled)
{
	g_SceneStressEnabled = enabled;
}
