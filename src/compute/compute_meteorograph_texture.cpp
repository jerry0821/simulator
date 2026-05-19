#include "compute_meteorograph_texture.h"

#include <d3d11.h>
#include <fstream>
#include <vector>
#include <cmath>

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

bool ComputeMeteorographTexture::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	Finalize();

	m_device = device;
	m_context = context;
	if (m_device == nullptr || m_context == nullptr)
	{
		return false;
	}

	std::ifstream shader_stream("resource/shader/shader_compute_meteorograph.cso", std::ios::binary);
	if (!shader_stream)
	{
		hal::dout << "ComputeMeteorographTexture::Initialize(): failed to open shader_compute_meteorograph.cso" << std::endl;
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
		hal::dout << "ComputeMeteorographTexture::Initialize(): CreateComputeShader failed" << std::endl;
		Finalize();
		return false;
	}

	D3D11_TEXTURE2D_DESC texture_desc{};
	texture_desc.Width = kTextureWidth;
	texture_desc.Height = kTextureHeight;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	texture_desc.SampleDesc.Count = 1;
	texture_desc.Usage = D3D11_USAGE_DEFAULT;
	texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	for (unsigned int index = 0; index < 2; ++index)
	{
		if (FAILED(m_device->CreateTexture2D(&texture_desc, nullptr, &m_textures[index])) ||
			FAILED(m_device->CreateShaderResourceView(m_textures[index], nullptr, &m_srvs[index])) ||
			FAILED(m_device->CreateUnorderedAccessView(m_textures[index], nullptr, &m_uavs[index])))
		{
			Finalize();
			return false;
		}
	}

	D3D11_TEXTURE2D_DESC rain_desc = texture_desc;
	rain_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	if (FAILED(m_device->CreateTexture2D(&rain_desc, nullptr, &m_rain_texture)) ||
		FAILED(m_device->CreateShaderResourceView(m_rain_texture, nullptr, &m_rain_srv)) ||
		FAILED(m_device->CreateUnorderedAccessView(m_rain_texture, nullptr, &m_rain_uav)))
	{
		Finalize();
		return false;
	}

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.ByteWidth = sizeof(MeteorographConstants);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(m_device->CreateBuffer(&buffer_desc, nullptr, &m_constant_buffer)))
	{
		Finalize();
		return false;
	}

	m_current_index = 0u;
	m_has_state = false;
	return true;
}

void ComputeMeteorographTexture::Finalize()
{
	SafeRelease(m_constant_buffer);
	SafeRelease(m_rain_uav);
	SafeRelease(m_rain_srv);
	SafeRelease(m_rain_texture);
	for (unsigned int index = 0; index < 2; ++index)
	{
		SafeRelease(m_uavs[index]);
		SafeRelease(m_srvs[index]);
		SafeRelease(m_textures[index]);
	}
	SafeRelease(m_compute_shader);
	m_device = nullptr;
	m_context = nullptr;
	m_current_index = 0u;
	m_has_state = false;
}

void ComputeMeteorographTexture::Update(
	float time_seconds,
	float delta_time_seconds,
	const ComputeNoiseSettings& settings,
	Backend::RenderShaderResource terrain_height) const
{
	if (!IsValid() || !terrain_height.isValid())
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
		auto* constants = static_cast<MeteorographConstants*>(mapped_resource.pData);
		*constants = MeteorographConstants{
			time_seconds,
			delta_time_seconds,
			wind_dir_x,
			wind_dir_y,
			settings.wind_strength,
			settings.wind_cross_influence,
			settings.noise_scale,
			0.005f,
			0.18f,
			0.58f,
			0.06f,
			0.10f,
			0.40f,
			settings.rain_multiplier,
			settings.force_rain,
			0.86f,
			kTextureWidth,
			kTextureHeight,
			m_has_state ? 0u : 1u,
			0u };
		m_context->Unmap(m_constant_buffer, 0);
	}

	const unsigned int previous_index = m_current_index;
	const unsigned int next_index = (m_current_index + 1u) % 2u;
	ID3D11ShaderResourceView* input_srvs[2] = {
		m_srvs[previous_index],
		terrain_height.shaderResourceView()};
	ID3D11SamplerState* sampler_state = Backend::DX11::Sampler::GetState();
	ID3D11UnorderedAccessView* output_uavs[2] = {
		m_uavs[next_index],
		m_rain_uav };

	m_context->CSSetShader(m_compute_shader, nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, &m_constant_buffer);
	m_context->CSSetShaderResources(0, 2, input_srvs);
	m_context->CSSetSamplers(0, 1, &sampler_state);
	m_context->CSSetUnorderedAccessViews(0, 2, output_uavs, nullptr);
	m_context->Dispatch(kTextureWidth / kThreadGroupSize, kTextureHeight / kThreadGroupSize, 1);

	ID3D11UnorderedAccessView* null_uavs[2] = { nullptr, nullptr };
	ID3D11Buffer* null_constant_buffer = nullptr;
	ID3D11ShaderResourceView* null_srvs[2] = { nullptr, nullptr };
	ID3D11SamplerState* null_sampler = nullptr;
	m_context->CSSetUnorderedAccessViews(0, 2, null_uavs, nullptr);
	m_context->CSSetShaderResources(0, 2, null_srvs);
	m_context->CSSetSamplers(0, 1, &null_sampler);
	m_context->CSSetConstantBuffers(0, 1, &null_constant_buffer);
	m_context->CSSetShader(nullptr, nullptr, 0);

	m_current_index = next_index;
	m_has_state = true;
}

bool ComputeMeteorographTexture::IsValid() const
{
	return m_compute_shader != nullptr &&
		   m_textures[0] != nullptr &&
		   m_textures[1] != nullptr &&
		   m_srvs[0] != nullptr &&
		   m_srvs[1] != nullptr &&
		   m_uavs[0] != nullptr &&
		   m_uavs[1] != nullptr &&
		   m_rain_texture != nullptr &&
		   m_rain_srv != nullptr &&
		   m_rain_uav != nullptr &&
		   m_constant_buffer != nullptr;
}

Backend::RenderShaderResource ComputeMeteorographTexture::Resource() const
{
	return Backend::RenderShaderResource(m_srvs[m_current_index]);
}

Backend::RenderShaderResource ComputeMeteorographTexture::RainResource() const
{
	return Backend::RenderShaderResource(m_rain_srv);
}
