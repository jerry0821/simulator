#include "compute_water_surface_height_texture.h"

#include <algorithm>
#include <cstring>
#include <d3d11.h>
#include <fstream>
#include <vector>

#include "debug_menu.h"
#include "debug_ostream.h"
#include "meshfield.h"
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

bool ComputeWaterSurfaceHeightTexture::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	Finalize();

	m_device = device;
	m_context = context;
	if (m_device == nullptr || m_context == nullptr)
	{
		return false;
	}

	std::ifstream shader_stream("resource/shader/shader_compute_water_surface_height.cso", std::ios::binary);
	if (!shader_stream)
	{
		hal::dout << "ComputeWaterSurfaceHeightTexture::Initialize(): failed to open shader_compute_water_surface_height.cso" << std::endl;
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
		hal::dout << "ComputeWaterSurfaceHeightTexture::Initialize(): CreateComputeShader failed" << std::endl;
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
		if (FAILED(m_device->CreateTexture2D(&texture_desc, nullptr, &m_textures[index])))
		{
			Finalize();
			return false;
		}

		if (FAILED(m_device->CreateShaderResourceView(m_textures[index], nullptr, &m_srvs[index])))
		{
			Finalize();
			return false;
		}

		if (FAILED(m_device->CreateUnorderedAccessView(m_textures[index], nullptr, &m_uavs[index])))
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
	std::vector<float> zero_flow_pixels(kTextureWidth * kTextureHeight * 4u, 0.0f);
	D3D11_SUBRESOURCE_DATA flow_init_data{};
	flow_init_data.pSysMem = zero_flow_pixels.data();
	flow_init_data.SysMemPitch = sizeof(float) * 4u * kTextureWidth;
	for (unsigned int index = 0; index < 2; ++index)
	{
		if (FAILED(m_device->CreateTexture2D(&flow_desc, &flow_init_data, &m_flow_textures[index])))
		{
			Finalize();
			return false;
		}

		if (FAILED(m_device->CreateShaderResourceView(m_flow_textures[index], nullptr, &m_flow_srvs[index])))
		{
			Finalize();
			return false;
		}

		if (FAILED(m_device->CreateUnorderedAccessView(m_flow_textures[index], nullptr, &m_flow_uavs[index])))
		{
			Finalize();
			return false;
		}
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

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.ByteWidth = sizeof(WaterSurfaceHeightConstants);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	if (FAILED(m_device->CreateBuffer(&buffer_desc, nullptr, &m_constant_buffer)))
	{
		Finalize();
		return false;
	}

	m_current_index = 0u;
	m_has_bootstrapped_state = false;

	return true;
}

void ComputeWaterSurfaceHeightTexture::Finalize()
{
	m_cpu_height_data_ready = false;
	m_terrain_height_samples.clear();
	m_water_height_samples.clear();
	SafeRelease(m_constant_buffer);
	SafeRelease(m_readback_texture);
	for (unsigned int index = 0; index < 2; ++index)
	{
		SafeRelease(m_flow_uavs[index]);
		SafeRelease(m_flow_srvs[index]);
		SafeRelease(m_flow_textures[index]);
	}
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
	m_has_bootstrapped_state = false;
}

void ComputeWaterSurfaceHeightTexture::ResetState()
{
	if (!IsValid())
	{
		return;
	}

	const float clear_values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	for (unsigned int index = 0; index < 2; ++index)
	{
		m_context->ClearUnorderedAccessViewFloat(m_uavs[index], clear_values);
		m_context->ClearUnorderedAccessViewFloat(m_flow_uavs[index], clear_values);
	}
	m_current_index = 0u;
	m_has_bootstrapped_state = false;
	m_cpu_height_data_ready = false;
	m_terrain_height_samples.clear();
	m_water_height_samples.clear();
}

void ComputeWaterSurfaceHeightTexture::InitializeState(
	ID3D11ShaderResourceView* terrain_height_srv,
	float water_surface_height,
	const SurfaceWaterSimulationSettings& settings) const
{
	if (!IsValid() || terrain_height_srv == nullptr)
	{
		return;
	}

	const WaterSurfaceHeightConstants constants = {
		water_surface_height,
		0.02f,
		settings.downhill_flow_rate * 3.0f,
		settings.max_outflow_fraction,
		settings.evaporation_rate,
		settings.accumulation_rate,
		settings.seepage_rate,
		settings.basin_fade,
		MeshFieldRenderer::FieldWidth(),
		MeshFieldRenderer::FieldDepth(),
		settings.debug_injection_x,
		settings.debug_injection_z,
		settings.debug_injection_radius,
		settings.debug_injection_amount,
		1.0f / 60.0f,
		kTextureWidth,
		kTextureHeight,
		1u,
		0u,
		0u,
		0u,
		0u,
		0u,
		0u
	};

	const unsigned int previous_index = m_current_index;
	const unsigned int next_index = (m_current_index + 1u) % 2u;

	m_context->UpdateSubresource(m_constant_buffer, 0, nullptr, &constants, 0, 0);

	ID3D11ShaderResourceView* srvs[] = {
		terrain_height_srv,
		m_srvs[previous_index],
		nullptr,
		m_flow_srvs[previous_index]
	};
	ID3D11UnorderedAccessView* uavs[] = { m_uavs[next_index], m_flow_uavs[next_index] };
	ID3D11Buffer* constant_buffers[] = { m_constant_buffer };
	ID3D11SamplerState* samplers[] = { Backend::DX11::Sampler::GetState() };

	m_context->CSSetShader(m_compute_shader, nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, constant_buffers);
	m_context->CSSetShaderResources(0, 4, srvs);
	m_context->CSSetSamplers(0, 1, samplers);
	m_context->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
	m_context->Dispatch(
		(kTextureWidth + kThreadGroupSize - 1) / kThreadGroupSize,
		(kTextureHeight + kThreadGroupSize - 1) / kThreadGroupSize,
		1);

	ID3D11ShaderResourceView* null_srvs[] = { nullptr, nullptr, nullptr, nullptr };
	ID3D11UnorderedAccessView* null_uavs[] = { nullptr, nullptr };
	ID3D11Buffer* null_cb = nullptr;
	ID3D11SamplerState* null_sampler = nullptr;
	m_context->CSSetShaderResources(0, 4, null_srvs);
	m_context->CSSetSamplers(0, 1, &null_sampler);
	m_context->CSSetUnorderedAccessViews(0, 2, null_uavs, nullptr);
	m_context->CSSetConstantBuffers(0, 1, &null_cb);
	m_context->CSSetShader(nullptr, nullptr, 0);

	// Promote the freshly written heightfield for GPU consumers immediately.
	// CPU readback is only for debug/range inspection and must not gate render usage.
	m_current_index = next_index;
	m_has_bootstrapped_state = true;

	if (m_readback_texture == nullptr)
	{
		m_cpu_height_data_ready = false;
		return;
	}

	m_context->CopyResource(m_readback_texture, m_textures[next_index]);
	m_context->Flush();

	D3D11_MAPPED_SUBRESOURCE mapped_resource{};
	const HRESULT map_result = m_context->Map(m_readback_texture, 0, D3D11_MAP_READ, 0, &mapped_resource);
	if (FAILED(map_result))
	{
		hal::dout << "ComputeWaterSurfaceHeightTexture::Update(): staging readback Map failed -> 0x"
				  << std::hex << static_cast<unsigned long>(map_result) << std::dec << std::endl;
		m_cpu_height_data_ready = false;
		return;
	}

	const size_t sample_count = static_cast<size_t>(kTextureWidth) * static_cast<size_t>(kTextureHeight);
	m_terrain_height_samples.assign(sample_count, 0.0f);
	m_water_height_samples.assign(sample_count, 0.0f);
	for (unsigned int row = 0; row < kTextureHeight; ++row)
	{
		const float* source_row = reinterpret_cast<const float*>(
			static_cast<const unsigned char*>(mapped_resource.pData) + mapped_resource.RowPitch * row);
		for (unsigned int col = 0; col < kTextureWidth; ++col)
		{
			const size_t sample_index = static_cast<size_t>(row) * kTextureWidth + col;
			m_terrain_height_samples[sample_index] = source_row[col * 4 + 0];
			m_water_height_samples[sample_index] = source_row[col * 4 + 1];
		}
	}

	m_context->Unmap(m_readback_texture, 0);
	m_cpu_height_data_ready = true;
}

void ComputeWaterSurfaceHeightTexture::Update(
	ID3D11ShaderResourceView* terrain_height_srv,
	ID3D11ShaderResourceView* rain_map_srv,
	float water_surface_height,
	const SurfaceWaterSimulationSettings& settings,
	bool inject_water_pulse,
	float delta_time_seconds) const
{
	if (!IsValid() || terrain_height_srv == nullptr || rain_map_srv == nullptr)
	{
		return;
	}

	const WaterSurfaceHeightConstants constants = {
		water_surface_height,
		0.02f,
		settings.downhill_flow_rate * 3.0f,
		settings.max_outflow_fraction,
		settings.evaporation_rate,
		settings.accumulation_rate,
		settings.seepage_rate,
		settings.basin_fade,
		MeshFieldRenderer::FieldWidth(),
		MeshFieldRenderer::FieldDepth(),
		settings.debug_injection_x,
		settings.debug_injection_z,
		settings.debug_injection_radius,
		settings.debug_injection_amount,
		std::clamp(delta_time_seconds, 0.0f, 1.0f / 30.0f),
		kTextureWidth,
		kTextureHeight,
		0u,
		inject_water_pulse ? 1u : 0u,
		0u,
		0u,
		0u,
		0u,
		0u
	};

	const unsigned int previous_index = m_current_index;
	const unsigned int next_index = (m_current_index + 1u) % 2u;

	m_context->UpdateSubresource(m_constant_buffer, 0, nullptr, &constants, 0, 0);

	ID3D11ShaderResourceView* srvs[] = {
		terrain_height_srv,
		m_srvs[previous_index],
		rain_map_srv,
		m_flow_srvs[previous_index]
	};
	ID3D11UnorderedAccessView* uavs[] = { m_uavs[next_index], m_flow_uavs[next_index] };
	ID3D11Buffer* constant_buffers[] = { m_constant_buffer };
	ID3D11SamplerState* samplers[] = { Backend::DX11::Sampler::GetState() };

	m_context->CSSetShader(m_compute_shader, nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, constant_buffers);
	m_context->CSSetShaderResources(0, 4, srvs);
	m_context->CSSetSamplers(0, 1, samplers);
	m_context->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
	m_context->Dispatch(
		(kTextureWidth + kThreadGroupSize - 1) / kThreadGroupSize,
		(kTextureHeight + kThreadGroupSize - 1) / kThreadGroupSize,
		1);

	ID3D11ShaderResourceView* null_srvs[] = { nullptr, nullptr, nullptr, nullptr };
	ID3D11UnorderedAccessView* null_uavs[] = { nullptr, nullptr };
	ID3D11Buffer* null_cb = nullptr;
	ID3D11SamplerState* null_sampler = nullptr;
	m_context->CSSetShaderResources(0, 4, null_srvs);
	m_context->CSSetSamplers(0, 1, &null_sampler);
	m_context->CSSetUnorderedAccessViews(0, 2, null_uavs, nullptr);
	m_context->CSSetConstantBuffers(0, 1, &null_cb);
	m_context->CSSetShader(nullptr, nullptr, 0);

	m_current_index = next_index;
	m_has_bootstrapped_state = true;

	if (m_readback_texture == nullptr)
	{
		m_cpu_height_data_ready = false;
		return;
	}

	m_context->CopyResource(m_readback_texture, m_textures[next_index]);
	m_context->Flush();

	D3D11_MAPPED_SUBRESOURCE mapped_resource{};
	const HRESULT map_result = m_context->Map(m_readback_texture, 0, D3D11_MAP_READ, 0, &mapped_resource);
	if (FAILED(map_result))
	{
		hal::dout << "ComputeWaterSurfaceHeightTexture::Update(): staging readback Map failed -> 0x"
				  << std::hex << static_cast<unsigned long>(map_result) << std::dec << std::endl;
		m_cpu_height_data_ready = false;
		return;
	}

	const size_t sample_count = static_cast<size_t>(kTextureWidth) * static_cast<size_t>(kTextureHeight);
	m_terrain_height_samples.assign(sample_count, 0.0f);
	m_water_height_samples.assign(sample_count, 0.0f);
	for (unsigned int row = 0; row < kTextureHeight; ++row)
	{
		const float* source_row = reinterpret_cast<const float*>(
			static_cast<const unsigned char*>(mapped_resource.pData) + mapped_resource.RowPitch * row);
		for (unsigned int col = 0; col < kTextureWidth; ++col)
		{
			const size_t sample_index = static_cast<size_t>(row) * kTextureWidth + col;
			m_terrain_height_samples[sample_index] = source_row[col * 4 + 0];
			m_water_height_samples[sample_index] = source_row[col * 4 + 1];
		}
	}

	m_context->Unmap(m_readback_texture, 0);
	m_cpu_height_data_ready = true;
}

bool ComputeWaterSurfaceHeightTexture::IsValid() const
{
	return m_compute_shader != nullptr &&
		   m_textures[0] != nullptr &&
		   m_textures[1] != nullptr &&
		   m_srvs[0] != nullptr &&
		   m_srvs[1] != nullptr &&
		   m_uavs[0] != nullptr &&
		   m_uavs[1] != nullptr &&
		   m_flow_textures[0] != nullptr &&
		   m_flow_textures[1] != nullptr &&
		   m_flow_srvs[0] != nullptr &&
		   m_flow_srvs[1] != nullptr &&
		   m_flow_uavs[0] != nullptr &&
		   m_flow_uavs[1] != nullptr &&
		   m_constant_buffer != nullptr;
}

Backend::RenderShaderResource ComputeWaterSurfaceHeightTexture::Resource() const
{
	return Backend::RenderShaderResource(m_srvs[m_current_index]);
}

Backend::RenderShaderResource ComputeWaterSurfaceHeightTexture::FlowResource() const
{
	return Backend::RenderShaderResource(m_flow_srvs[m_current_index]);
}

bool ComputeWaterSurfaceHeightTexture::ComputeTerrainHeightRange(float& out_min_height, float& out_max_height) const
{
	if (!m_cpu_height_data_ready || m_terrain_height_samples.empty())
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

bool ComputeWaterSurfaceHeightTexture::ComputeWaterHeightRange(float& out_min_height, float& out_max_height) const
{
	if (!m_cpu_height_data_ready || m_water_height_samples.empty())
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
