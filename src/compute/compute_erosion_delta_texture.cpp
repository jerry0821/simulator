#include "compute_erosion_delta_texture.h"

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

bool ComputeErosionDeltaTexture::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	Finalize();

	m_device = device;
	m_context = context;
	if (m_device == nullptr || m_context == nullptr)
	{
		return false;
	}

	std::ifstream shader_stream("resource/shader/shader_compute_erosion_delta.cso", std::ios::binary);
	if (!shader_stream)
	{
		hal::dout << "ComputeErosionDeltaTexture::Initialize(): failed to open shader_compute_erosion_delta.cso" << std::endl;
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
		hal::dout << "ComputeErosionDeltaTexture::Initialize(): CreateComputeShader failed" << std::endl;
		Finalize();
		return false;
	}

	std::vector<float> zero_pixels(kTextureWidth * kTextureHeight * 4u, 0.0f);
	D3D11_SUBRESOURCE_DATA init_data{};
	init_data.pSysMem = zero_pixels.data();
	init_data.SysMemPitch = sizeof(float) * 4u * kTextureWidth;

	for (int i = 0; i < 2; ++i)
	{
		D3D11_TEXTURE2D_DESC texture_desc{};
		texture_desc.Width = kTextureWidth;
		texture_desc.Height = kTextureHeight;
		texture_desc.MipLevels = 1;
		texture_desc.ArraySize = 1;
		texture_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		texture_desc.SampleDesc.Count = 1;
		texture_desc.Usage = D3D11_USAGE_DEFAULT;
		texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

		if (FAILED(m_device->CreateTexture2D(&texture_desc, &init_data, &m_textures[i])))
		{
			Finalize();
			return false;
		}

		if (FAILED(m_device->CreateShaderResourceView(m_textures[i], nullptr, &m_srvs[i])))
		{
			Finalize();
			return false;
		}

		if (FAILED(m_device->CreateUnorderedAccessView(m_textures[i], nullptr, &m_uavs[i])))
		{
			Finalize();
			return false;
		}
	}

	D3D11_TEXTURE2D_DESC output_desc{};
	output_desc.Width = kTextureWidth;
	output_desc.Height = kTextureHeight;
	output_desc.MipLevels = 1;
	output_desc.ArraySize = 1;
	output_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	output_desc.SampleDesc.Count = 1;
	output_desc.Usage = D3D11_USAGE_DEFAULT;
	output_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(m_device->CreateTexture2D(&output_desc, &init_data, &m_output_texture)))
	{
		Finalize();
		return false;
	}

	if (FAILED(m_device->CreateShaderResourceView(m_output_texture, nullptr, &m_output_srv)))
	{
		Finalize();
		return false;
	}

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.ByteWidth = sizeof(ErosionDeltaConstants);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	if (FAILED(m_device->CreateBuffer(&buffer_desc, nullptr, &m_constant_buffer)))
	{
		hal::dout << "ComputeErosionDeltaTexture::Initialize(): CreateBuffer(constant) failed" << std::endl;
		Finalize();
		return false;
	}

	m_current_index = 0;
	return true;
}

void ComputeErosionDeltaTexture::Finalize()
{
	SafeRelease(m_constant_buffer);
	SafeRelease(m_output_srv);
	SafeRelease(m_output_texture);
	for (int i = 0; i < 2; ++i)
	{
		SafeRelease(m_uavs[i]);
		SafeRelease(m_srvs[i]);
		SafeRelease(m_textures[i]);
	}
	SafeRelease(m_compute_shader);
	m_device = nullptr;
	m_context = nullptr;
	m_current_index = 0;
}

void ComputeErosionDeltaTexture::Update(
	ID3D11ShaderResourceView* terrain_height_srv,
	ID3D11ShaderResourceView* surface_water_srv,
	ID3D11ShaderResourceView* surface_water_flow_srv)
{
	if (!IsValid() || terrain_height_srv == nullptr || surface_water_srv == nullptr || surface_water_flow_srv == nullptr)
	{
		return;
	}

	const unsigned int next_index = (m_current_index + 1u) % 2u;
	const ErosionDeltaConstants constants = {
		0.012f,
		0.008f,
		0.12f,
		0.010f,
		0.45f,
		0.25f,
		kTextureWidth,
		kTextureHeight
	};

	m_context->UpdateSubresource(m_constant_buffer, 0, nullptr, &constants, 0, 0);

	ID3D11ShaderResourceView* srvs[] = {
		terrain_height_srv,
		surface_water_srv,
		surface_water_flow_srv,
		m_srvs[m_current_index]
	};
	ID3D11UnorderedAccessView* uavs[] = { m_uavs[next_index] };
	ID3D11Buffer* constant_buffers[] = { m_constant_buffer };
	ID3D11SamplerState* samplers[] = { Backend::DX11::Sampler::GetState() };

	m_context->CSSetShader(m_compute_shader, nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, constant_buffers);
	m_context->CSSetShaderResources(0, 4, srvs);
	m_context->CSSetSamplers(0, 1, samplers);
	m_context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
	m_context->Dispatch(
		(kTextureWidth + kThreadGroupSize - 1) / kThreadGroupSize,
		(kTextureHeight + kThreadGroupSize - 1) / kThreadGroupSize,
		1);

	ID3D11ShaderResourceView* null_srvs[] = { nullptr, nullptr, nullptr, nullptr };
	ID3D11UnorderedAccessView* null_uav = nullptr;
	ID3D11Buffer* null_cb = nullptr;
	ID3D11SamplerState* null_sampler = nullptr;
	m_context->CSSetShaderResources(0, 4, null_srvs);
	m_context->CSSetSamplers(0, 1, &null_sampler);
	m_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
	m_context->CSSetConstantBuffers(0, 1, &null_cb);
	m_context->CSSetShader(nullptr, nullptr, 0);
	m_context->CopyResource(m_output_texture, m_textures[next_index]);

	m_current_index = next_index;
}

bool ComputeErosionDeltaTexture::IsValid() const
{
	return m_compute_shader != nullptr &&
		   m_textures[0] != nullptr &&
		   m_textures[1] != nullptr &&
		   m_srvs[0] != nullptr &&
		   m_srvs[1] != nullptr &&
		   m_uavs[0] != nullptr &&
		   m_uavs[1] != nullptr &&
		   m_output_texture != nullptr &&
		   m_output_srv != nullptr &&
		   m_constant_buffer != nullptr;
}

Backend::RenderShaderResource ComputeErosionDeltaTexture::Resource() const
{
	return Backend::RenderShaderResource(m_output_srv);
}
