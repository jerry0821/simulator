#include "compute_surface_water_texture.h"

#include <algorithm>
#include <cfloat>
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
	std::vector<float> zero_heightfield_pixels(kTextureWidth * kTextureHeight * 2u, 0.0f);
	D3D11_SUBRESOURCE_DATA heightfield_init_data{};
	heightfield_init_data.pSysMem = zero_heightfield_pixels.data();
	heightfield_init_data.SysMemPitch = sizeof(float) * 2u * kTextureWidth;

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

	D3D11_TEXTURE2D_DESC heightfield_desc{};
	heightfield_desc.Width = kTextureWidth;
	heightfield_desc.Height = kTextureHeight;
	heightfield_desc.MipLevels = 1;
	heightfield_desc.ArraySize = 1;
	heightfield_desc.Format = DXGI_FORMAT_R32G32_FLOAT;
	heightfield_desc.SampleDesc.Count = 1;
	heightfield_desc.Usage = D3D11_USAGE_DEFAULT;
	heightfield_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	for (int i = 0; i < 2; ++i)
	{
		if (FAILED(m_device->CreateTexture2D(&heightfield_desc, &heightfield_init_data, &m_heightfield_textures[i])))
		{
			Finalize();
			return false;
		}

		if (FAILED(m_device->CreateShaderResourceView(m_heightfield_textures[i], nullptr, &m_heightfield_srvs[i])))
		{
			Finalize();
			return false;
		}

		if (FAILED(m_device->CreateUnorderedAccessView(m_heightfield_textures[i], nullptr, &m_heightfield_uavs[i])))
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

	for (int i = 0; i < 2; ++i)
	{
		if (FAILED(m_device->CreateTexture2D(&flow_desc, &init_data, &m_flow_textures[i])))
		{
			Finalize();
			return false;
		}

		if (FAILED(m_device->CreateShaderResourceView(m_flow_textures[i], nullptr, &m_flow_srvs[i])))
		{
			Finalize();
			return false;
		}

		if (FAILED(m_device->CreateUnorderedAccessView(m_flow_textures[i], nullptr, &m_flow_uavs[i])))
		{
			Finalize();
			return false;
		}
	}

	D3D11_TEXTURE2D_DESC readback_desc = flow_desc;
	readback_desc.Usage = D3D11_USAGE_STAGING;
	readback_desc.BindFlags = 0;
	readback_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	readback_desc.MiscFlags = 0;
	if (FAILED(m_device->CreateTexture2D(&readback_desc, nullptr, &m_readback_surface_texture)))
	{
		Finalize();
		return false;
	}
	D3D11_TEXTURE2D_DESC heightfield_readback_desc = heightfield_desc;
	heightfield_readback_desc.Usage = D3D11_USAGE_STAGING;
	heightfield_readback_desc.BindFlags = 0;
	heightfield_readback_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	heightfield_readback_desc.MiscFlags = 0;
	if (FAILED(m_device->CreateTexture2D(&heightfield_readback_desc, nullptr, &m_readback_heightfield_texture)))
	{
		Finalize();
		return false;
	}
	if (FAILED(m_device->CreateTexture2D(&readback_desc, nullptr, &m_readback_flow_texture)))
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
	m_has_bootstrapped_state = false;
	return true;
}

void ComputeSurfaceWaterTexture::Finalize()
{
	m_cpu_debug_data_ready = false;
	m_has_bootstrapped_state = false;
	m_surface_water_amount_samples.clear();
	m_flow_magnitude_samples.clear();
	m_terrain_height_samples.clear();
	m_water_height_samples.clear();
	SafeRelease(m_constant_buffer);
	SafeRelease(m_readback_surface_texture);
	SafeRelease(m_readback_heightfield_texture);
	SafeRelease(m_readback_flow_texture);
	SafeRelease(m_flow_preview_uav);
	SafeRelease(m_flow_preview_srv);
	SafeRelease(m_flow_preview_texture);
	SafeRelease(m_velocity_uav);
	SafeRelease(m_velocity_srv);
	SafeRelease(m_velocity_texture);
	SafeRelease(m_sediment_uav);
	SafeRelease(m_sediment_srv);
	SafeRelease(m_sediment_texture);
	for (int i = 0; i < 2; ++i)
	{
		SafeRelease(m_heightfield_uavs[i]);
		SafeRelease(m_heightfield_srvs[i]);
		SafeRelease(m_heightfield_textures[i]);
	}
	for (int i = 0; i < 2; ++i)
	{
		SafeRelease(m_flow_uavs[i]);
		SafeRelease(m_flow_srvs[i]);
		SafeRelease(m_flow_textures[i]);
	}
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
		m_context->ClearUnorderedAccessViewFloat(m_heightfield_uavs[i], clear_values);
		m_context->ClearUnorderedAccessViewFloat(m_flow_uavs[i], clear_values);
	}
	m_context->ClearUnorderedAccessViewFloat(m_flow_preview_uav, clear_values);
	m_context->ClearUnorderedAccessViewFloat(m_velocity_uav, clear_values);
	m_context->ClearUnorderedAccessViewFloat(m_sediment_uav, clear_values);
	m_has_bootstrapped_state = false;
	m_cpu_debug_data_ready = false;
	m_surface_water_amount_samples.clear();
	m_flow_magnitude_samples.clear();
	m_terrain_height_samples.clear();
	m_water_height_samples.clear();
}

