#include "shadow_pass.h"

#include <array>

#include "light.h"
#include "render_backend_dx11.h"
#include "render_frame_context.h"
#include "render_scene.h"
#include "shader_shadow.h"
#include "shadow_map_dx11.h"
#include "render_resource_usage.h"

ShadowPass::ShadowPass(RenderBackendDX11& backend)
	: backend_(backend),
	  shadow_map_(std::make_unique<ShadowMapDX11>())
{
	shadow_map_->initialize(4096, 4096);
}

std::string_view ShadowPass::name() const
{
	return "ShadowPass";
}

std::span<const RenderResourceUsage> ShadowPass::resources() const
{
	static constexpr std::array usages = {
		RenderResourceUsage{ RenderResourceId::ShadowMap, RenderResourceAccess::Write }
	};
	return usages;
}

void ShadowPass::execute(const RenderFrameContext& frame_context)
{
	(void)backend_;

	shadow_map_->beginPass();
	ShaderShadow_Begin();
	ShaderShadow_SetLightViewProjection(Light_GetLightViewProjectionMatrix());

	if (frame_context.render_scene != nullptr)
	{
		frame_context.render_scene->drawShadow(frame_context);
	}

	ShaderShadow_End();
}

ID3D11ShaderResourceView* ShadowPass::shadowShaderResourceView() const
{
	return shadow_map_->shaderResourceView();
}
