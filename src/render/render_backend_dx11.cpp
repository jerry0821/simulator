#include "render_backend_dx11.h"

#include "direct3d.h"

void RenderBackendDX11::bindScenePass() const
{
	Direct3D_SetSceneRenderTarget();
}

void RenderBackendDX11::beginDepthPrePass() const
{
	bindScenePass();
	Direct3D_ClearSceneDepth();
}

void RenderBackendDX11::beginOpaquePass() const
{
	bindScenePass();
	Direct3D_ClearSceneColor();
}

void RenderBackendDX11::beginScenePass() const
{
	bindScenePass();
	Direct3D_ClearScene();
}

void RenderBackendDX11::beginFrame() const
{
	// Reserved for per-frame backend work.
}

void RenderBackendDX11::endFrame() const
{
	Direct3D_Present();
}

RenderBackendInfo RenderBackendDX11::info() const
{
	return {
		Direct3D_GetBackBufferWidth(),
		Direct3D_GetBackBufferHeight(),
	};
}

void RenderBackendDX11::beginBackbufferPass() const
{
	Direct3D_ClearBackbuffer();
	Direct3D_SetBackbuffer();
}

Backend::RenderSceneColorResource RenderBackendDX11::sceneColorResource() const
{
	return Backend::RenderSceneColorResource(Direct3D_GetSceneSRV());
}

Backend::RenderDepthResource RenderBackendDX11::sceneDepthResource() const
{
	return Backend::RenderDepthResource(Direct3D_GetSceneDSV(), Direct3D_GetSceneDepthSRV());
}
