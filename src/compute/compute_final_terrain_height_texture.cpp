#include "compute_final_terrain_height_texture.h"

#include <algorithm>
#include <cmath>
#include <cstring>
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

bool ComputeFinalTerrainHeightTexture::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	Finalize();

	m_device = device;
	m_context = context;
	if (m_device == nullptr || m_context == nullptr)
	{
		return false;
	}

	std::ifstream shader_stream("resource/shader/shader_compute_final_terrain_height.cso", std::ios::binary);
	if (!shader_stream)
	{
		hal::dout << "ComputeFinalTerrainHeightTexture::Initialize(): failed to open shader_compute_final_terrain_height.cso" << std::endl;
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
		hal::dout << "ComputeFinalTerrainHeightTexture::Initialize(): CreateComputeShader failed" << std::endl;
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

	D3D11_TEXTURE2D_DESC readback_desc = texture_desc;
	readback_desc.Usage = D3D11_USAGE_STAGING;
	readback_desc.BindFlags = 0;
	readback_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	readback_desc.MiscFlags = 0;
	if (FAILED(m_device->CreateTexture2D(&readback_desc, nullptr, &m_readback_texture)))
	{
		Finalize();
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
	srv_desc.Format = texture_desc.Format;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = 1;
	srv_desc.Texture2D.MostDetailedMip = 0;
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
	buffer_desc.ByteWidth = sizeof(FinalTerrainHeightConstants);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	if (FAILED(m_device->CreateBuffer(&buffer_desc, nullptr, &m_constant_buffer)))
	{
		hal::dout << "ComputeFinalTerrainHeightTexture::Initialize(): CreateBuffer(constant) failed" << std::endl;
		Finalize();
		return false;
	}

	return true;
}

void ComputeFinalTerrainHeightTexture::Finalize()
{
	m_cpu_height_data_ready = false;
	m_height_samples.clear();
	SafeRelease(m_constant_buffer);
	SafeRelease(m_uav);
	SafeRelease(m_srv);
	SafeRelease(m_readback_texture);
	SafeRelease(m_texture);
	SafeRelease(m_compute_shader);
	m_device = nullptr;
	m_context = nullptr;
}

void ComputeFinalTerrainHeightTexture::Update(
	ID3D11ShaderResourceView* base_terrain_height_srv,
	ID3D11ShaderResourceView* erosion_delta_srv)
{
	if (!IsValid() || base_terrain_height_srv == nullptr || erosion_delta_srv == nullptr)
	{
		return;
	}

	const FinalTerrainHeightConstants constants = {
		0.18f,
		0.10f,
		-32.0f,
		96.0f,
		-0.80f,
		0.45f,
		kTextureWidth,
		kTextureHeight
	};

	m_context->UpdateSubresource(m_constant_buffer, 0, nullptr, &constants, 0, 0);

	ID3D11ShaderResourceView* srvs[] = { base_terrain_height_srv, erosion_delta_srv };
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

	if (m_readback_texture == nullptr)
	{
		m_cpu_height_data_ready = false;
		return;
	}

	m_context->CopyResource(m_readback_texture, m_texture);

	D3D11_MAPPED_SUBRESOURCE mapped_resource{};
	if (FAILED(m_context->Map(m_readback_texture, 0, D3D11_MAP_READ, 0, &mapped_resource)))
	{
		m_cpu_height_data_ready = false;
		return;
	}

	m_height_samples.assign(static_cast<size_t>(kTextureWidth) * static_cast<size_t>(kTextureHeight), 0.0f);
	for (unsigned int row = 0; row < kTextureHeight; ++row)
	{
		const float* source_row = reinterpret_cast<const float*>(
			static_cast<const unsigned char*>(mapped_resource.pData) + mapped_resource.RowPitch * row);
		std::memcpy(
			m_height_samples.data() + static_cast<size_t>(row) * kTextureWidth,
			source_row,
			sizeof(float) * kTextureWidth);
	}

	m_context->Unmap(m_readback_texture, 0);
	m_cpu_height_data_ready = true;
}

bool ComputeFinalTerrainHeightTexture::IsValid() const
{
	return m_compute_shader != nullptr &&
		   m_texture != nullptr &&
		   m_srv != nullptr &&
		   m_uav != nullptr &&
		   m_constant_buffer != nullptr;
}

Backend::RenderShaderResource ComputeFinalTerrainHeightTexture::Resource() const
{
	return Backend::RenderShaderResource(m_srv);
}

bool ComputeFinalTerrainHeightTexture::HasCpuHeightData() const
{
	return m_cpu_height_data_ready &&
		m_height_samples.size() == static_cast<size_t>(kTextureWidth) * static_cast<size_t>(kTextureHeight);
}

float ComputeFinalTerrainHeightTexture::SampleHeightWorld(
	float world_x,
	float world_z,
	float field_width,
	float field_depth) const
{
	if (!HasCpuHeightData() || field_width <= 0.0f || field_depth <= 0.0f)
	{
		return 0.0f;
	}

	const float u = std::clamp((world_x + field_width * 0.5f) / field_width, 0.0f, 1.0f);
	const float v = std::clamp((world_z + field_depth * 0.5f) / field_depth, 0.0f, 1.0f);
	const float sample_x = u * static_cast<float>(kTextureWidth - 1);
	const float sample_y = v * static_cast<float>(kTextureHeight - 1);
	const unsigned int x0 = static_cast<unsigned int>(std::floor(sample_x));
	const unsigned int y0 = static_cast<unsigned int>(std::floor(sample_y));
	const unsigned int x1 = std::min(x0 + 1, kTextureWidth - 1);
	const unsigned int y1 = std::min(y0 + 1, kTextureHeight - 1);
	const float tx = sample_x - static_cast<float>(x0);
	const float ty = sample_y - static_cast<float>(y0);

	const auto sample = [this](unsigned int x, unsigned int y)
	{
		return m_height_samples[static_cast<size_t>(x) + static_cast<size_t>(y) * kTextureWidth];
	};

	const float h00 = sample(x0, y0);
	const float h10 = sample(x1, y0);
	const float h01 = sample(x0, y1);
	const float h11 = sample(x1, y1);
	const float hx0 = std::lerp(h00, h10, tx);
	const float hx1 = std::lerp(h01, h11, tx);
	return std::lerp(hx0, hx1, ty);
}
