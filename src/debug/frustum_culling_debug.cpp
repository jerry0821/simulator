#include "frustum_culling_debug.h"

namespace
{
bool g_FrustumCullingEnabled = true;
FrustumCullingStats g_FrustumCullingStats{};
}

void FrustumCullingDebug_ResetStats()
{
	g_FrustumCullingStats = {};
}

void FrustumCullingDebug_RecordVisible()
{
	++g_FrustumCullingStats.tested_objects;
	++g_FrustumCullingStats.visible_objects;
}

void FrustumCullingDebug_RecordCulled()
{
	++g_FrustumCullingStats.tested_objects;
	++g_FrustumCullingStats.culled_objects;
}

void FrustumCullingDebug_RecordVisibleCount(int count)
{
	if (count <= 0)
	{
		return;
	}

	g_FrustumCullingStats.tested_objects += count;
	g_FrustumCullingStats.visible_objects += count;
}

void FrustumCullingDebug_RecordCulledCount(int count)
{
	if (count <= 0)
	{
		return;
	}

	g_FrustumCullingStats.tested_objects += count;
	g_FrustumCullingStats.culled_objects += count;
}

bool FrustumCullingDebug_IsEnabled()
{
	return g_FrustumCullingEnabled;
}

void FrustumCullingDebug_SetEnabled(bool enabled)
{
	g_FrustumCullingEnabled = enabled;
}

FrustumCullingStats FrustumCullingDebug_GetStats()
{
	return g_FrustumCullingStats;
}
