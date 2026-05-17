#include "compute_surface_water_texture.h"

#include <d3d11.h>
#include <fstream>
#include <vector>

#include "debug_menu.h"
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

	D3D11_TEXTURE2D_DESC flow_desc{};
	flow_desc.Width = kTextureWidth;
	flow_desc.Height = kTextureHeight;
	flow_desc.MipLevels = 1;
	flow_desc.ArraySize = 1;
	flow_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	flow_desc.SampleDesc.Count = 1;
	flow_desc.Usage = D3D11_USAGE_DEFAULT;
	flow_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	if (FAILED(m_device->CreateTexture2D(&flow_desc, &init_data, &m_flow_texture)))
	{
		Finalize();
		return false;
	}

	if (FAILED(m_device->CreateShaderResourceView(m_flow_texture, nullptr, &m_flow_srv)))
	{
		Finalize();
		return false;
	}

	if (FAILED(m_device->CreateUnorderedAccessView(m_flow_texture, nullptr, &m_flow_uav)))
	{
		Finalize();
		return false;
	}

	if (FAILED(m_device->CreateTexture2D(&flow_desc, &init_data, &m_flow_preview_texture)))
	{
		Finalize();
		return false;
	}

	if (FAILED(m_device->CreateShaderResourceView(m_flow_preview_texture, nullptr, &m_flow_preview_srv)))
	{
		Finalize();
		return false;
	}

	if (FAILED(m_device->CreateUnorderedAccessView(m_flow_preview_texture, nullptr, &m_flow_preview_uav)))
	{
		Finalize();
		return false;
	}

	if (FAILED(m_device->CreateTexture2D(&flow_desc, &init_data, &m_velocity_texture)))
	{
		Finalize();
		return false;
	}

	if (FAILED(m_device->CreateShaderResourceView(m_velocity_texture, nullptr, &m_velocity_srv)))
	{
		Finalize();
		return false;
	}

	if (FAILED(m_device->CreateUnorderedAccessView(m_velocity_texture, nullptr, &m_velocity_uav)))
	{
		Finalize();
		return false;
	}

	if (FAILED(m_device->CreateTexture2D(&flow_desc, &init_data, &m_sediment_texture)))
	{
		Finalize();
		return false;
	}

	if (FAILED(m_device->CreateShaderResourceView(m_sediment_texture, nullptr, &m_sediment_srv)))
	{
		Finalize();
		return false;
	}

	if (FAILED(m_device->CreateUnorderedAccessView(m_sediment_texture, nullptr, &m_sediment_uav)))
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

	m_current_index = 0;
	return true;
}

