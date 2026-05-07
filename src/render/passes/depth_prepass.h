#ifndef DEPTH_PREPASS_H
#define DEPTH_PREPASS_H

#include "render_pass.h"

class RenderBackendDX11;

class DepthPrePass : public RenderPass
{
public:
	explicit DepthPrePass(RenderBackendDX11& backend);

	std::string_view name() const override;
	std::span<const RenderResourceUsage> resources() const override;
	void execute(const RenderFrameContext& frame_context) override;

private:
	RenderBackendDX11& backend_;
};

#endif // DEPTH_PREPASS_H
