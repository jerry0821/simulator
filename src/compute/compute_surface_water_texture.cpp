#include "compute_surface_water_texture.h"

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

bool CreateFloat4Texture(
	ID3D11Device* device,
	unsigned int width,
	unsigned int height,
	ID3D11Texture2D** texture,
	ID3D11ShaderResourceView** srv,
	ID3D11UnorderedAccessView** uav)
{
	if (device == nullptr || texture == nullptr || srv == nullptr || uav == nullptr)
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC texture_desc{};
	texture_desc.Width = width;
	texture_desc.Height = height;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	texture_desc.SampleDesc.Count = 1;
	texture_desc.Usage = D3D11_USAGE_DEFAULT;
	texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	if (FAILED(device->CreateTexture2D(&texture_desc, nullptr, texture)))
	{
		return false;
	}

	if (FAILED(device->CreateShaderResourceView(*texture, nullptr, srv)))
	{
		SafeRelease(*texture);
		return false;
	}

	if (FAILED(device->CreateUnorderedAccessView(*texture, nullptr, uav)))
	{
		SafeRelease(*srv);
		SafeRelease(*texture);
		return false;
	}

	return true;
}
}

bool ComputeSurfaceWaterTexture::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	Finalize();

	m_device = device;
	m_context = context;
	if (m_device == nullptr || m_context == nullptr)
	{
		return false;
	}

	std::ifstream shader_stream("resource/shader/shader_compute_surface_water.cso", std::ios::binary);
	if (!shader_stream)
	{
		hal::dout << "ComputeSurfaceWaterTexture::Initialize(): failed to open shader_compute_surface_water.cso" << std::endl;
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
		hal::dout << "ComputeSurfaceWaterTexture::Initialize(): CreateComputeShader failed" << std::endl;
		Finalize();
		return false;
	}

	if (!CreateFloat4Texture(
			m_device,
			kTextureWidth,
			kTextureHeight,
			&m_surface_texture,
			&m_surface_srv,
			&m_surface_uav) ||
		!CreateFloat4Texture(
			m_device,
			kTextureWidth,
			kTextureHeight,
			&m_flow_texture,
			&m_flow_srv,
			&m_flow_uav) ||
		!CreateFloat4Texture(
			m_device,
			kTextureWidth,
			kTextureHeight,
			&m_flow_preview_texture,
			&m_flow_preview_srv,
			&m_flow_preview_uav))
	{
		Finalize();
		return false;
	}

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.ByteWidth = sizeof(SurfaceWaterConstants);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	if (FAILED(m_device->CreateBuffer(&buffer_desc, nullptr, &m_constant_buffer)))
	{
		hal::dout << "ComputeSurfaceWaterTexture::Initialize(): CreateBuffer(constant) failed" << std::endl;
		Finalize();
		return false;
	}

	return true;
}

void ComputeSurfaceWaterTexture::Finalize()
{
	SafeRelease(m_constant_buffer);
	SafeRelease(m_flow_preview_uav);
	SafeRelease(m_flow_preview_srv);
	SafeRelease(m_flow_preview_texture);
	SafeRelease(m_flow_uav);
	SafeRelease(m_flow_srv);
	SafeRelease(m_flow_texture);
	SafeRelease(m_surface_uav);
	SafeRelease(m_surface_srv);
	SafeRelease(m_surface_texture);
	SafeRelease(m_compute_shader);
	m_device = nullptr;
	m_context = nullptr;
}

void ComputeSurfaceWaterTexture::ClearState() const
{
	if (!IsValid())
	{
		return;
	}

	const float clear_values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_context->ClearUnorderedAccessViewFloat(m_surface_uav, clear_values);
	m_context->ClearUnorderedAccessViewFloat(m_flow_uav, clear_values);
	m_context->ClearUnorderedAccessViewFloat(m_flow_preview_uav, clear_values);
}

void ComputeSurfaceWaterTexture::Update(
	ID3D11ShaderResourceView* water_surface_height_srv,
	ID3D11ShaderResourceView* authoritative_flow_srv,
	ID3D11ShaderResourceView* authoritative_velocity_srv,
	ID3D11ShaderResourceView* authoritative_sediment_srv,
	ID3D11ShaderResourceView* wind_field_srv,
	float time_seconds)
{
	if (!IsValid() ||
		water_surface_height_srv == nullptr ||
		authoritative_flow_srv == nullptr ||
		authoritative_velocity_srv == nullptr ||
		authoritative_sediment_srv == nullptr ||
		wind_field_srv == nullptr)
	{
		return;
	}

	const SurfaceWaterConstants constants = {
		time_seconds,
		512.0f,
		512.0f,
		kTextureWidth,
		kTextureHeight,
		0u,
		0u,
		0u
	};

	m_context->UpdateSubresource(m_constant_buffer, 0, nullptr, &constants, 0, 0);

	ID3D11ShaderResourceView* srvs[] = {
		water_surface_height_srv,
		authoritative_flow_srv,
		authoritative_velocity_srv,
		authoritative_sediment_srv,
		wind_field_srv
	};
	ID3D11UnorderedAccessView* uavs[] = {
		m_surface_uav,
		m_flow_uav,
		m_flow_preview_uav
	};
	ID3D11Buffer* constant_buffers[] = { m_constant_buffer };
	ID3D11SamplerState* samplers[] = { Backend::DX11::Sampler::GetState() };

	m_context->CSSetShader(m_compute_shader, nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, constant_buffers);
	m_context->CSSetShaderResources(0, 5, srvs);
	m_context->CSSetSamplers(0, 1, samplers);
	m_context->CSSetUnorderedAccessViews(0, 3, uavs, nullptr);
	m_context->Dispatch(
		(kTextureWidth + kThreadGroupSize - 1) / kThreadGroupSize,
		(kTextureHeight + kThreadGroupSize - 1) / kThreadGroupSize,
		1);

	ID3D11ShaderResourceView* null_srvs[] = { nullptr, nullptr, nullptr, nullptr, nullptr };
	ID3D11UnorderedAccessView* null_uavs[] = { nullptr, nullptr, nullptr };
	ID3D11Buffer* null_cb = nullptr;
	ID3D11SamplerState* null_sampler = nullptr;
	m_context->CSSetShaderResources(0, 5, null_srvs);
	m_context->CSSetSamplers(0, 1, &null_sampler);
	m_context->CSSetUnorderedAccessViews(0, 3, null_uavs, nullptr);
	m_context->CSSetConstantBuffers(0, 1, &null_cb);
	m_context->CSSetShader(nullptr, nullptr, 0);
}

bool ComputeSurfaceWaterTexture::IsValid() const
{
	return m_compute_shader != nullptr &&
		   m_surface_texture != nullptr &&
		   m_surface_srv != nullptr &&
		   m_surface_uav != nullptr &&
		   m_flow_texture != nullptr &&
		   m_flow_srv != nullptr &&
		   m_flow_uav != nullptr &&
		   m_flow_preview_texture != nullptr &&
		   m_flow_preview_srv != nullptr &&
		   m_flow_preview_uav != nullptr &&
		   m_constant_buffer != nullptr;
}

Backend::RenderShaderResource ComputeSurfaceWaterTexture::Resource() const
{
	return Backend::RenderShaderResource(m_surface_srv);
}

Backend::RenderShaderResource ComputeSurfaceWaterTexture::FlowResource() const
{
	return Backend::RenderShaderResource(m_flow_srv);
}

Backend::RenderShaderResource ComputeSurfaceWaterTexture::FlowPreviewResource() const
{
	return Backend::RenderShaderResource(m_flow_preview_srv);
}
