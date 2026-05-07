#ifndef FORWARD_OPAQUE_PASS_H
#define FORWARD_OPAQUE_PASS_H

#include "render_pass.h"

#include <d3d11.h>

class RenderBackendDX11;

class ForwardOpaquePass : public RenderPass
{
public:
	explicit ForwardOpaquePass(RenderBackendDX11& backend);

	std::string_view name() const override;
	std::span<const RenderResourceUsage> resources() const override;
	void execute(const RenderFrameContext& frame_context) override;

private:
	RenderBackendDX11& backend_;
	ID3D11ShaderResourceView* shadow_shader_resource_view_ = nullptr;

public:
	void setShadowShaderResourceView(ID3D11ShaderResourceView* shadow_shader_resource_view)
	{
		shadow_shader_resource_view_ = shadow_shader_resource_view;
	}
};

#endif // FORWARD_OPAQUE_PASS_H
