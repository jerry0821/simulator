#ifndef RENDER_BACKEND_DX11_H
#define RENDER_BACKEND_DX11_H

#include <d3d11.h>

#include "render_shadow_map_resource.h"

struct RenderBackendInfo
{
	unsigned int backbuffer_width = 0;
	unsigned int backbuffer_height = 0;
};

class RenderBackendDX11
{
public:
	void beginFrame() const;
	void endFrame() const;
	RenderBackendInfo info() const;

	void bindScenePass() const;
	void bindScenePassReadOnlyDepth() const;
	void beginDepthPrePass() const;
	void beginOpaquePass() const;
	void beginScenePass() const;
	void beginBackbufferPass() const;
	void resolveSceneColor() const;

	Backend::RenderSceneColorResource sceneColorResource() const;
	Backend::RenderDepthResource sceneDepthResource() const;
};

#endif // RENDER_BACKEND_DX11_H
