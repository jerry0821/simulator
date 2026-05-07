#include "transparency_pass.h"

#include <array>

#include "render_backend_dx11.h"
#include "render_frame_context.h"
#include "render_scene.h"
#include "render_resource_usage.h"

TransparencyPass::TransparencyPass(RenderBackendDX11& backend)
	: backend_(backend)
{
}

std::string_view TransparencyPass::name() const
{
	return "TransparencyPass";
}

std::span<const RenderResourceUsage> TransparencyPass::resources() const
{
	static constexpr std::array usages = {
		RenderResourceUsage{ RenderResourceId::ShadowMap, RenderResourceAccess::Read },
		RenderResourceUsage{ RenderResourceId::SceneDepth, RenderResourceAccess::Read },
		RenderResourceUsage{ RenderResourceId::SceneColor, RenderResourceAccess::ReadWrite }
	};
	return usages;
}

void TransparencyPass::execute(const RenderFrameContext& frame_context)
{
	// Alpha objects are layered after opaque.
	backend_.bindScenePass();

	if (frame_context.render_scene != nullptr)
	{
		RenderFrameContext pass_context = frame_context;
		pass_context.resources.shadow_map = Backend::RenderShadowMapResource(shadow_shader_resource_view_);
		frame_context.render_scene->drawTransparency(pass_context);
	}
}
