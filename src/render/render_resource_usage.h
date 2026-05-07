#ifndef RENDER_RESOURCE_USAGE_H
#define RENDER_RESOURCE_USAGE_H

enum class RenderResourceId
{
	SceneColor,
	SceneDepth,
	ShadowMap,
	BackBuffer
};

enum class RenderResourceAccess
{
	Read,
	Write,
	ReadWrite
};

struct RenderResourceUsage
{
	RenderResourceId resource_id = RenderResourceId::SceneColor;
	RenderResourceAccess access = RenderResourceAccess::Read;
};

#endif // RENDER_RESOURCE_USAGE_H
