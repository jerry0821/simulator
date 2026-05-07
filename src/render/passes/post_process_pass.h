#ifndef POST_PROCESS_PASS_H
#define POST_PROCESS_PASS_H

#include <d3d11.h>
#include "render_pass.h"

class RenderBackendDX11;
struct PostProcessSettings;

class PostProcessPass : public RenderPass
{
public:
	explicit PostProcessPass(RenderBackendDX11& backend);
	~PostProcessPass() override;

	std::string_view name() const override;
	std::span<const RenderResourceUsage> resources() const override;
	void execute(const RenderFrameContext& frame_context) override;

private:
	bool EnsureBloomShaders();
	bool EnsureBloomTargets(unsigned int width, unsigned int height);
	void ReleaseBloomResources();
	ID3D11ShaderResourceView* RenderBloom(
		ID3D11ShaderResourceView* scene_srv,
		const PostProcessSettings& settings);

	ID3D11Texture2D* bloom_tex_a_ = nullptr;
	ID3D11RenderTargetView* bloom_rtv_a_ = nullptr;
	ID3D11ShaderResourceView* bloom_srv_a_ = nullptr;
	ID3D11Texture2D* bloom_tex_b_ = nullptr;
	ID3D11RenderTargetView* bloom_rtv_b_ = nullptr;
	ID3D11ShaderResourceView* bloom_srv_b_ = nullptr;
	ID3D11PixelShader* bloom_prefilter_ps_ = nullptr;
	ID3D11PixelShader* bloom_blur_ps_ = nullptr;
	ID3D11Buffer* bloom_constant_buffer_ = nullptr;
	unsigned int bloom_width_ = 0;
	unsigned int bloom_height_ = 0;
	RenderBackendDX11& backend_;
};

#endif // POST_PROCESS_PASS_H
