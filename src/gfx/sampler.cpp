// ----------------------------------------------------
// Sampler utility [sampler.cpp]
// ====================================================
// Created by: Jerry
// Date: 2025-09-11
// Version: 1.0
// ----------------------------------------------------
#include "sampler.h"

#include "direct3d.h"

namespace Backend::DX11::Sampler
{
	static ID3D11DeviceContext* g_pContext = nullptr;

	static ID3D11SamplerState* g_pSamplerFilterPoint = nullptr;
	static ID3D11SamplerState* g_pSamplerFilterLinear = nullptr;
	static ID3D11SamplerState* g_pSamplerFilterAnisotropic = nullptr;

	void Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	{
		g_pContext = pContext;

		D3D11_SAMPLER_DESC sampler_desc{};
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.BorderColor[0] = 0.0f;
		sampler_desc.BorderColor[1] = 0.0f;
		sampler_desc.BorderColor[2] = 0.0f;
		sampler_desc.BorderColor[3] = 0.0f;
		sampler_desc.MipLODBias = 0.0f;
		sampler_desc.MaxAnisotropy = 16;
		sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		sampler_desc.MinLOD = 0.0f;
		sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		pDevice->CreateSamplerState(&sampler_desc, &g_pSamplerFilterPoint);

		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		pDevice->CreateSamplerState(&sampler_desc, &g_pSamplerFilterLinear);

		sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
		pDevice->CreateSamplerState(&sampler_desc, &g_pSamplerFilterAnisotropic);
	}

	void Finalize()
	{
		SAFE_RELEASE(g_pSamplerFilterAnisotropic);
		SAFE_RELEASE(g_pSamplerFilterLinear);
		SAFE_RELEASE(g_pSamplerFilterPoint);
	}

	void SetPointFilter()
	{
		g_pContext->PSSetSamplers(0, 1, &g_pSamplerFilterPoint);
		g_pContext->VSSetSamplers(0, 1, &g_pSamplerFilterPoint);
	}

	void SetLinearFilter()
	{
		g_pContext->PSSetSamplers(0, 1, &g_pSamplerFilterLinear);
		g_pContext->VSSetSamplers(0, 1, &g_pSamplerFilterLinear);
	}

	void SetAnisotropicFilter()
	{
		g_pContext->PSSetSamplers(0, 1, &g_pSamplerFilterAnisotropic);
		g_pContext->VSSetSamplers(0, 1, &g_pSamplerFilterAnisotropic);
	}

	ID3D11SamplerState* GetState()
	{
		return g_pSamplerFilterAnisotropic;
	}
}
