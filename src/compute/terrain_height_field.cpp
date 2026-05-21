#include "terrain_height_field.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "compute_texture_dimensions.h"
#include "meshfield.h"
#include "direct3d.h"

namespace
{
constexpr unsigned int kTerrainHeightTextureWidth = ComputeTextureDimensions::kTerrainHeightResolution;
constexpr unsigned int kTerrainHeightTextureHeight = ComputeTextureDimensions::kTerrainHeightResolution;
constexpr size_t kTerrainHeightSampleCount =
	static_cast<size_t>(kTerrainHeightTextureWidth) * static_cast<size_t>(kTerrainHeightTextureHeight);
constexpr float kFieldWidth = ComputeTextureDimensions::kWorldSideLength;
constexpr float kFieldDepth = ComputeTextureDimensions::kWorldSideLength;

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;

bool g_is_flat_height_field = false;
TerrainSettings g_terrain_settings{};
std::vector<float> g_original_heights;

ID3D11ComputeShader* g_height_compute_shader = nullptr;
ID3D11Texture2D* g_height_texture = nullptr;
ID3D11ShaderResourceView* g_height_texture_srv = nullptr;
ID3D11UnorderedAccessView* g_height_texture_uav = nullptr;
ID3D11Texture2D* g_height_readback_texture = nullptr;
ID3D11Buffer* g_height_constant_buffer = nullptr;
ID3D11Texture2D* g_flat_height_texture = nullptr;
ID3D11ShaderResourceView* g_flat_height_texture_srv = nullptr;

struct TerrainHeightConstants
{
	float base_frequency = 0.0f;
	float base_height = 0.0f;
	float detail_frequency = 0.0f;
	float detail_height = 0.0f;
	float ridge_frequency = 0.0f;
	float ridge_height = 0.0f;
	float continent_height = 0.0f;
	float lake_center_z = 0.0f;
	float lake_radius_x = 0.0f;
	float lake_radius_z = 0.0f;
	float lake_depth = 0.0f;
	float field_width = 0.0f;
	float field_depth = 0.0f;
	float cell_size_x = 0.0f;
	float cell_size_z = 0.0f;
	unsigned int width = 0u;
	unsigned int height = 0u;
	float padding0 = 0.0f;
	float padding1 = 0.0f;
	float padding2 = 0.0f;
};

bool CreateFloatTexture(
	unsigned int width,
	unsigned int height,
	unsigned int bind_flags,
	const float* initial_data,
	ID3D11Texture2D** texture,
	ID3D11ShaderResourceView** srv,
	ID3D11UnorderedAccessView** uav)
{
	if (g_device == nullptr || texture == nullptr)
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC texture_desc{};
	texture_desc.Width = width;
	texture_desc.Height = height;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = DXGI_FORMAT_R32_FLOAT;
	texture_desc.SampleDesc.Count = 1;
	texture_desc.Usage = D3D11_USAGE_DEFAULT;
	texture_desc.BindFlags = bind_flags;

	D3D11_SUBRESOURCE_DATA init_data{};
	D3D11_SUBRESOURCE_DATA* init_data_ptr = nullptr;
	if (initial_data != nullptr)
	{
		init_data.pSysMem = initial_data;
		init_data.SysMemPitch = width * sizeof(float);
		init_data_ptr = &init_data;
	}

	if (FAILED(g_device->CreateTexture2D(&texture_desc, init_data_ptr, texture)))
	{
		return false;
	}

	if (srv != nullptr && (bind_flags & D3D11_BIND_SHADER_RESOURCE) != 0)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
		srv_desc.Format = texture_desc.Format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;
		srv_desc.Texture2D.MostDetailedMip = 0;
		if (FAILED(g_device->CreateShaderResourceView(*texture, &srv_desc, srv)))
		{
			SAFE_RELEASE(*texture);
			return false;
		}
	}

