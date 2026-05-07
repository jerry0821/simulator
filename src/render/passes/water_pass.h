#ifndef WATER_PASS_H
#define WATER_PASS_H

#include <d3d11.h>
#include <DirectXMath.h>

#include "render_pass.h"

class RenderBackendDX11;

class WaterPass : public RenderPass
{
public:
	explicit WaterPass(RenderBackendDX11& backend);

	std::string_view name() const override;
	std::span<const RenderResourceUsage> resources() const override;
	void execute(const RenderFrameContext& frame_context) override;

private:
	void drawSurfaceLayer(ID3D11ShaderResourceView* texture_srv,
						  const DirectX::XMMATRIX& world,
						  const DirectX::XMFLOAT4& color) const;
	void drawSurfacePlaneLayer(ID3D11ShaderResourceView* texture_srv,
							   const DirectX::XMMATRIX& world,
							   const DirectX::XMFLOAT4& color) const;
	void drawSurfacePlaneHighlightLayer(ID3D11ShaderResourceView* texture_srv,
										const DirectX::XMMATRIX& world,
										const DirectX::XMFLOAT4& color) const;

	RenderBackendDX11& backend_;
};

#endif // WATER_PASS_H
