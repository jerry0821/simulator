#include "shadow_map_dx11.h"

#include "direct3d.h"

ShadowMapDX11::~ShadowMapDX11()
{
	finalize();
}

bool ShadowMapDX11::initialize(unsigned int width, unsigned int height)
{
	finalize();

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = Direct3D_GetDevice()->CreateTexture2D(&desc, nullptr, &shadow_texture_);
	if (FAILED(hr)) return false;

	D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
	dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsv_desc.Texture2D.MipSlice = 0;
	hr = Direct3D_GetDevice()->CreateDepthStencilView(shadow_texture_, &dsv_desc, &shadow_dsv_);
	if (FAILED(hr)) return false;

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = 1;
	hr = Direct3D_GetDevice()->CreateShaderResourceView(shadow_texture_, &srv_desc, &shadow_srv_);
	if (FAILED(hr)) return false;

	viewport_.TopLeftX = 0.0f;
	viewport_.TopLeftY = 0.0f;
	viewport_.Width = static_cast<float>(width);
	viewport_.Height = static_cast<float>(height);
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;

	return true;
}

void ShadowMapDX11::finalize()
{
	SAFE_RELEASE(shadow_srv_);
	SAFE_RELEASE(shadow_dsv_);
	SAFE_RELEASE(shadow_texture_);
}

void ShadowMapDX11::beginPass() const
{
	ID3D11ShaderResourceView* null_srv = nullptr;
	auto* context = Direct3D_GetContext();
	context->PSSetShaderResources(2, 1, &null_srv);
	context->RSSetViewports(1, &viewport_);
	context->OMSetRenderTargets(0, nullptr, shadow_dsv_);
	context->ClearDepthStencilView(shadow_dsv_, D3D11_CLEAR_DEPTH, 1.0f, 0);
}
