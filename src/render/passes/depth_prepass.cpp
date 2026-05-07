#include "depth_prepass.h"

#include <array>
#include <DirectXMath.h>

#include "render_frame_context.h"
#include "render_backend_dx11.h"
#include "render_scene.h"
#include "render_resource_usage.h"
#include "shader_shadow.h"

DepthPrePass::DepthPrePass(RenderBackendDX11& backend)
	: backend_(backend)
{
}

std::string_view DepthPrePass::name() const
{
	return "DepthPrePass";
}

std::span<const RenderResourceUsage> DepthPrePass::resources() const
{
	static constexpr std::array usages = {
		RenderResourceUsage{ RenderResourceId::SceneDepth, RenderResourceAccess::Write }
	};
	return usages;
}

void DepthPrePass::execute(const RenderFrameContext& frame_context)
{
	backend_.beginDepthPrePass();

	// Reuse the depth-only shader for camera depth.
	const auto view = DirectX::XMLoadFloat4x4(&frame_context.globals.view_matrix);
	const auto projection = DirectX::XMLoadFloat4x4(&frame_context.globals.projection_matrix);

	ShaderShadow_Begin();
	ShaderShadow_SetViewProjection(view * projection);

	if (frame_context.render_scene != nullptr)
	{
		frame_context.render_scene->drawDepthPrePass(frame_context);
	}

	ShaderShadow_End();
}
