#include "instancing_debug.h"

namespace
{
bool g_instancing_enabled = true;
InstancingStats g_instancing_stats{};
}

void InstancingDebug_ResetStats()
{
	g_instancing_stats = {};
}

void InstancingDebug_AddBatch(int instance_count)
{
	if (instance_count <= 0)
	{
		return;
	}

	++g_instancing_stats.instanced_batch_count;
	g_instancing_stats.instanced_instance_count += instance_count;
	g_instancing_stats.estimated_draw_calls_saved += instance_count - 1;
}

bool InstancingDebug_IsEnabled()
{
	return g_instancing_enabled;
}

void InstancingDebug_SetEnabled(bool enabled)
{
	g_instancing_enabled = enabled;
}

InstancingStats InstancingDebug_GetStats()
{
	return g_instancing_stats;
}
