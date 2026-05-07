#ifndef SHADOW_PASS_H
#define SHADOW_PASS_H

#include <d3d11.h>
#include <memory>

#include "render_pass.h"

class RenderBackendDX11;
class ShadowMapDX11;

class ShadowPass : public RenderPass
{
public:
	explicit ShadowPass(RenderBackendDX11& backend);

	std::string_view name() const override;
	std::span<const RenderResourceUsage> resources() const override;
	void execute(const RenderFrameContext& frame_context) override;

	ID3D11ShaderResourceView* shadowShaderResourceView() const;

private:
	RenderBackendDX11& backend_;
	std::unique_ptr<ShadowMapDX11> shadow_map_;
};

#endif // SHADOW_PASS_H
