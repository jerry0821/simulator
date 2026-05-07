#ifndef TRANSPARENCY_PASS_H
#define TRANSPARENCY_PASS_H

#include "render_pass.h"

#include <d3d11.h>

class RenderBackendDX11;

class TransparencyPass : public RenderPass
{
public:
	explicit TransparencyPass(RenderBackendDX11& backend);

	std::string_view name() const override;
	std::span<const RenderResourceUsage> resources() const override;
	void execute(const RenderFrameContext& frame_context) override;

	void setShadowShaderResourceView(ID3D11ShaderResourceView* shadow_shader_resource_view)
	{
		shadow_shader_resource_view_ = shadow_shader_resource_view;
	}

private:
	RenderBackendDX11& backend_;
	ID3D11ShaderResourceView* shadow_shader_resource_view_ = nullptr;
};

#endif // TRANSPARENCY_PASS_H
