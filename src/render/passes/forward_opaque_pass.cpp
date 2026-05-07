#include "forward_opaque_pass.h"

#include <array>

#include "render_frame_context.h"
#include "render_backend_dx11.h"
#include "render_scene.h"
#include "render_resource_usage.h"

ForwardOpaquePass::ForwardOpaquePass(RenderBackendDX11& backend)
	: backend_(backend)
{
}

std::string_view ForwardOpaquePass::name() const
{
	return "ForwardOpaquePass";
}

std::span<const RenderResourceUsage> ForwardOpaquePass::resources() const
{
	static constexpr std::array usages = {
		RenderResourceUsage{ RenderResourceId::ShadowMap, RenderResourceAccess::Read },
		RenderResourceUsage{ RenderResourceId::SceneDepth, RenderResourceAccess::ReadWrite },
		RenderResourceUsage{ RenderResourceId::SceneColor, RenderResourceAccess::Write }
	};
	return usages;
}

void ForwardOpaquePass::execute(const RenderFrameContext& frame_context)
{
	backend_.beginOpaquePass();

	if (frame_context.render_scene != nullptr)
	{
		RenderFrameContext pass_context = frame_context;
		pass_context.resources.shadow_map = Backend::RenderShadowMapResource(shadow_shader_resource_view_);
		frame_context.render_scene->drawForwardOpaque(pass_context);
	}
}
