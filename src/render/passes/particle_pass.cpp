#include "particle_pass.h"

#include <array>

#include "render_backend_dx11.h"
#include "render_frame_context.h"
#include "render_scene.h"
#include "render_resource_usage.h"

ParticlePass::ParticlePass(RenderBackendDX11& backend)
	: backend_(backend)
{
}

std::string_view ParticlePass::name() const
{
	return "ParticlePass";
}

std::span<const RenderResourceUsage> ParticlePass::resources() const
{
	static constexpr std::array usages = {
		RenderResourceUsage{ RenderResourceId::SceneDepth, RenderResourceAccess::Read },
		RenderResourceUsage{ RenderResourceId::SceneColor, RenderResourceAccess::ReadWrite }
	};
	return usages;
}

void ParticlePass::execute(const RenderFrameContext& frame_context)
{
	// Particles are layered after geometry.
	backend_.bindScenePass();

	if (frame_context.render_scene != nullptr)
	{
		frame_context.render_scene->drawParticles(frame_context);
	}
}