	if (uav != nullptr && (bind_flags & D3D11_BIND_UNORDERED_ACCESS) != 0)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
		uav_desc.Format = texture_desc.Format;
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		if (FAILED(g_device->CreateUnorderedAccessView(*texture, &uav_desc, uav)))
		{
			if (srv != nullptr)
			{
				SAFE_RELEASE(*srv);
			}
			SAFE_RELEASE(*texture);
			return false;
		}
	}

	return true;
}

bool CreateFlatHeightTexture()
{
	SAFE_RELEASE(g_flat_height_texture_srv);
	SAFE_RELEASE(g_flat_height_texture);

	std::vector<float> flat_heights(kTerrainHeightSampleCount, 0.0f);
	if (!CreateFloatTexture(
			kTerrainHeightTextureWidth,
			kTerrainHeightTextureHeight,
			D3D11_BIND_SHADER_RESOURCE,
			flat_heights.data(),
			&g_flat_height_texture,
			&g_flat_height_texture_srv,
			nullptr))
	{
		OutputDebugStringA("[TerrainHeightField] Failed to create flat terrain height texture.\n");
		return false;
	}

	return true;
}

bool InitializeTerrainHeightCompute()
{
	SAFE_RELEASE(g_height_constant_buffer);
	SAFE_RELEASE(g_height_readback_texture);
	SAFE_RELEASE(g_height_texture_uav);
	SAFE_RELEASE(g_height_texture_srv);
	SAFE_RELEASE(g_height_texture);
	SAFE_RELEASE(g_height_compute_shader);

	std::ifstream shader_stream("resource/shader/shader_compute_terrain_height.cso", std::ios::binary);
	if (!shader_stream)
	{
		OutputDebugStringA("[TerrainHeightField] Failed to open shader_compute_terrain_height.cso.\n");
		return false;
	}

	const std::vector<char> shader_bytes(
		(std::istreambuf_iterator<char>(shader_stream)),
		std::istreambuf_iterator<char>());
	if (shader_bytes.empty())
	{
		OutputDebugStringA("[TerrainHeightField] shader_compute_terrain_height.cso was empty.\n");
		return false;
	}

	if (FAILED(g_device->CreateComputeShader(shader_bytes.data(), shader_bytes.size(), nullptr, &g_height_compute_shader)))
	{
		OutputDebugStringA("[TerrainHeightField] Failed to create terrain height compute shader.\n");
		SAFE_RELEASE(g_height_compute_shader);
		return false;
	}

	if (!CreateFloatTexture(
			kTerrainHeightTextureWidth,
			kTerrainHeightTextureHeight,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
			nullptr,
			&g_height_texture,
			&g_height_texture_srv,
			&g_height_texture_uav))
	{
		OutputDebugStringA("[TerrainHeightField] Failed to create terrain height UAV texture.\n");
		SAFE_RELEASE(g_height_compute_shader);
		return false;
	}

	D3D11_TEXTURE2D_DESC readback_desc{};
	readback_desc.Width = kTerrainHeightTextureWidth;
	readback_desc.Height = kTerrainHeightTextureHeight;
	readback_desc.MipLevels = 1;
	readback_desc.ArraySize = 1;
	readback_desc.Format = DXGI_FORMAT_R32_FLOAT;
	readback_desc.SampleDesc.Count = 1;
	readback_desc.Usage = D3D11_USAGE_STAGING;
	readback_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	if (FAILED(g_device->CreateTexture2D(&readback_desc, nullptr, &g_height_readback_texture)))
	{
		OutputDebugStringA("[TerrainHeightField] Failed to create terrain height readback texture.\n");
		SAFE_RELEASE(g_height_texture_uav);
		SAFE_RELEASE(g_height_texture_srv);
		SAFE_RELEASE(g_height_texture);
		SAFE_RELEASE(g_height_compute_shader);
		return false;
	}

	D3D11_BUFFER_DESC constant_desc{};
	constant_desc.Usage = D3D11_USAGE_DEFAULT;
	constant_desc.ByteWidth = sizeof(TerrainHeightConstants);
	constant_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (FAILED(g_device->CreateBuffer(&constant_desc, nullptr, &g_height_constant_buffer)))
	{
		OutputDebugStringA("[TerrainHeightField] Failed to create terrain height constant buffer.\n");
		SAFE_RELEASE(g_height_readback_texture);
		SAFE_RELEASE(g_height_texture_uav);
		SAFE_RELEASE(g_height_texture_srv);
		SAFE_RELEASE(g_height_texture);
		SAFE_RELEASE(g_height_compute_shader);
		return false;
	}

	return true;
}

