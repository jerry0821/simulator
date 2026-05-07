#ifndef PARTICLE_PASS_H
#define PARTICLE_PASS_H

#include "render_pass.h"

class RenderBackendDX11;

class ParticlePass : public RenderPass
{
public:
	explicit ParticlePass(RenderBackendDX11& backend);

	std::string_view name() const override;
	std::span<const RenderResourceUsage> resources() const override;
	void execute(const RenderFrameContext& frame_context) override;

private:
	RenderBackendDX11& backend_;
};

#endif // PARTICLE_PASS_H
