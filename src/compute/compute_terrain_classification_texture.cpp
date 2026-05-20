#include "compute_terrain_classification_texture.h"

#include <d3d11.h>
#include <fstream>
#include <vector>

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

bool ComputeTerrainClassificationTexture::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	Finalize();

	m_device = device;
	m_context = context;
	if (m_device == nullptr || m_context == nullptr)
	{
		return false;
	}

	std::ifstream shader_stream("resource/shader/shader_compute_terrain_classification.cso", std::ios::binary);
	if (!shader_stream)
	{
		hal::dout << "ComputeTerrainClassificationTexture::Initialize(): failed to open shader_compute_terrain_classification.cso" << std::endl;
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
		hal::dout << "ComputeTerrainClassificationTexture::Initialize(): CreateComputeShader failed" << std::endl;
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

	D3D11_TEXTURE2D_DESC vegetation_desc = texture_desc;
	vegetation_desc.Format = DXGI_FORMAT_R16_FLOAT;
	if (FAILED(m_device->CreateTexture2D(&vegetation_desc, nullptr, &m_vegetation_texture)))
	{
		Finalize();
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC vegetation_srv_desc{};
	vegetation_srv_desc.Format = vegetation_desc.Format;
	vegetation_srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	vegetation_srv_desc.Texture2D.MipLevels = 1;
	if (FAILED(m_device->CreateShaderResourceView(m_vegetation_texture, &vegetation_srv_desc, &m_vegetation_srv)))
	{
		Finalize();
		return false;
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC vegetation_uav_desc{};
	vegetation_uav_desc.Format = vegetation_desc.Format;
	vegetation_uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	if (FAILED(m_device->CreateUnorderedAccessView(m_vegetation_texture, &vegetation_uav_desc, &m_vegetation_uav)))
	{
		Finalize();
		return false;
	}

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.ByteWidth = sizeof(TerrainClassificationConstants);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (FAILED(m_device->CreateBuffer(&buffer_desc, nullptr, &m_constant_buffer)))
	{
		hal::dout << "ComputeTerrainClassificationTexture::Initialize(): CreateBuffer(constant) failed" << std::endl;
		Finalize();
		return false;
	}

	return true;
}

void ComputeTerrainClassificationTexture::Finalize()
{
	SafeRelease(m_constant_buffer);
	SafeRelease(m_vegetation_uav);
	SafeRelease(m_vegetation_srv);
	SafeRelease(m_vegetation_texture);
	SafeRelease(m_compute_shader);
	m_device = nullptr;
	m_context = nullptr;
}

void ComputeTerrainClassificationTexture::Update(
	ID3D11ShaderResourceView* terrain_height_srv,
	ID3D11ShaderResourceView* terrain_normal_srv,
	ID3D11ShaderResourceView* water_interaction_srv,
	ID3D11ShaderResourceView* erosion_delta_srv,
	ID3D11ShaderResourceView* climate_srv,
	const TerrainMaterialSettings& material_settings)
{
	if (!IsValid() ||
		terrain_height_srv == nullptr ||
		terrain_normal_srv == nullptr ||
		water_interaction_srv == nullptr ||
		erosion_delta_srv == nullptr ||
		climate_srv == nullptr)
	{
		return;
	}

	const TerrainClassificationConstants constants = {
		material_settings.grass_slope_min,
		material_settings.grass_slope_max,
		material_settings.grass_noise_strength,
		material_settings.grass_height_start,
		material_settings.grass_height_end,
		material_settings.rock_slope_start,
		material_settings.rock_slope_end,
		material_settings.rock_height_start,
		material_settings.rock_height_end,
		material_settings.stone_noise_scale,
		material_settings.shoreline_offset_start,
		material_settings.shoreline_offset_end,
		material_settings.lowland_height_start,
		material_settings.lowland_height_end,
		material_settings.grass_coverage_min,
		material_settings.water_height,
		MeshFieldRenderer::FieldWidth(),
		MeshFieldRenderer::FieldDepth(),
		2.2f,
		1.0f,
		kTextureWidth,
		kTextureHeight,
		0u,
		0u
	};

	m_context->UpdateSubresource(m_constant_buffer, 0, nullptr, &constants, 0, 0);

	ID3D11ShaderResourceView* srvs[] = {
		terrain_height_srv,
		terrain_normal_srv,
		water_interaction_srv,
		erosion_delta_srv,
		climate_srv
	};
	ID3D11UnorderedAccessView* uavs[] = { m_vegetation_uav };
	ID3D11Buffer* constant_buffers[] = { m_constant_buffer };
	ID3D11SamplerState* samplers[] = { Backend::DX11::Sampler::GetState() };

	m_context->CSSetShader(m_compute_shader, nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, constant_buffers);
	m_context->CSSetShaderResources(0, 5, srvs);
	m_context->CSSetSamplers(0, 1, samplers);
	m_context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
	m_context->Dispatch(
		(kTextureWidth + kThreadGroupSize - 1) / kThreadGroupSize,
		(kTextureHeight + kThreadGroupSize - 1) / kThreadGroupSize,
		1);

	ID3D11ShaderResourceView* null_srvs[] = { nullptr, nullptr, nullptr, nullptr, nullptr };
	ID3D11UnorderedAccessView* null_uavs[] = { nullptr };
	ID3D11Buffer* null_cb = nullptr;
	ID3D11SamplerState* null_sampler = nullptr;
	m_context->CSSetShaderResources(0, 5, null_srvs);
	m_context->CSSetSamplers(0, 1, &null_sampler);
	m_context->CSSetUnorderedAccessViews(0, 1, null_uavs, nullptr);
	m_context->CSSetConstantBuffers(0, 1, &null_cb);
	m_context->CSSetShader(nullptr, nullptr, 0);
}

bool ComputeTerrainClassificationTexture::IsValid() const
{
	return m_compute_shader != nullptr &&
		m_vegetation_texture != nullptr &&
		m_vegetation_srv != nullptr &&
		m_vegetation_uav != nullptr &&
		m_constant_buffer != nullptr;
}

Backend::RenderShaderResource ComputeTerrainClassificationTexture::VegetationSuitabilityResource() const
{
	return Backend::RenderShaderResource(m_vegetation_srv);
}