bool UpdateTerrainHeightTexture()
{
	if (g_context == nullptr ||
		g_height_compute_shader == nullptr ||
		g_height_texture_uav == nullptr ||
		g_height_texture == nullptr ||
		g_height_constant_buffer == nullptr)
	{
		return false;
	}

	const float height_cell_size_x = kFieldWidth / static_cast<float>(kTerrainHeightTextureWidth - 1u);
	const float height_cell_size_z = kFieldDepth / static_cast<float>(kTerrainHeightTextureHeight - 1u);
	const TerrainHeightConstants constants = {
		g_terrain_settings.base_frequency,
		g_terrain_settings.base_height,
		g_terrain_settings.detail_frequency,
		g_terrain_settings.detail_height,
		g_terrain_settings.ridge_frequency,
		g_terrain_settings.ridge_height,
		g_terrain_settings.continent_height,
		g_terrain_settings.lake_center_z,
		g_terrain_settings.lake_radius_x,
		g_terrain_settings.lake_radius_z,
		g_terrain_settings.lake_depth,
		kFieldWidth,
		kFieldDepth,
		height_cell_size_x,
		height_cell_size_z,
		kTerrainHeightTextureWidth,
		kTerrainHeightTextureHeight,
		0.0f,
		0.0f,
		0.0f
	};
	g_context->UpdateSubresource(g_height_constant_buffer, 0, nullptr, &constants, 0, 0);

	ID3D11ShaderResourceView* null_vs_srv = nullptr;
	g_context->VSSetShaderResources(0, 1, &null_vs_srv);

	ID3D11UnorderedAccessView* uavs[] = { g_height_texture_uav };
	ID3D11Buffer* constant_buffers[] = { g_height_constant_buffer };

	g_context->CSSetShader(g_height_compute_shader, nullptr, 0);
	g_context->CSSetConstantBuffers(0, 1, constant_buffers);
	g_context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
	g_context->Dispatch(
		(kTerrainHeightTextureWidth + 7) / 8,
		(kTerrainHeightTextureHeight + 7) / 8,
		1);

	ID3D11ShaderResourceView* null_srvs[] = { nullptr };
	ID3D11UnorderedAccessView* null_uav = nullptr;
	ID3D11Buffer* null_cb = nullptr;
	g_context->CSSetShaderResources(0, 1, null_srvs);
	g_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
	g_context->CSSetConstantBuffers(0, 1, &null_cb);
	g_context->CSSetShader(nullptr, nullptr, 0);

	g_context->CopyResource(g_height_readback_texture, g_height_texture);
	D3D11_MAPPED_SUBRESOURCE mapped_resource{};
	if (FAILED(g_context->Map(g_height_readback_texture, 0, D3D11_MAP_READ, 0, &mapped_resource)))
	{
		OutputDebugStringA("[TerrainHeightField] Failed to read back generated terrain height texture.\n");
		return false;
	}

	g_original_heights.assign(kTerrainHeightSampleCount, 0.0f);
	for (unsigned int row = 0; row < kTerrainHeightTextureHeight; ++row)
	{
		const float* source_row = reinterpret_cast<const float*>(
			static_cast<const unsigned char*>(mapped_resource.pData) + mapped_resource.RowPitch * row);
		std::memcpy(
			g_original_heights.data() + static_cast<size_t>(row) * kTerrainHeightTextureWidth,
			source_row,
			sizeof(float) * kTerrainHeightTextureWidth);
	}
	g_context->Unmap(g_height_readback_texture, 0);
	return true;
}
}

