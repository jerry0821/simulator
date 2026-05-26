#include "compute_grass_data_texture.h"

#include <d3d11.h>
#include <fstream>
#include <vector>

#include "debug_ostream.h"
#include "sampler.h"
#include "terrain_height_field.h"

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

bool ComputeGrassDataTexture::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	Finalize();

	m_device = device;
	m_context = context;
	if (m_device == nullptr || m_context == nullptr)
	{
		return false;
	}

	std::ifstream shader_stream("resource/shader/shader_compute_grass_data.cso", std::ios::binary);
	if (!shader_stream)
	{
		hal::dout << "ComputeGrassDataTexture::Initialize(): failed to open shader_compute_grass_data.cso" << std::endl;
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
		hal::dout << "ComputeGrassDataTexture::Initialize(): CreateComputeShader failed" << std::endl;
		Finalize();
		return false;
	}

	D3D11_TEXTURE2D_DESC texture_desc{};
	texture_desc.Width = kTextureWidth;
	texture_desc.Height = kTextureHeight;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	texture_desc.SampleDesc.Count = 1;
	texture_desc.Usage = D3D11_USAGE_DEFAULT;
	texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	if (FAILED(m_device->CreateTexture2D(&texture_desc, nullptr, &m_texture)))
	{
		Finalize();
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
	srv_desc.Format = texture_desc.Format;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = 1;
	if (FAILED(m_device->CreateShaderResourceView(m_texture, &srv_desc, &m_srv)))
	{
		Finalize();
		return false;
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
	uav_desc.Format = texture_desc.Format;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	if (FAILED(m_device->CreateUnorderedAccessView(m_texture, &uav_desc, &m_uav)))
	{
		Finalize();
		return false;
	}

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.ByteWidth = sizeof(GrassDataConstants);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (FAILED(m_device->CreateBuffer(&buffer_desc, nullptr, &m_constant_buffer)))
	{
		hal::dout << "ComputeGrassDataTexture::Initialize(): CreateBuffer(constant) failed" << std::endl;
		Finalize();
		return false;
	}

	return true;
}

void ComputeGrassDataTexture::Finalize()
{
	SafeRelease(m_constant_buffer);
	SafeRelease(m_uav);
	SafeRelease(m_srv);
	SafeRelease(m_texture);
	SafeRelease(m_compute_shader);
	m_device = nullptr;
	m_context = nullptr;
}

void ComputeGrassDataTexture::Update(
	ID3D11ShaderResourceView* terrain_normal_srv,
	ID3D11ShaderResourceView* terrain_height_srv,
	ID3D11ShaderResourceView* terrain_vegetation_suitability_srv)
{
	if (!IsValid() ||
		terrain_normal_srv == nullptr ||
		terrain_height_srv == nullptr ||
		terrain_vegetation_suitability_srv == nullptr)
	{
		return;
	}

	const GrassDataConstants constants = {
		TerrainHeightField::FieldWidth(),
		TerrainHeightField::FieldDepth(),
		kTextureWidth,
		kTextureHeight };
	m_context->UpdateSubresource(m_constant_buffer, 0, nullptr, &constants, 0, 0);

	ID3D11ShaderResourceView* srvs[] = {
		terrain_normal_srv,
		terrain_height_srv,
		terrain_vegetation_suitability_srv
	};
	ID3D11UnorderedAccessView* uavs[] = { m_uav };
	ID3D11Buffer* constant_buffers[] = { m_constant_buffer };
	ID3D11SamplerState* samplers[] = { Backend::DX11::Sampler::GetState() };

	m_context->CSSetShader(m_compute_shader, nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, constant_buffers);
	m_context->CSSetShaderResources(0, 3, srvs);
	m_context->CSSetSamplers(0, 1, samplers);
	m_context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
	m_context->Dispatch(
		(kTextureWidth + kThreadGroupSize - 1) / kThreadGroupSize,
		(kTextureHeight + kThreadGroupSize - 1) / kThreadGroupSize,
		1);

	ID3D11ShaderResourceView* null_srvs[] = { nullptr, nullptr, nullptr };
	ID3D11UnorderedAccessView* null_uavs[] = { nullptr };
	ID3D11Buffer* null_cb = nullptr;
	ID3D11SamplerState* null_sampler = nullptr;
	m_context->CSSetShaderResources(0, 3, null_srvs);
	m_context->CSSetSamplers(0, 1, &null_sampler);
	m_context->CSSetUnorderedAccessViews(0, 1, null_uavs, nullptr);
	m_context->CSSetConstantBuffers(0, 1, &null_cb);
	m_context->CSSetShader(nullptr, nullptr, 0);
}

bool ComputeGrassDataTexture::IsValid() const
{
	return m_compute_shader != nullptr &&
		m_texture != nullptr &&
		m_srv != nullptr &&
		m_uav != nullptr &&
		m_constant_buffer != nullptr;
}

Backend::RenderShaderResource ComputeGrassDataTexture::Resource() const
{
	return Backend::RenderShaderResource(m_srv);
}
