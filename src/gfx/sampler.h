// ----------------------------------------------------
// Sampler utility [sampler.h]
// ====================================================
// Created by: Jerry
// Date: 2025-09-11
// Version: 1.0
// ----------------------------------------------------
#ifndef SAMPLER_H
#define SAMPLER_H

#include <d3d11.h>

namespace Backend::DX11::Sampler
{
	void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
	void Finalize();

	void SetPointFilter();
	void SetLinearFilter();
	void SetAnisotropicFilter();

	ID3D11SamplerState* GetState();
}

#endif // SAMPLER_H
