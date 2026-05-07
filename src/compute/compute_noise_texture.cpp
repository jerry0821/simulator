#include "compute_noise_texture.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>

#include <d3d11.h>

#include "debug_ostream.h"
#include "direct3d.h"

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

bool ComputeNoiseTexture::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	m_device = device;
	m_context = context;

	if (m_device == nullptr || m_context == nullptr)
	{
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
		hal::dout << "ComputeNoiseTexture::Initialize(): failed to create texture" << std::endl;
		return false;
	}

	if (FAILED(m_device->CreateShaderResourceView(m_texture, nullptr, &m_srv)))
	{
		hal::dout << "ComputeNoiseTexture::Initialize(): failed to create SRV" << std::endl;
		return false;
	}

	if (FAILED(m_device->CreateUnorderedAccessView(m_texture, nullptr, &m_uav)))
	{
		hal::dout << "ComputeNoiseTexture::Initialize(): failed to create UAV" << std::endl;
		return false;
	}

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(NoiseConstants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	if (FAILED(m_device->CreateBuffer(&buffer_desc, nullptr, &m_constant_buffer)))
	{
		hal::dout << "ComputeNoiseTexture::Initialize(): failed to create constant buffer" << std::endl;
		return false;
	}

	std::ifstream shader_stream("resource/shader/shader_compute_noise.cso", std::ios::binary);
	if (!shader_stream)
	{
		hal::dout << "ComputeNoiseTexture::Initialize(): failed to open shader_compute_noise.cso" << std::endl;
		return false;
	}

	shader_stream.seekg(0, std::ios::end);
	const std::streamsize shader_size = shader_stream.tellg();
	shader_stream.seekg(0, std::ios::beg);

	std::vector<char> shader_data(static_cast<size_t>(shader_size));
	shader_stream.read(shader_data.data(), shader_size);

	if (FAILED(m_device->CreateComputeShader(
		shader_data.data(),
		shader_data.size(),
		nullptr,
		&m_compute_shader)))
	{
		hal::dout << "ComputeNoiseTexture::Initialize(): failed to create compute shader" << std::endl;
		return false;
	}

	return true;
}

void ComputeNoiseTexture::Finalize()
{
	SafeRelease(m_constant_buffer);
	SafeRelease(m_uav);
	SafeRelease(m_srv);
	SafeRelease(m_texture);
	SafeRelease(m_compute_shader);

	m_context = nullptr;
	m_device = nullptr;
}

void ComputeNoiseTexture::Update(float time_seconds, const ComputeNoiseSettings& settings) const
{
	if (!IsValid())
	{
		return;
	}

	float wind_dir_x = settings.wind_direction_x;
	float wind_dir_y = settings.wind_direction_y;
	const float wind_length = std::sqrt(wind_dir_x * wind_dir_x + wind_dir_y * wind_dir_y);
	if (wind_length > 0.0001f)
	{
		wind_dir_x /= wind_length;
		wind_dir_y /= wind_length;
	}
	else
	{
		wind_dir_x = 1.0f;
		wind_dir_y = 0.0f;
	}

	const float cross_dir_x = -wind_dir_y;
	const float cross_dir_y = wind_dir_x;
	const float primary_wind_speed = settings.wind_strength;
	const float secondary_wind_speed = settings.wind_strength * settings.wind_cross_influence;

	const NoiseConstants constants = {
		time_seconds,
		settings.noise_scale,
		wind_dir_x * primary_wind_speed + settings.flow_speed_x0,
		wind_dir_y * primary_wind_speed + settings.flow_speed_y0,
		cross_dir_x * secondary_wind_speed + settings.flow_speed_x1,
		cross_dir_y * secondary_wind_speed + settings.flow_speed_y1,
		settings.band_strength,
		settings.contrast,
		kTextureWidth,
		kTextureHeight
	};

	m_context->UpdateSubresource(m_constant_buffer, 0, nullptr, &constants, 0, 0);

	ID3D11UnorderedAccessView* uavs[] = { m_uav };
	ID3D11Buffer* constant_buffers[] = { m_constant_buffer };

	m_context->CSSetShader(m_compute_shader, nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, constant_buffers);
	m_context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

	const unsigned int dispatch_x = (kTextureWidth + kThreadGroupSize - 1) / kThreadGroupSize;
	const unsigned int dispatch_y = (kTextureHeight + kThreadGroupSize - 1) / kThreadGroupSize;
	m_context->Dispatch(dispatch_x, dispatch_y, 1);

	ID3D11UnorderedAccessView* null_uav[] = { nullptr };
	ID3D11Buffer* null_constant_buffer[] = { nullptr };
	m_context->CSSetUnorderedAccessViews(0, 1, null_uav, nullptr);
	m_context->CSSetConstantBuffers(0, 1, null_constant_buffer);
	m_context->CSSetShader(nullptr, nullptr, 0);
}

bool ComputeNoiseTexture::IsValid() const
{
	return m_compute_shader != nullptr && m_srv != nullptr && m_uav != nullptr;
}

Backend::RenderShaderResource ComputeNoiseTexture::Resource() const
{
	return Backend::RenderShaderResource(m_srv);
}
