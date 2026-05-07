#ifndef SHADOW_MAP_DX11_H
#define SHADOW_MAP_DX11_H

#include <d3d11.h>

class ShadowMapDX11
{
public:
	ShadowMapDX11() = default;
	~ShadowMapDX11();

	ShadowMapDX11(const ShadowMapDX11&) = delete;
	ShadowMapDX11& operator=(const ShadowMapDX11&) = delete;

	bool initialize(unsigned int width, unsigned int height);
	void finalize();

	void beginPass() const;

	ID3D11ShaderResourceView* shaderResourceView() const { return shadow_srv_; }

private:
	ID3D11Texture2D* shadow_texture_ = nullptr;
	ID3D11DepthStencilView* shadow_dsv_ = nullptr;
	ID3D11ShaderResourceView* shadow_srv_ = nullptr;
	D3D11_VIEWPORT viewport_ = {};
};

#endif // SHADOW_MAP_DX11_H
