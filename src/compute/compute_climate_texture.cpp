#include "compute_climate_texture.h"

#include <d3d11.h>
#include <fstream>
#include <vector>

#include "debug_ostream.h"

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

bool ComputeClimateTexture::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	Finalize();

	m_device = device;
	m_context = context;
	if (m_device == nullptr || m_context == nullptr)
	{
		return false;
	}

	std::ifstream shader_stream("resource/shader/shader_compute_climate_field.cso", std::ios::binary);
	if (!shader_stream)
	{
		hal::dout << "ComputeClimateTexture::Initialize(): failed to open shader_compute_climate_field.cso" << std::endl;
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
		hal::dout << "ComputeClimateTexture::Initialize(): CreateComputeShader failed" << std::endl;
		Finalize();
		return false;
	}

	D3D11_TEXTURE2D_DESC texture_desc{};
	texture_desc.Width = kTextureWidth;
	texture_desc.Height = kTextureHeight;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.ByteWidth = sizeof(ClimateConstants);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(m_device->CreateBuffer(&buffer_desc, nullptr, &m_constant_buffer)))
	{
		Finalize();
		return false;
	}

	return true;
}

void ComputeClimateTexture::Finalize()
{
	SafeRelease(m_constant_buffer);
	SafeRelease(m_uav);
	SafeRelease(m_srv);
	SafeRelease(m_texture);
	SafeRelease(m_compute_shader);
	m_device = nullptr;
	m_context = nullptr;
}

void ComputeClimateTexture::Update(float time_seconds) const
{
	if (!IsValid())
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE mapped_resource{};
	if (SUCCEEDED(m_context->Map(m_constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource)))
	{
		auto* constants = static_cast<ClimateConstants*>(mapped_resource.pData);
		*constants = ClimateConstants{
			time_seconds,
			4.5f,
			5.5f,
			7.0f,
			0.50f,
			0.58f,
			0.18f,
			0.14f,
			kTextureWidth,
			kTextureHeight,
			0u,
			0u };
		m_context->Unmap(m_constant_buffer, 0);
	}

	m_context->CSSetShader(m_compute_shader, nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, &m_constant_buffer);
	m_context->CSSetUnorderedAccessViews(0, 1, &m_uav, nullptr);
	m_context->Dispatch(kTextureWidth / kThreadGroupSize, kTextureHeight / kThreadGroupSize, 1);

	ID3D11UnorderedAccessView* null_uav = nullptr;
	ID3D11Buffer* null_constant_buffer = nullptr;
	m_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
	m_context->CSSetConstantBuffers(0, 1, &null_constant_buffer);
	m_context->CSSetShader(nullptr, nullptr, 0);
}

bool ComputeClimateTexture::IsValid() const
{
	return m_compute_shader != nullptr &&
		   m_texture != nullptr &&
		   m_srv != nullptr &&
		   m_uav != nullptr &&
		   m_constant_buffer != nullptr;
}

Backend::RenderShaderResource ComputeClimateTexture::Resource() const
{
	return Backend::RenderShaderResource(m_srv);
}
