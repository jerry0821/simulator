#ifndef FRUSTUM_CULLING_DEBUG_H
#define FRUSTUM_CULLING_DEBUG_H

struct FrustumCullingStats
{
	int tested_objects = 0;
	int visible_objects = 0;
	int culled_objects = 0;
};

void FrustumCullingDebug_ResetStats();
void FrustumCullingDebug_RecordVisible();
void FrustumCullingDebug_RecordCulled();
void FrustumCullingDebug_RecordVisibleCount(int count);
void FrustumCullingDebug_RecordCulledCount(int count);

bool FrustumCullingDebug_IsEnabled();
void FrustumCullingDebug_SetEnabled(bool enabled);

FrustumCullingStats FrustumCullingDebug_GetStats();

#endif // FRUSTUM_CULLING_DEBUG_H
