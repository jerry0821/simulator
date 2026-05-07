#ifndef RENDERER_H
#define RENDERER_H

#include <memory>
#include <vector>

#include "render_backend_dx11.h"
#include "render_frame_context.h"
#include "render_frame_plan.h"
#include "render_pass.h"

class ForwardOpaquePass;
class ShadowPass;
class TransparencyPass;

class Renderer
{
public:
	explicit Renderer(RenderBackendDX11& backend);

	void RenderFrame(const RenderFrameContext& frame_context);
	RenderFramePlan BuildFramePlan() const;

private:
	void InstallDefaultPasses();

	RenderBackendDX11& backend_;
	std::vector<std::unique_ptr<RenderPass>> passes_;
	ShadowPass* shadow_pass_ = nullptr;
	ForwardOpaquePass* forward_opaque_pass_ = nullptr;
	TransparencyPass* transparency_pass_ = nullptr;
};

#endif // RENDERER_H
