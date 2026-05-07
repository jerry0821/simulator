#ifndef INSTANCING_DEBUG_H
#define INSTANCING_DEBUG_H

struct InstancingStats
{
	int instanced_batch_count = 0;
	int instanced_instance_count = 0;
	int estimated_draw_calls_saved = 0;
};

void InstancingDebug_ResetStats();
void InstancingDebug_AddBatch(int instance_count);

bool InstancingDebug_IsEnabled();
void InstancingDebug_SetEnabled(bool enabled);

InstancingStats InstancingDebug_GetStats();

#endif // INSTANCING_DEBUG_H
