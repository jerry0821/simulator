#include "compute_water_surface_height_texture.h"

#include <d3d11.h>
#include <fstream>
#include <vector>

#include "debug_ostream.h"
#include "sampler.h"

namespace
{
template <typename T>
void SafeRelease(T*& resource)
{
	if (resource != nullptr)
	{
		resource->Release();
		resource = nullptr;
	}
}
}

bool ComputeWaterSurfaceHeightTexture::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	Finalize();

	m_device = device;
	m_context = context;
	if (m_device == nullptr || m_context == nullptr)
	{
		return false;
	}

	std::ifstream shader_stream("resource/shader/shader_compute_water_surface_height.cso", std::ios::binary);
	if (!shader_stream)
	{
		hal::dout << "ComputeWaterSurfaceHeightTexture::Initialize(): failed to open shader_compute_water_surface_height.cso" << std::endl;
		return false;
	}

	const std::vector<char> shader_bytes((std::istreambuf_iterator<char>(shader_stream)),
										 std::istreambuf_iterator<char>());
	if (shader_bytes.empty())
	{
		return false;
	}

	if (FAILED(m_device->CreateComputeShader(shader_bytes.data(), shader_bytes.size(), nullptr, &m_compute_shader)))
	{
		hal::dout << "ComputeWaterSurfaceHeightTexture::Initialize(): CreateComputeShader failed" << std::endl;
		Finalize();
		return false;
	}

	D3D11_TEXTURE2D_DESC texture_desc{};
	texture_desc.Width = kTextureWidth;
	texture_desc.Height = kTextureHeight;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = DXGI_FORMAT_R32_FLOAT;
	texture_desc.SampleDesc.Count = 1;
	texture_desc.Usage = D3D11_USAGE_DEFAULT;
	texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	if (FAILED(m_device->CreateTexture2D(&texture_desc, nullptr, &m_texture)))
	{
		Finalize();
		return false;
	}

	if (FAILED(m_device->CreateShaderResourceView(m_texture, nullptr, &m_srv)))
	{
		Finalize();
		return false;
	}

	if (FAILED(m_device->CreateUnorderedAccessView(m_texture, nullptr, &m_uav)))
	{
		Finalize();
		return false;
	}

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.ByteWidth = sizeof(WaterSurfaceHeightConstants);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	if (FAILED(m_device->CreateBuffer(&buffer_desc, nullptr, &m_constant_buffer)))
	{
		Finalize();
		return false;
	}

	return true;
}

void ComputeWaterSurfaceHeightTexture::Finalize()
{
	SafeRelease(m_constant_buffer);
	SafeRelease(m_uav);
	SafeRelease(m_srv);
	SafeRelease(m_texture);
	SafeRelease(m_compute_shader);
	m_device = nullptr;
	m_context = nullptr;
}

void ComputeWaterSurfaceHeightTexture::Update(
	ID3D11ShaderResourceView* terrain_height_srv,
	ID3D11ShaderResourceView* surface_water_srv,
	float water_surface_height) const
{
	if (!IsValid() || terrain_height_srv == nullptr || surface_water_srv == nullptr)
	{
		return;
	}

	const WaterSurfaceHeightConstants constants = {
		water_surface_height,
		0.22f,
		0.82f,
		0.004f,
		kTextureWidth,
		kTextureHeight,
		0u,
		0u
	};

	m_context->UpdateSubresource(m_constant_buffer, 0, nullptr, &constants, 0, 0);

	ID3D11ShaderResourceView* srvs[] = {
		terrain_height_srv,
		surface_water_srv
	};
	ID3D11UnorderedAccessView* uavs[] = { m_uav };
	ID3D11Buffer* constant_buffers[] = { m_constant_buffer };
	ID3D11SamplerState* samplers[] = { Backend::DX11::Sampler::GetState() };

	m_context->CSSetShader(m_compute_shader, nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, constant_buffers);
	m_context->CSSetShaderResources(0, 2, srvs);
	m_context->CSSetSamplers(0, 1, samplers);
	m_context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
	m_context->Dispatch(
		(kTextureWidth + kThreadGroupSize - 1) / kThreadGroupSize,
		(kTextureHeight + kThreadGroupSize - 1) / kThreadGroupSize,
		1);

	ID3D11ShaderResourceView* null_srvs[] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* null_uav = nullptr;
	ID3D11Buffer* null_cb = nullptr;
	ID3D11SamplerState* null_sampler = nullptr;
	m_context->CSSetShaderResources(0, 2, null_srvs);
	m_context->CSSetSamplers(0, 1, &null_sampler);
	m_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
	m_context->CSSetConstantBuffers(0, 1, &null_cb);
	m_context->CSSetShader(nullptr, nullptr, 0);
}

bool ComputeWaterSurfaceHeightTexture::IsValid() const
{
	return m_compute_shader != nullptr &&
		   m_texture != nullptr &&
		   m_srv != nullptr &&
		   m_uav != nullptr &&
		   m_constant_buffer != nullptr;
}

Backend::RenderShaderResource ComputeWaterSurfaceHeightTexture::Resource() const
{
	return Backend::RenderShaderResource(m_srv);
}
