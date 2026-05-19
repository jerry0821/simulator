#include "compute_wind_field_texture.h"

#include <fstream>
#include <vector>
#include <cmath>

#include "debug_ostream.h"
#include "direct3d.h"
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

bool ComputeWindFieldTexture::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	Finalize();

	m_device = device;
	m_context = context;
	if (m_device == nullptr || m_context == nullptr)
	{
		return false;
	}

	std::ifstream shader_stream("resource/shader/shader_compute_wind_field.cso", std::ios::binary);
	if (!shader_stream)
	{
		hal::dout << "ComputeWindFieldTexture::Initialize(): failed to open shader_compute_wind_field.cso" << std::endl;
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
		hal::dout << "ComputeWindFieldTexture::Initialize(): CreateComputeShader failed" << std::endl;
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

	for (int i = 0; i < 2; ++i)
	{
		if (FAILED(m_device->CreateTexture2D(&texture_desc, nullptr, &m_textures[i])))
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

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.ByteWidth = sizeof(WindConstants);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(m_device->CreateBuffer(&buffer_desc, nullptr, &m_constant_buffer)))
	{
		Finalize();
		return false;
	}

	m_current_index = 0;
	m_has_state = false;
	return true;
}

void ComputeWindFieldTexture::Finalize()
{
	SafeRelease(m_constant_buffer);
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
	m_has_state = false;
}

void ComputeWindFieldTexture::Update(
	float time_seconds,
	float delta_time_seconds,
	const ComputeNoiseSettings& settings,
	ID3D11ShaderResourceView* climate_field_srv,
	ID3D11ShaderResourceView* terrain_height_srv) const
{
	if (!IsValid() || climate_field_srv == nullptr || terrain_height_srv == nullptr)
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

	D3D11_MAPPED_SUBRESOURCE mapped_resource{};
	if (SUCCEEDED(m_context->Map(m_constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource)))
	{
		auto* constants = static_cast<WindConstants*>(mapped_resource.pData);
		*constants = WindConstants{
			time_seconds,
			delta_time_seconds,
			wind_dir_x,
			wind_dir_y,
			settings.wind_strength,
			settings.wind_cross_influence,
			settings.noise_scale,
			0.085f,
			0.42f,
			0.72f,
			0.20f,
			0.28f,
			kTextureWidth,
			kTextureHeight,
			m_has_state ? 0u : 1u,
			0u };
		m_context->Unmap(m_constant_buffer, 0);
	}

	const unsigned int previous_index = m_current_index;
	const unsigned int next_index = (m_current_index + 1u) % 2u;

	m_context->CSSetShader(m_compute_shader, nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, &m_constant_buffer);
	ID3D11ShaderResourceView* srvs[] = {
		m_srvs[previous_index],
		climate_field_srv,
		terrain_height_srv
	};
	ID3D11SamplerState* samplers[] = { Backend::DX11::Sampler::GetState() };
	m_context->CSSetShaderResources(0, 3, srvs);
	m_context->CSSetSamplers(0, 1, samplers);
	m_context->CSSetUnorderedAccessViews(0, 1, &m_uavs[next_index], nullptr);
	m_context->Dispatch(kTextureWidth / kThreadGroupSize, kTextureHeight / kThreadGroupSize, 1);

	ID3D11UnorderedAccessView* null_uav = nullptr;
	ID3D11Buffer* null_constant_buffer = nullptr;
	ID3D11ShaderResourceView* null_srvs[3] = { nullptr, nullptr, nullptr };
	ID3D11SamplerState* null_sampler = nullptr;
	m_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
	m_context->CSSetConstantBuffers(0, 1, &null_constant_buffer);
	m_context->CSSetShaderResources(0, 3, null_srvs);
	m_context->CSSetSamplers(0, 1, &null_sampler);
	m_context->CSSetShader(nullptr, nullptr, 0);

	m_current_index = next_index;
	m_has_state = true;
}

bool ComputeWindFieldTexture::IsValid() const
{
	return m_compute_shader != nullptr &&
		   m_textures[0] != nullptr &&
		   m_textures[1] != nullptr &&
		   m_srvs[0] != nullptr &&
		   m_srvs[1] != nullptr &&
		   m_uavs[0] != nullptr &&
		   m_uavs[1] != nullptr &&
		   m_constant_buffer != nullptr;
}

Backend::RenderShaderResource ComputeWindFieldTexture::Resource() const
{
	return Backend::RenderShaderResource(m_srvs[m_current_index]);
}