void ComputeSurfaceWaterTexture::Finalize()
{
	SafeRelease(m_constant_buffer);
	SafeRelease(m_flow_preview_uav);
	SafeRelease(m_flow_preview_srv);
	SafeRelease(m_flow_preview_texture);
	SafeRelease(m_velocity_uav);
	SafeRelease(m_velocity_srv);
	SafeRelease(m_velocity_texture);
	SafeRelease(m_sediment_uav);
	SafeRelease(m_sediment_srv);
	SafeRelease(m_sediment_texture);
	SafeRelease(m_flow_uav);
	SafeRelease(m_flow_srv);
	SafeRelease(m_flow_texture);
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

void ComputeSurfaceWaterTexture::ClearState() const
{
	if (!IsValid())
	{
		return;
	}

	const float clear_values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	for (int i = 0; i < 2; ++i)
	{
		m_context->ClearUnorderedAccessViewFloat(m_uavs[i], clear_values);
	}
	m_context->ClearUnorderedAccessViewFloat(m_flow_uav, clear_values);
	m_context->ClearUnorderedAccessViewFloat(m_flow_preview_uav, clear_values);
	m_context->ClearUnorderedAccessViewFloat(m_velocity_uav, clear_values);
	m_context->ClearUnorderedAccessViewFloat(m_sediment_uav, clear_values);
}

void ComputeSurfaceWaterTexture::Update(
	ID3D11ShaderResourceView* terrain_height_srv,
	ID3D11ShaderResourceView* rain_map_srv,
	ID3D11ShaderResourceView* wind_field_srv,
	float water_height,
	const SurfaceWaterSimulationSettings& settings,
	bool inject_water_pulse,
	float time_seconds)
{
	if (!IsValid() || terrain_height_srv == nullptr || rain_map_srv == nullptr || wind_field_srv == nullptr)
	{
		return;
	}

	const unsigned int next_index = (m_current_index + 1u) % 2u;
	const SurfaceWaterConstants constants = {
		water_height,
		settings.accumulation_rate,
		settings.evaporation_rate,
		settings.seepage_rate,
		settings.basin_fade,
		settings.downhill_flow_rate,
		settings.flow_damping,
		settings.max_outflow_fraction,
		512.0f,
		512.0f,
		settings.debug_injection_x,
		settings.debug_injection_z,
		settings.debug_injection_radius,
		settings.debug_injection_amount,
		time_seconds,
		kTextureWidth,
		kTextureHeight,
		settings.presentation_only ? 1u : 0u,
		inject_water_pulse ? 1u : 0u,
		0u,
		0u,
		0u,
		0u
	};

	m_context->UpdateSubresource(m_constant_buffer, 0, nullptr, &constants, 0, 0);

	ID3D11ShaderResourceView* srvs[] = {
		terrain_height_srv,
		rain_map_srv,
		wind_field_srv
	};
	ID3D11UnorderedAccessView* uavs[] = {
		m_uavs[next_index],
		m_flow_uav,
		m_flow_preview_uav,
		m_velocity_uav,
		m_sediment_uav
	};
	ID3D11Buffer* constant_buffers[] = { m_constant_buffer };
	ID3D11SamplerState* samplers[] = { Backend::DX11::Sampler::GetState() };

	m_context->CSSetShader(m_compute_shader, nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, constant_buffers);
	m_context->CSSetShaderResources(0, 3, srvs);
	m_context->CSSetSamplers(0, 1, samplers);
	m_context->CSSetUnorderedAccessViews(0, 5, uavs, nullptr);
	m_context->Dispatch(
		(kTextureWidth + kThreadGroupSize - 1) / kThreadGroupSize,
		(kTextureHeight + kThreadGroupSize - 1) / kThreadGroupSize,
		1);

	ID3D11ShaderResourceView* null_srvs[] = { nullptr, nullptr, nullptr };
	ID3D11UnorderedAccessView* null_uavs[] = { nullptr, nullptr, nullptr, nullptr, nullptr };
	ID3D11Buffer* null_cb = nullptr;
	ID3D11SamplerState* null_sampler = nullptr;
	m_context->CSSetShaderResources(0, 3, null_srvs);
	m_context->CSSetSamplers(0, 1, &null_sampler);
	m_context->CSSetUnorderedAccessViews(0, 5, null_uavs, nullptr);
	m_context->CSSetConstantBuffers(0, 1, &null_cb);
	m_context->CSSetShader(nullptr, nullptr, 0);

	m_current_index = next_index;
}

bool ComputeSurfaceWaterTexture::IsValid() const
{
	return m_compute_shader != nullptr &&
		   m_textures[0] != nullptr &&
		   m_textures[1] != nullptr &&
		   m_srvs[0] != nullptr &&
		   m_srvs[1] != nullptr &&
		   m_uavs[0] != nullptr &&
		   m_uavs[1] != nullptr &&
		   m_flow_texture != nullptr &&
		   m_flow_srv != nullptr &&
		   m_flow_uav != nullptr &&
		   m_flow_preview_texture != nullptr &&
		   m_flow_preview_srv != nullptr &&
		   m_flow_preview_uav != nullptr &&
		   m_velocity_texture != nullptr &&
		   m_velocity_srv != nullptr &&
		   m_velocity_uav != nullptr &&
		   m_sediment_texture != nullptr &&
		   m_sediment_srv != nullptr &&
		   m_sediment_uav != nullptr &&
		   m_constant_buffer != nullptr;
}

Backend::RenderShaderResource ComputeSurfaceWaterTexture::Resource() const
{
	return Backend::RenderShaderResource(m_srvs[m_current_index]);
}

Backend::RenderShaderResource ComputeSurfaceWaterTexture::FlowResource() const
{
	return Backend::RenderShaderResource(m_flow_srv);
}

Backend::RenderShaderResource ComputeSurfaceWaterTexture::FlowPreviewResource() const
{
	return Backend::RenderShaderResource(m_flow_preview_srv);
}

Backend::RenderShaderResource ComputeSurfaceWaterTexture::VelocityResource() const
{
	return Backend::RenderShaderResource(m_velocity_srv);
}

Backend::RenderShaderResource ComputeSurfaceWaterTexture::SedimentResource() const
{
	return Backend::RenderShaderResource(m_sediment_srv);
}