void ComputeSurfaceWaterTexture::Update(
	ID3D11ShaderResourceView* terrain_height_srv,
	ID3D11ShaderResourceView* rain_map_srv,
	ID3D11ShaderResourceView* wind_field_srv,
	float water_height,
	const SurfaceWaterSimulationSettings& settings,
	bool inject_water_pulse,
	float time_seconds,
	float delta_time_seconds)
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
		std::clamp(delta_time_seconds, 0.0f, 1.0f / 30.0f),
		kTextureWidth,
		kTextureHeight,
		settings.presentation_only ? 1u : 0u,
		inject_water_pulse ? 1u : 0u,
		m_has_bootstrapped_state ? 0u : 1u,
		0u,
		0u
	};

	m_context->UpdateSubresource(m_constant_buffer, 0, nullptr, &constants, 0, 0);

	ID3D11ShaderResourceView* srvs[] = {
		terrain_height_srv,
		rain_map_srv,
		wind_field_srv,
		m_flow_srvs[m_current_index]
	};
	ID3D11UnorderedAccessView* uavs[] = {
		m_uavs[next_index],
		m_flow_uavs[next_index],
		m_flow_preview_uav,
		m_velocity_uav,
		m_sediment_uav,
		m_heightfield_uavs[next_index]
	};
	ID3D11Buffer* constant_buffers[] = { m_constant_buffer };
	ID3D11SamplerState* samplers[] = { Backend::DX11::Sampler::GetState() };

	m_context->CSSetShader(m_compute_shader, nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, constant_buffers);
	m_context->CSSetShaderResources(0, 4, srvs);
	m_context->CSSetSamplers(0, 1, samplers);
	m_context->CSSetUnorderedAccessViews(0, 6, uavs, nullptr);
	m_context->Dispatch(
		(kTextureWidth + kThreadGroupSize - 1) / kThreadGroupSize,
		(kTextureHeight + kThreadGroupSize - 1) / kThreadGroupSize,
		1);

	ID3D11ShaderResourceView* null_srvs[] = { nullptr, nullptr, nullptr, nullptr };
	ID3D11UnorderedAccessView* null_uavs[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	ID3D11Buffer* null_cb = nullptr;
	ID3D11SamplerState* null_sampler = nullptr;
	m_context->CSSetShaderResources(0, 4, null_srvs);
	m_context->CSSetSamplers(0, 1, &null_sampler);
	m_context->CSSetUnorderedAccessViews(0, 6, null_uavs, nullptr);
	m_context->CSSetConstantBuffers(0, 1, &null_cb);
	m_context->CSSetShader(nullptr, nullptr, 0);

	m_current_index = next_index;
	m_has_bootstrapped_state = true;

	if (m_readback_surface_texture == nullptr ||
		m_readback_heightfield_texture == nullptr ||
		m_readback_flow_texture == nullptr)
	{
		m_cpu_debug_data_ready = false;
		return;
	}

	m_context->CopyResource(m_readback_surface_texture, m_textures[next_index]);
	m_context->CopyResource(m_readback_heightfield_texture, m_heightfield_textures[next_index]);
	m_context->CopyResource(m_readback_flow_texture, m_flow_textures[next_index]);
	m_context->Flush();

	const size_t sample_count = static_cast<size_t>(kTextureWidth) * static_cast<size_t>(kTextureHeight);
	m_surface_water_amount_samples.assign(sample_count, 0.0f);
	m_flow_magnitude_samples.assign(sample_count, 0.0f);
	m_terrain_height_samples.assign(sample_count, 0.0f);
	m_water_height_samples.assign(sample_count, 0.0f);

	D3D11_MAPPED_SUBRESOURCE mapped_surface{};
	D3D11_MAPPED_SUBRESOURCE mapped_heightfield{};
	D3D11_MAPPED_SUBRESOURCE mapped_flow{};
	const HRESULT surface_map_result = m_context->Map(m_readback_surface_texture, 0, D3D11_MAP_READ, 0, &mapped_surface);
	const HRESULT heightfield_map_result = m_context->Map(m_readback_heightfield_texture, 0, D3D11_MAP_READ, 0, &mapped_heightfield);
	const HRESULT flow_map_result = m_context->Map(m_readback_flow_texture, 0, D3D11_MAP_READ, 0, &mapped_flow);
	if (FAILED(surface_map_result) || FAILED(heightfield_map_result) || FAILED(flow_map_result))
	{
		hal::dout
			<< "ComputeSurfaceWaterTexture::Update(): readback map failed. surface=0x"
			<< std::hex << static_cast<unsigned long>(surface_map_result)
			<< " heightfield=0x" << static_cast<unsigned long>(heightfield_map_result)
			<< " flow=0x" << static_cast<unsigned long>(flow_map_result)
			<< std::dec << std::endl;
		if (SUCCEEDED(surface_map_result))
		{
			m_context->Unmap(m_readback_surface_texture, 0);
		}
		if (SUCCEEDED(heightfield_map_result))
		{
			m_context->Unmap(m_readback_heightfield_texture, 0);
		}
		if (SUCCEEDED(flow_map_result))
		{
			m_context->Unmap(m_readback_flow_texture, 0);
		}
		m_cpu_debug_data_ready = false;
		return;
	}

	for (unsigned int row = 0; row < kTextureHeight; ++row)
	{
		const float* surface_row = reinterpret_cast<const float*>(
			static_cast<const unsigned char*>(mapped_surface.pData) + mapped_surface.RowPitch * row);
		const float* heightfield_row = reinterpret_cast<const float*>(
			static_cast<const unsigned char*>(mapped_heightfield.pData) + mapped_heightfield.RowPitch * row);
		const float* flow_row = reinterpret_cast<const float*>(
			static_cast<const unsigned char*>(mapped_flow.pData) + mapped_flow.RowPitch * row);
		for (unsigned int col = 0; col < kTextureWidth; ++col)
		{
			const size_t sample_index = static_cast<size_t>(row) * kTextureWidth + col;
			m_surface_water_amount_samples[sample_index] = surface_row[col * 4 + 0];
			m_terrain_height_samples[sample_index] = heightfield_row[col * 2 + 0];
			m_water_height_samples[sample_index] = heightfield_row[col * 2 + 1];
			const float outflow_x = flow_row[col * 4 + 0];
			const float outflow_y = flow_row[col * 4 + 1];
			const float outflow_z = flow_row[col * 4 + 2];
			const float outflow_w = flow_row[col * 4 + 3];
			m_flow_magnitude_samples[sample_index] = outflow_x + outflow_y + outflow_z + outflow_w;
		}
	}

	m_context->Unmap(m_readback_surface_texture, 0);
	m_context->Unmap(m_readback_heightfield_texture, 0);
	m_context->Unmap(m_readback_flow_texture, 0);
	m_cpu_debug_data_ready = true;
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
		   m_heightfield_textures[0] != nullptr &&
		   m_heightfield_textures[1] != nullptr &&
		   m_heightfield_srvs[0] != nullptr &&
		   m_heightfield_srvs[1] != nullptr &&
		   m_heightfield_uavs[0] != nullptr &&
		   m_heightfield_uavs[1] != nullptr &&
		   m_flow_textures[0] != nullptr &&
		   m_flow_textures[1] != nullptr &&
		   m_flow_srvs[0] != nullptr &&
		   m_flow_srvs[1] != nullptr &&
		   m_flow_uavs[0] != nullptr &&
		   m_flow_uavs[1] != nullptr &&
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

Backend::RenderShaderResource ComputeSurfaceWaterTexture::HeightfieldResource() const
{
	return Backend::RenderShaderResource(m_heightfield_srvs[m_current_index]);
}

Backend::RenderShaderResource ComputeSurfaceWaterTexture::FlowResource() const
{
	return Backend::RenderShaderResource(m_flow_srvs[m_current_index]);
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

bool ComputeSurfaceWaterTexture::ComputeWaterAmountRange(float& out_min_amount, float& out_max_amount) const
{
	if (!m_cpu_debug_data_ready || m_surface_water_amount_samples.empty())
	{
		return false;
	}

	const auto [min_it, max_it] =
		std::minmax_element(m_surface_water_amount_samples.begin(), m_surface_water_amount_samples.end());
	if (min_it == m_surface_water_amount_samples.end() || max_it == m_surface_water_amount_samples.end())
	{
		return false;
	}

	out_min_amount = *min_it;
	out_max_amount = *max_it;
	return true;
}

bool ComputeSurfaceWaterTexture::ComputeFlowMagnitudeRange(float& out_min_magnitude, float& out_max_magnitude) const
{
	if (!m_cpu_debug_data_ready || m_flow_magnitude_samples.empty())
	{
		return false;
	}

	const auto [min_it, max_it] =
		std::minmax_element(m_flow_magnitude_samples.begin(), m_flow_magnitude_samples.end());
	if (min_it == m_flow_magnitude_samples.end() || max_it == m_flow_magnitude_samples.end())
	{
		return false;
	}

	out_min_magnitude = *min_it;
	out_max_magnitude = *max_it;
	return true;
}

bool ComputeSurfaceWaterTexture::ComputeTerrainHeightRange(float& out_min_height, float& out_max_height) const
{
	if (!m_cpu_debug_data_ready || m_terrain_height_samples.empty())
	{
		return false;
	}

	const auto [min_it, max_it] =
		std::minmax_element(m_terrain_height_samples.begin(), m_terrain_height_samples.end());
	if (min_it == m_terrain_height_samples.end() || max_it == m_terrain_height_samples.end())
	{
		return false;
	}

	out_min_height = *min_it;
	out_max_height = *max_it;
	return true;
}

bool ComputeSurfaceWaterTexture::ComputeWaterHeightRange(float& out_min_height, float& out_max_height) const
{
	if (!m_cpu_debug_data_ready || m_water_height_samples.empty())
	{
		return false;
	}

	const auto [min_it, max_it] =
		std::minmax_element(m_water_height_samples.begin(), m_water_height_samples.end());
	if (min_it == m_water_height_samples.end() || max_it == m_water_height_samples.end())
	{
		return false;
	}

	out_min_height = *min_it;
	out_max_height = *max_it;
	return true;
}

bool ComputeSurfaceWaterTexture::ComputeWaterDepthRange(float& out_min_depth, float& out_max_depth) const
{
	if (!m_cpu_debug_data_ready ||
		m_water_height_samples.empty() ||
		m_terrain_height_samples.empty() ||
		m_water_height_samples.size() != m_terrain_height_samples.size())
	{
		return false;
	}

	float min_depth = FLT_MAX;
	float max_depth = -FLT_MAX;
	for (size_t i = 0; i < m_water_height_samples.size(); ++i)
	{
		const float depth = std::max(m_water_height_samples[i] - m_terrain_height_samples[i], 0.0f);
		min_depth = std::min(min_depth, depth);
		max_depth = std::max(max_depth, depth);
	}

	if (min_depth == FLT_MAX || max_depth == -FLT_MAX)
	{
		return false;
	}

	out_min_depth = min_depth;
	out_max_depth = max_depth;
	return true;
}

bool ComputeSurfaceWaterTexture::SampleHeightCell(
	unsigned int x,
	unsigned int y,
	float& out_terrain_height,
	float& out_water_height,
	float& out_water_depth) const
{
	if (!m_cpu_debug_data_ready ||
		m_terrain_height_samples.empty() ||
		m_water_height_samples.empty() ||
		m_terrain_height_samples.size() != m_water_height_samples.size())
	{
		return false;
	}

	if (x >= kTextureWidth || y >= kTextureHeight)
	{
		return false;
	}

	const size_t sample_index = static_cast<size_t>(y) * kTextureWidth + x;
	out_terrain_height = m_terrain_height_samples[sample_index];
	out_water_height = m_water_height_samples[sample_index];
	out_water_depth = std::max(out_water_height - out_terrain_height, 0.0f);
	return true;
}

bool ComputeSurfaceWaterTexture::SampleCell(
	unsigned int x,
	unsigned int y,
	float& out_surface_water_amount,
	float& out_flow_sum) const
{
	if (!m_cpu_debug_data_ready ||
		m_surface_water_amount_samples.empty() ||
		m_flow_magnitude_samples.empty() ||
		m_surface_water_amount_samples.size() != m_flow_magnitude_samples.size())
	{
		return false;
	}

	if (x >= kTextureWidth || y >= kTextureHeight)
	{
		return false;
	}

	const size_t sample_index = static_cast<size_t>(y) * kTextureWidth + x;
	out_surface_water_amount = m_surface_water_amount_samples[sample_index];
	out_flow_sum = m_flow_magnitude_samples[sample_index];
	return true;
}