bool TerrainHeightField::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	g_device = device;
	g_context = context;
	g_is_flat_height_field = false;

	return CreateFlatHeightTexture() &&
		InitializeTerrainHeightCompute() &&
		UpdateTerrainHeightTexture();
}

void TerrainHeightField::Finalize()
{
	SAFE_RELEASE(g_flat_height_texture_srv);
	SAFE_RELEASE(g_flat_height_texture);
	SAFE_RELEASE(g_height_constant_buffer);
	SAFE_RELEASE(g_height_readback_texture);
	SAFE_RELEASE(g_height_texture_uav);
	SAFE_RELEASE(g_height_texture_srv);
	SAFE_RELEASE(g_height_texture);
	SAFE_RELEASE(g_height_compute_shader);
	g_original_heights.clear();
	g_context = nullptr;
	g_device = nullptr;
}

void TerrainHeightField::SetFlatMode(bool is_flat)
{
	g_is_flat_height_field = is_flat;
}

bool TerrainHeightField::IsFlatMode()
{
	return g_is_flat_height_field;
}

void TerrainHeightField::ApplyTerrainSettings(const TerrainSettings& settings)
{
	if (g_terrain_settings == settings)
	{
		return;
	}

	g_terrain_settings = settings;
	UpdateTerrainHeightTexture();
}

const TerrainSettings& TerrainHeightField::GetTerrainSettings()
{
	return g_terrain_settings;
}

float TerrainHeightField::GetSuggestedWaterHeight()
{
	return std::max(
		30.0f,
		g_terrain_settings.base_height * 0.68f +
		g_terrain_settings.continent_height * 0.32f +
		10.0f);
}

float TerrainHeightField::FieldWidth()
{
	return kFieldWidth;
}

float TerrainHeightField::FieldDepth()
{
	return kFieldDepth;
}

Backend::RenderShaderResource TerrainHeightField::HeightResource()
{
	return Backend::RenderShaderResource(HeightSRV());
}

ID3D11ShaderResourceView* TerrainHeightField::HeightSRV()
{
	if (g_is_flat_height_field || g_height_texture_srv == nullptr)
	{
		return g_flat_height_texture_srv;
	}

	return g_height_texture_srv;
}

float TerrainHeightField::GetHeight(float x, float z)
{
	if (g_is_flat_height_field || g_original_heights.size() != kTerrainHeightSampleCount)
	{
		return 0.0f;
	}

	const float u = std::clamp((x + kFieldWidth * 0.5f) / kFieldWidth, 0.0f, 1.0f);
	const float v = std::clamp((z + kFieldDepth * 0.5f) / kFieldDepth, 0.0f, 1.0f);
	const float sample_x = u * static_cast<float>(kTerrainHeightTextureWidth - 1u);
	const float sample_z = v * static_cast<float>(kTerrainHeightTextureHeight - 1u);

	const unsigned int x0 = static_cast<unsigned int>(std::floor(sample_x));
	const unsigned int z0 = static_cast<unsigned int>(std::floor(sample_z));
	const unsigned int x1 = std::min(x0 + 1u, kTerrainHeightTextureWidth - 1u);
	const unsigned int z1 = std::min(z0 + 1u, kTerrainHeightTextureHeight - 1u);
	const float tx = sample_x - static_cast<float>(x0);
	const float tz = sample_z - static_cast<float>(z0);

	const float h00 = g_original_heights[static_cast<size_t>(x0) + static_cast<size_t>(z0) * kTerrainHeightTextureWidth];
	const float h10 = g_original_heights[static_cast<size_t>(x1) + static_cast<size_t>(z0) * kTerrainHeightTextureWidth];
	const float h01 = g_original_heights[static_cast<size_t>(x0) + static_cast<size_t>(z1) * kTerrainHeightTextureWidth];
	const float h11 = g_original_heights[static_cast<size_t>(x1) + static_cast<size_t>(z1) * kTerrainHeightTextureWidth];

	const float hx0 = std::lerp(h00, h10, tx);
	const float hx1 = std::lerp(h01, h11, tx);
	return std::lerp(hx0, hx1, tz);
}
