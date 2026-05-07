// ----------------------------------------------------
// Procedural mesh field renderer [meshfield.cpp]
// ====================================================
// Created by: Jerry
// Updated : 2026-03-26
// ----------------------------------------------------
#include "meshfield.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <fstream>
#include <vector>

#include <d3d11.h>
#include <DirectXMath.h>

#include "DirectXTex.h"
#include "debug_ostream.h"
#include "direct3d.h"
#include "sampler.h"
#include "shader_field.h"
#include "texture.h"

using namespace DirectX;

namespace
{
constexpr float kFieldMeshWidth = 2.0f;
constexpr float kFieldMeshDepth = 2.0f;
constexpr int kFieldMeshHCount = 256;
constexpr int kFieldMeshVCount = 256;

constexpr int kFieldMeshHVertexCount = kFieldMeshHCount + 1;
constexpr int kFieldMeshVVertexCount = kFieldMeshVCount + 1;

constexpr int kNumVertex = kFieldMeshHVertexCount * kFieldMeshVVertexCount;
constexpr int kNumIndex = 6 * kFieldMeshHCount * kFieldMeshVCount;

struct Vertex3D
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT4 color;
	XMFLOAT2 texcoord;
};

ID3D11Buffer* g_vertex_buffer = nullptr;
ID3D11Buffer* g_index_buffer = nullptr;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;

int g_field_texture_id0 = -1;
int g_field_texture_id1 = -1;
int g_field_texture_id2 = -1;

bool g_is_flat_mesh_field = false;
std::vector<Vertex3D> g_mesh_vertices;
std::vector<unsigned int> g_mesh_indices;
std::vector<float> g_original_heights;
std::vector<float> g_height_map_samples;
ID3D11ComputeShader* g_height_compute_shader = nullptr;
ID3D11Texture2D* g_height_texture = nullptr;
ID3D11ShaderResourceView* g_height_texture_srv = nullptr;
ID3D11UnorderedAccessView* g_height_texture_uav = nullptr;
ID3D11Texture2D* g_height_readback_texture = nullptr;
ID3D11Buffer* g_height_constant_buffer = nullptr;
ID3D11Texture2D* g_authored_height_texture = nullptr;
ID3D11ShaderResourceView* g_authored_height_texture_srv = nullptr;
ID3D11Texture2D* g_flat_height_texture = nullptr;
ID3D11ShaderResourceView* g_flat_height_texture_srv = nullptr;
ID3D11ShaderResourceView* g_render_height_override_srv = nullptr;
TerrainSettings g_terrain_settings{};
size_t g_height_map_width = 0;
size_t g_height_map_height = 0;
bool g_has_height_map = false;

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
	unsigned int has_authored_height_map = 0u;
	unsigned int width = 0u;
	unsigned int height = 0u;
	float padding0 = 0.0f;
	float padding1 = 0.0f;
};

float Hash21(float x, float z)
{
	const float value = std::sin(x * 127.1f + z * 311.7f) * 43758.5453f;
	return value - std::floor(value);
}

float Smoothstep(float value)
{
	return value * value * (3.0f - 2.0f * value);
}

float RemapClamped(float value, float in_min, float in_max)
{
	if (in_max <= in_min)
	{
		return 0.0f;
	}

	return std::clamp((value - in_min) / (in_max - in_min), 0.0f, 1.0f);
}

float Terrace(float value, float steps, float softness)
{
	if (steps <= 1.0f)
	{
		return value;
	}

	const float scaled = value * steps;
	const float base = std::floor(scaled) / steps;
	const float next = std::ceil(scaled) / steps;
	const float local = scaled - std::floor(scaled);
	const float smooth = std::pow(Smoothstep(local), std::max(softness, 0.01f));
	return std::lerp(base, next, smooth);
}

float ValueNoise(float x, float z)
{
	const float floor_x = std::floor(x);
	const float floor_z = std::floor(z);
	const float frac_x = x - floor_x;
	const float frac_z = z - floor_z;

	const float n00 = Hash21(floor_x, floor_z);
	const float n10 = Hash21(floor_x + 1.0f, floor_z);
	const float n01 = Hash21(floor_x, floor_z + 1.0f);
	const float n11 = Hash21(floor_x + 1.0f, floor_z + 1.0f);

	const float sx = Smoothstep(frac_x);
	const float sz = Smoothstep(frac_z);
	const float nx0 = std::lerp(n00, n10, sx);
	const float nx1 = std::lerp(n01, n11, sx);
	return std::lerp(nx0, nx1, sz);
}

float WorleyNoise(float x, float z)
{
	const float cell_x = std::floor(x);
	const float cell_z = std::floor(z);
	float min_distance_sq = FLT_MAX;

	for (int oz = -1; oz <= 1; ++oz)
	{
		for (int ox = -1; ox <= 1; ++ox)
		{
			const float sample_x = cell_x + static_cast<float>(ox);
			const float sample_z = cell_z + static_cast<float>(oz);
			const float point_x = sample_x + Hash21(sample_x + 19.0f, sample_z - 7.0f);
			const float point_z = sample_z + Hash21(sample_x - 11.0f, sample_z + 23.0f);
			const float dx = point_x - x;
			const float dz = point_z - z;
			min_distance_sq = std::min(min_distance_sq, dx * dx + dz * dz);
		}
	}

	return std::sqrt(min_distance_sq);
}

float Fbm(float x, float z, int octaves)
{
	float value = 0.0f;
	float amplitude = 0.5f;
	float frequency = 1.0f;

	for (int octave = 0; octave < octaves; ++octave)
	{
		value += ValueNoise(x * frequency, z * frequency) * amplitude;
		frequency *= 2.0f;
		amplitude *= 0.5f;
	}

	return value;
}

float RidgedFbm(float x, float z, int octaves)
{
	float value = 0.0f;
	float amplitude = 0.5f;
	float frequency = 1.0f;

	for (int octave = 0; octave < octaves; ++octave)
	{
		float sample = ValueNoise(x * frequency, z * frequency);
		sample = 1.0f - std::fabs(sample * 2.0f - 1.0f);
		sample *= sample;
		value += sample * amplitude;
		frequency *= 2.0f;
		amplitude *= 0.5f;
	}

	return value;
}

DirectX::XMFLOAT2 DomainWarp(float x, float z, float frequency, float amplitude, float offset_x, float offset_z)
{
	const float warp_x = Fbm(x * frequency + offset_x, z * frequency + offset_z, 3) - 0.5f;
	const float warp_z = Fbm(x * frequency - offset_z, z * frequency - offset_x, 3) - 0.5f;
	return { warp_x * amplitude, warp_z * amplitude };
}

bool LoadTerrainHeightMap()
{
	DirectX::TexMetadata metadata{};
	DirectX::ScratchImage image;
	HRESULT hr = DirectX::LoadFromWICFile(
		L"resource/texture/height_map.png",
		DirectX::WIC_FLAGS_NONE,
		&metadata,
		image);
	if (FAILED(hr))
	{
		OutputDebugStringA("[MeshField] Failed to load height_map.png. Falling back to procedural terrain.\n");
		g_height_map_samples.clear();
		g_height_map_width = 0;
		g_height_map_height = 0;
		g_has_height_map = false;
		return false;
	}

	const DirectX::Image* source_image = image.GetImage(0, 0, 0);
	DirectX::ScratchImage converted_image;
	if (metadata.format != DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		hr = DirectX::Convert(
			image.GetImages(),
			image.GetImageCount(),
			metadata,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DirectX::TEX_FILTER_DEFAULT,
			0.0f,
			converted_image);
		if (FAILED(hr))
		{
			OutputDebugStringA("[MeshField] Failed to convert height_map.png. Falling back to procedural terrain.\n");
			g_height_map_samples.clear();
			g_height_map_width = 0;
			g_height_map_height = 0;
			g_has_height_map = false;
			return false;
		}

		source_image = converted_image.GetImage(0, 0, 0);
		metadata = converted_image.GetMetadata();
	}

	if (source_image == nullptr || metadata.width == 0 || metadata.height == 0)
	{
		OutputDebugStringA("[MeshField] height_map.png produced no readable image data.\n");
		g_height_map_samples.clear();
		g_height_map_width = 0;
		g_height_map_height = 0;
		g_has_height_map = false;
		return false;
	}

	g_height_map_width = metadata.width;
	g_height_map_height = metadata.height;
	g_height_map_samples.assign(g_height_map_width * g_height_map_height, 0.0f);

	for (size_t y = 0; y < g_height_map_height; ++y)
	{
		const unsigned char* row = source_image->pixels + (source_image->rowPitch * y);
		for (size_t x = 0; x < g_height_map_width; ++x)
		{
			const unsigned char* pixel = row + x * 4;
			const float red = static_cast<float>(pixel[0]) / 255.0f;
			const float green = static_cast<float>(pixel[1]) / 255.0f;
			const float blue = static_cast<float>(pixel[2]) / 255.0f;
			g_height_map_samples[x + y * g_height_map_width] = (red + green + blue) / 3.0f;
		}
	}

	g_has_height_map = true;
	OutputDebugStringA("[MeshField] Loaded hand-authored height map.\n");
	return true;
}

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

	std::vector<float> flat_heights(static_cast<size_t>(kNumVertex), 0.0f);
	if (!CreateFloatTexture(
			static_cast<unsigned int>(kFieldMeshHVertexCount),
			static_cast<unsigned int>(kFieldMeshVVertexCount),
			D3D11_BIND_SHADER_RESOURCE,
			flat_heights.data(),
			&g_flat_height_texture,
			&g_flat_height_texture_srv,
			nullptr))
	{
		OutputDebugStringA("[MeshField] Failed to create flat terrain height texture.\n");
		return false;
	}

	return true;
}

bool CreateAuthoredHeightTexture()
{
	SAFE_RELEASE(g_authored_height_texture_srv);
	SAFE_RELEASE(g_authored_height_texture);

	const unsigned int width = g_has_height_map ? static_cast<unsigned int>(g_height_map_width) : 1u;
	const unsigned int height = g_has_height_map ? static_cast<unsigned int>(g_height_map_height) : 1u;
	std::vector<float> fallback_samples;
	const float* sample_data = nullptr;
	if (g_has_height_map && !g_height_map_samples.empty())
	{
		sample_data = g_height_map_samples.data();
	}
	else
	{
		fallback_samples.assign(1, 0.0f);
		sample_data = fallback_samples.data();
	}

	if (!CreateFloatTexture(
			width,
			height,
			D3D11_BIND_SHADER_RESOURCE,
			sample_data,
			&g_authored_height_texture,
			&g_authored_height_texture_srv,
			nullptr))
	{
		OutputDebugStringA("[MeshField] Failed to create authored terrain height texture.\n");
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
		OutputDebugStringA("[MeshField] Failed to open shader_compute_terrain_height.cso.\n");
		return false;
	}

	const std::vector<char> shader_bytes(
		(std::istreambuf_iterator<char>(shader_stream)),
		std::istreambuf_iterator<char>());
	if (shader_bytes.empty())
	{
		OutputDebugStringA("[MeshField] shader_compute_terrain_height.cso was empty.\n");
		return false;
	}

	if (FAILED(g_device->CreateComputeShader(shader_bytes.data(), shader_bytes.size(), nullptr, &g_height_compute_shader)))
	{
		OutputDebugStringA("[MeshField] Failed to create terrain height compute shader.\n");
		SAFE_RELEASE(g_height_compute_shader);
		return false;
	}

	if (!CreateFloatTexture(
			static_cast<unsigned int>(kFieldMeshHVertexCount),
			static_cast<unsigned int>(kFieldMeshVVertexCount),
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
			nullptr,
			&g_height_texture,
			&g_height_texture_srv,
			&g_height_texture_uav))
	{
		OutputDebugStringA("[MeshField] Failed to create terrain height UAV texture.\n");
		SAFE_RELEASE(g_height_compute_shader);
		return false;
	}

	D3D11_TEXTURE2D_DESC readback_desc{};
	readback_desc.Width = static_cast<unsigned int>(kFieldMeshHVertexCount);
	readback_desc.Height = static_cast<unsigned int>(kFieldMeshVVertexCount);
	readback_desc.MipLevels = 1;
	readback_desc.ArraySize = 1;
	readback_desc.Format = DXGI_FORMAT_R32_FLOAT;
	readback_desc.SampleDesc.Count = 1;
	readback_desc.Usage = D3D11_USAGE_STAGING;
	readback_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	if (FAILED(g_device->CreateTexture2D(&readback_desc, nullptr, &g_height_readback_texture)))
	{
		OutputDebugStringA("[MeshField] Failed to create terrain height readback texture.\n");
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
		OutputDebugStringA("[MeshField] Failed to create terrain height constant buffer.\n");
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

	const float field_width = kFieldMeshHCount * kFieldMeshWidth;
	const float field_depth = kFieldMeshVCount * kFieldMeshDepth;
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
		field_width,
		field_depth,
		kFieldMeshWidth,
		kFieldMeshDepth,
		g_has_height_map ? 1u : 0u,
		static_cast<unsigned int>(kFieldMeshHVertexCount),
		static_cast<unsigned int>(kFieldMeshVVertexCount),
		0.0f,
		0.0f
	};
	g_context->UpdateSubresource(g_height_constant_buffer, 0, nullptr, &constants, 0, 0);

	ID3D11ShaderResourceView* null_vs_srv = nullptr;
	g_context->VSSetShaderResources(0, 1, &null_vs_srv);

	ID3D11ShaderResourceView* authored_srvs[] = { g_authored_height_texture_srv };
	ID3D11SamplerState* samplers[] = { Backend::DX11::Sampler::GetState() };
	ID3D11UnorderedAccessView* uavs[] = { g_height_texture_uav };
	ID3D11Buffer* constant_buffers[] = { g_height_constant_buffer };

	g_context->CSSetShader(g_height_compute_shader, nullptr, 0);
	g_context->CSSetConstantBuffers(0, 1, constant_buffers);
	g_context->CSSetShaderResources(0, 1, authored_srvs);
	g_context->CSSetSamplers(0, 1, samplers);
	g_context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
	g_context->Dispatch(
		(kFieldMeshHVertexCount + 7) / 8,
		(kFieldMeshVVertexCount + 7) / 8,
		1);

	ID3D11ShaderResourceView* null_srvs[] = { nullptr };
	ID3D11UnorderedAccessView* null_uav = nullptr;
	ID3D11Buffer* null_cb = nullptr;
	ID3D11SamplerState* null_sampler = nullptr;
	g_context->CSSetShaderResources(0, 1, null_srvs);
	g_context->CSSetSamplers(0, 1, &null_sampler);
	g_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
	g_context->CSSetConstantBuffers(0, 1, &null_cb);
	g_context->CSSetShader(nullptr, nullptr, 0);

	g_context->CopyResource(g_height_readback_texture, g_height_texture);
	D3D11_MAPPED_SUBRESOURCE mapped_resource{};
	if (FAILED(g_context->Map(g_height_readback_texture, 0, D3D11_MAP_READ, 0, &mapped_resource)))
	{
		OutputDebugStringA("[MeshField] Failed to read back generated terrain height texture.\n");
		return false;
	}

	g_original_heights.assign(static_cast<size_t>(kNumVertex), 0.0f);
	for (int row = 0; row < kFieldMeshVVertexCount; ++row)
	{
		const float* source_row = reinterpret_cast<const float*>(
			static_cast<const unsigned char*>(mapped_resource.pData) + mapped_resource.RowPitch * row);
		std::memcpy(
			g_original_heights.data() + static_cast<size_t>(row) * kFieldMeshHVertexCount,
			source_row,
			sizeof(float) * kFieldMeshHVertexCount);
	}
	g_context->Unmap(g_height_readback_texture, 0);
	return true;
}

float SampleHeightMap01(float world_x, float world_z)
{
	if (!g_has_height_map || g_height_map_width < 2 || g_height_map_height < 2)
	{
		return 0.0f;
	}

	const float field_width = kFieldMeshHCount * kFieldMeshWidth;
	const float field_depth = kFieldMeshVCount * kFieldMeshDepth;
	const float u = std::clamp((world_x + field_width * 0.5f) / field_width, 0.0f, 1.0f);
	const float v = std::clamp((world_z + field_depth * 0.5f) / field_depth, 0.0f, 1.0f);

	const float sample_x = u * static_cast<float>(g_height_map_width - 1);
	const float sample_y = v * static_cast<float>(g_height_map_height - 1);
	const int x0 = static_cast<int>(std::floor(sample_x));
	const int y0 = static_cast<int>(std::floor(sample_y));
	const int x1 = std::min(x0 + 1, static_cast<int>(g_height_map_width - 1));
	const int y1 = std::min(y0 + 1, static_cast<int>(g_height_map_height - 1));
	const float tx = sample_x - static_cast<float>(x0);
	const float ty = sample_y - static_cast<float>(y0);

	const float h00 = g_height_map_samples[static_cast<size_t>(x0) + static_cast<size_t>(y0) * g_height_map_width];
	const float h10 = g_height_map_samples[static_cast<size_t>(x1) + static_cast<size_t>(y0) * g_height_map_width];
	const float h01 = g_height_map_samples[static_cast<size_t>(x0) + static_cast<size_t>(y1) * g_height_map_width];
	const float h11 = g_height_map_samples[static_cast<size_t>(x1) + static_cast<size_t>(y1) * g_height_map_width];

	const float hx0 = std::lerp(h00, h10, tx);
	const float hx1 = std::lerp(h01, h11, tx);
	return std::lerp(hx0, hx1, ty);
}

float GenerateTerrainHeight(float world_x, float world_z)
{
	const float nx = world_x * g_terrain_settings.base_frequency;
	const float nz = world_z * g_terrain_settings.base_frequency;
	const XMFLOAT2 coarse_warp = DomainWarp(nx, nz, 0.65f, 1.35f, 13.0f, -17.0f);
	const XMFLOAT2 fine_warp = DomainWarp(nx + coarse_warp.x, nz + coarse_warp.y, 1.55f, 0.45f, -29.0f, 7.0f);
	const float dx = nx + coarse_warp.x + fine_warp.x;
	const float dz = nz + coarse_warp.y + fine_warp.y;

	// #0 Base worley noise
	const float worley_base = 1.0f - std::clamp(WorleyNoise(dx * 2.8f, dz * 2.8f), 0.0f, 1.0f);

	// #1 Random steep weight
	const float steep_weight = std::lerp(
		1.10f,
		3.05f,
		ValueNoise(std::floor(dx * 1.65f) + 19.0f, std::floor(dz * 1.65f) - 23.0f));
	const float weighted_mass = std::pow(std::clamp(worley_base, 0.0f, 1.0f), steep_weight);

	// #2 Coord dissolution
	const float dissolution = Fbm(dx * 1.75f + 23.0f, dz * 1.75f - 11.0f, 4);
	const float dissolved_mass = weighted_mass * std::lerp(0.42f, 1.26f, dissolution);

	// #3 Fractal simplex noise (approximated here with value-noise fbm)
	const float fractal_simplex = Fbm(
		dx * g_terrain_settings.detail_frequency * 0.95f + 31.0f,
		dz * g_terrain_settings.detail_frequency * 0.95f - 17.0f,
		5);

	// #4 Rift
	const float rift_source = RidgedFbm(dx * 0.42f - 17.0f, dz * 0.36f + 29.0f, 4);
	const float rift_mask = std::pow(1.0f - std::clamp(rift_source, 0.0f, 1.0f), 1.65f);

	// #5 Ridge
	const float ridge_base = std::clamp(RidgedFbm(
			dx * g_terrain_settings.ridge_frequency + 7.0f,
			dz * g_terrain_settings.ridge_frequency - 13.0f,
			5), 0.0f, 1.0f);
	const float ridge_mask = std::pow(ridge_base, 2.25f);

	// #6 Global trend
	constexpr float kRangeCenterAX = -95.0f;
	constexpr float kRangeCenterAZ = 26.0f;
	constexpr float kRangeCenterBX = 82.0f;
	constexpr float kRangeCenterBZ = 118.0f;
	const float range_a =
		1.0f - std::clamp(std::sqrt(
			std::pow((world_x - kRangeCenterAX) / 288.0f, 2.0f) +
			std::pow((world_z - kRangeCenterAZ) / 228.0f, 2.0f)), 0.0f, 1.0f);
	const float range_b =
		1.0f - std::clamp(std::sqrt(
			std::pow((world_x - kRangeCenterBX) / 268.0f, 2.0f) +
			std::pow((world_z - kRangeCenterBZ) / 214.0f, 2.0f)), 0.0f, 1.0f);
	float global_trend = std::max(std::pow(range_a, 2.35f), std::pow(range_b, 2.85f));
	float authored_macro = 0.0f;

	if (g_has_height_map)
	{
		authored_macro = std::pow(std::clamp(SampleHeightMap01(world_x, world_z), 0.0f, 1.0f), 1.35f);
		global_trend = std::max(global_trend, authored_macro);
	}
	const float authored_foothills = Smoothstep(RemapClamped(authored_macro, 0.24f, 0.78f));
	const float authored_summit = std::pow(RemapClamped(authored_macro, 0.58f, 1.0f), 1.45f);
	const float massif_macro = std::max(global_trend, authored_foothills * 0.92f);

	const float rolling_base = Fbm(world_x * 0.0045f + 7.0f, world_z * 0.0045f - 5.0f, 4);
	const float macro_warp_detail = Fbm(world_x * 0.010f - 9.0f, world_z * 0.010f + 14.0f, 3) - 0.5f;

	const float basin_x = world_x / g_terrain_settings.lake_radius_x;
	const float basin_z = (world_z - g_terrain_settings.lake_center_z) / g_terrain_settings.lake_radius_z;
	const float basin_distance = std::sqrt(basin_x * basin_x + basin_z * basin_z);
	const float lake_basin = std::clamp(1.0f - basin_distance, 0.0f, 1.0f);

	constexpr float kHeroPeakX = 46.0f;
	constexpr float kHeroPeakZ = 118.0f;
	constexpr float kSpineStartX = -68.0f;
	constexpr float kSpineStartZ = 48.0f;
	constexpr float kSpineEndX = 128.0f;
	constexpr float kSpineEndZ = 136.0f;
	const XMFLOAT2 spine_start = { kSpineStartX, kSpineStartZ };
	const XMFLOAT2 spine_end = { kSpineEndX, kSpineEndZ };
	const XMFLOAT2 spine = { spine_end.x - spine_start.x, spine_end.y - spine_start.y };
	const float spine_length_sq = std::max(spine.x * spine.x + spine.y * spine.y, 1.0f);
	const float spine_length = std::sqrt(spine_length_sq);
	const XMFLOAT2 spine_dir = { spine.x / spine_length, spine.y / spine_length };
	const XMFLOAT2 spine_normal = { -spine_dir.y, spine_dir.x };
	const XMFLOAT2 world_pos = { world_x, world_z };
	const float spine_t = std::clamp(
		((world_pos.x - spine_start.x) * spine.x + (world_pos.y - spine_start.y) * spine.y) / spine_length_sq,
		0.0f,
		1.0f);
	const XMFLOAT2 nearest_spine = {
		spine_start.x + spine.x * spine_t,
		spine_start.y + spine.y * spine_t
	};
	const float spine_distance = std::sqrt(
		(world_pos.x - nearest_spine.x) * (world_pos.x - nearest_spine.x) +
		(world_pos.y - nearest_spine.y) * (world_pos.y - nearest_spine.y));
	const float spine_progress = Smoothstep(spine_t);
	const float spine_width = std::lerp(148.0f, 86.0f, spine_progress);
	const float spine_shoulder_mask = std::pow(std::clamp(1.0f - spine_distance / spine_width, 0.0f, 1.0f), 1.42f);
	const float spine_core_width = spine_width * std::lerp(0.54f, 0.40f, spine_progress);
	const float spine_core_mask = std::pow(std::clamp(1.0f - spine_distance / spine_core_width, 0.0f, 1.0f), 2.85f);
	const float mountain_spine_mask = std::max(
		spine_shoulder_mask * std::lerp(0.78f, 1.08f, spine_progress),
		spine_core_mask * std::lerp(0.22f, 1.12f, std::pow(RemapClamped(spine_progress, 0.35f, 1.0f), 1.45f)));

	const float hero_parallel = (world_x - kHeroPeakX) * spine_dir.x + (world_z - kHeroPeakZ) * spine_dir.y;
	const float hero_perpendicular = (world_x - kHeroPeakX) * spine_normal.x + (world_z - kHeroPeakZ) * spine_normal.y;
	const float hero_shoulder_distance = std::sqrt(
		std::pow(hero_parallel / 170.0f, 2.0f) +
		std::pow(hero_perpendicular / 96.0f, 2.0f));
	const float hero_peak_mask = std::pow(std::clamp(1.0f - hero_shoulder_distance, 0.0f, 1.0f), 2.10f);
	const float hero_core_distance = std::sqrt(
		std::pow(hero_parallel / 102.0f, 2.0f) +
		std::pow(hero_perpendicular / 58.0f, 2.0f));
	const float hero_peak_core = std::pow(std::clamp(1.0f - hero_core_distance, 0.0f, 1.0f), 3.40f);
	const float summit_cluster = std::max(std::max(hero_peak_mask, hero_peak_core), mountain_spine_mask);

	const float massif_body = std::pow(std::clamp(dissolved_mass * massif_macro, 0.0f, 1.0f), 1.18f);
	const float massif_peak = std::pow(
		std::clamp(dissolved_mass * std::max(massif_macro, authored_summit * 0.85f), 0.0f, 1.0f),
		5.45f);
	const float shoulder_noise =
		(fractal_simplex - 0.5f) *
		std::lerp(0.04f, 0.62f, massif_macro) *
		std::lerp(1.0f, 0.55f, hero_peak_core);
	const float summit_variation = (ValueNoise(world_x * 0.016f + 31.0f, world_z * 0.016f - 13.0f) - 0.5f) * massif_peak;
	const float ridge_focus = std::pow(massif_macro, 1.30f);
	const float alpine_uplift = std::pow(std::clamp(massif_macro, 0.0f, 1.0f), 2.55f);
	const float hero_uplift = std::pow(std::max(hero_peak_core, summit_cluster * 0.82f), 1.10f);

	float height = (rolling_base - 0.5f) * (g_terrain_settings.base_height * 0.10f);
	height += macro_warp_detail * g_terrain_settings.detail_height * 0.18f;
	height += massif_body * (g_terrain_settings.base_height * 2.00f);
	height += (fractal_simplex - 0.5f) * g_terrain_settings.detail_height * std::lerp(0.05f, 0.68f, massif_macro);
	height += ridge_mask * ridge_focus * (g_terrain_settings.ridge_height * 1.38f);
	height -= rift_mask * ridge_focus * (g_terrain_settings.ridge_height * 0.88f);
	height += massif_peak * (g_terrain_settings.ridge_height * 2.20f);
	height += alpine_uplift * (g_terrain_settings.base_height * 0.88f);
	height += spine_shoulder_mask * std::lerp(0.55f, 0.92f, spine_progress) * (g_terrain_settings.base_height * 1.05f);
	height += spine_core_mask * std::lerp(0.15f, 1.00f, spine_progress) * (g_terrain_settings.ridge_height * 1.10f);
	height += hero_peak_mask * (g_terrain_settings.ridge_height * 1.55f);
	height += hero_peak_core * ((g_terrain_settings.ridge_height * 1.70f) + (g_terrain_settings.base_height * 0.35f));
	height += hero_uplift * ridge_mask * (g_terrain_settings.ridge_height * 1.05f);
	height += shoulder_noise * g_terrain_settings.detail_height * 0.72f;
	height += summit_variation * g_terrain_settings.detail_height * 0.68f;
	height -= lake_basin * g_terrain_settings.lake_depth;
	height += massif_macro * g_terrain_settings.continent_height * 0.90f;

	return height;
}

void RecalculateNormals()
{
	for (int z = 0; z < kFieldMeshVVertexCount; ++z)
	{
		for (int x = 0; x < kFieldMeshHVertexCount; ++x)
		{
			const int index = x + kFieldMeshHVertexCount * z;
			const float center_height = g_mesh_vertices[index].position.y;

			const float left_height =
				(x > 0) ? g_mesh_vertices[index - 1].position.y : center_height;
			const float right_height =
				(x < kFieldMeshHVertexCount - 1) ? g_mesh_vertices[index + 1].position.y : center_height;
			const float up_height =
				(z > 0) ? g_mesh_vertices[index - kFieldMeshHVertexCount].position.y : center_height;
			const float down_height =
				(z < kFieldMeshVVertexCount - 1) ? g_mesh_vertices[index + kFieldMeshHVertexCount].position.y : center_height;

			const XMFLOAT3 tangent = {kFieldMeshWidth * 2.0f, right_height - left_height, 0.0f};
			const XMFLOAT3 bitangent = {0.0f, down_height - up_height, kFieldMeshDepth * 2.0f};

			const XMVECTOR tangent_vector = XMLoadFloat3(&tangent);
			const XMVECTOR bitangent_vector = XMLoadFloat3(&bitangent);
			const XMVECTOR normal_vector = XMVector3Normalize(XMVector3Cross(bitangent_vector, tangent_vector));
			XMStoreFloat3(&g_mesh_vertices[index].normal, normal_vector);
		}
	}
}

void SmoothHeightField(int passes)
{
	if (g_mesh_vertices.empty() || passes <= 0)
	{
		return;
	}

	std::vector<float> heights(g_mesh_vertices.size());
	std::vector<float> smoothed(g_mesh_vertices.size());

	for (size_t i = 0; i < g_mesh_vertices.size(); ++i)
	{
		heights[i] = g_mesh_vertices[i].position.y;
	}

	for (int pass = 0; pass < passes; ++pass)
	{
		for (int z = 0; z < kFieldMeshVVertexCount; ++z)
		{
			for (int x = 0; x < kFieldMeshHVertexCount; ++x)
			{
				float sum = 0.0f;
				float weight_sum = 0.0f;

				for (int oz = -1; oz <= 1; ++oz)
				{
					for (int ox = -1; ox <= 1; ++ox)
					{
						const int sx = std::clamp(x + ox, 0, kFieldMeshHVertexCount - 1);
						const int sz = std::clamp(z + oz, 0, kFieldMeshVVertexCount - 1);
						const int sample_index = sx + kFieldMeshHVertexCount * sz;
						const float weight = (ox == 0 && oz == 0) ? 4.0f : ((ox == 0 || oz == 0) ? 2.0f : 1.0f);
						sum += heights[sample_index] * weight;
						weight_sum += weight;
					}
				}

				const int index = x + kFieldMeshHVertexCount * z;
				smoothed[index] = sum / weight_sum;
			}
		}

		heights.swap(smoothed);
	}

	for (size_t i = 0; i < g_mesh_vertices.size(); ++i)
	{
		g_mesh_vertices[i].position.y = heights[i];
		g_original_heights[i] = heights[i];
	}
}

void BuildProceduralTerrain()
{
	g_mesh_vertices.resize(kNumVertex);
	g_mesh_indices.resize(kNumIndex);
	g_original_heights.assign(kNumVertex, 0.0f);

	for (int z = 0; z < kFieldMeshVVertexCount; ++z)
	{
		for (int x = 0; x < kFieldMeshHVertexCount; ++x)
		{
			const int index = x + kFieldMeshHVertexCount * z;
			const float local_x = static_cast<float>(x) * kFieldMeshWidth;
			const float local_z = static_cast<float>(z) * kFieldMeshDepth;

			g_mesh_vertices[index].position = {local_x, 0.0f, local_z};
			g_mesh_vertices[index].normal = {0.0f, 1.0f, 0.0f};
			g_mesh_vertices[index].color = {1.0f, 1.0f, 1.0f, 1.0f};
			g_mesh_vertices[index].texcoord = {
				static_cast<float>(x) / static_cast<float>(kFieldMeshHCount),
				static_cast<float>(z) / static_cast<float>(kFieldMeshVCount)};
		}
	}

	int index = 0;
	for (int v = 0; v < kFieldMeshVCount; ++v)
	{
		for (int h = 0; h < kFieldMeshHCount; ++h)
		{
			g_mesh_indices[index + 0] = h + (v + 0) * kFieldMeshHVertexCount;
			g_mesh_indices[index + 1] = h + (v + 1) * kFieldMeshHVertexCount + 1;
			g_mesh_indices[index + 2] = g_mesh_indices[index + 0] + 1;
			g_mesh_indices[index + 3] = g_mesh_indices[index + 0];
			g_mesh_indices[index + 4] = g_mesh_indices[index + 1] - 1;
			g_mesh_indices[index + 5] = g_mesh_indices[index + 1];
			index += 6;
		}
	}
}

void RebuildGpuBuffers()
{
	SAFE_RELEASE(g_vertex_buffer);
	SAFE_RELEASE(g_index_buffer);

	D3D11_BUFFER_DESC vertex_desc{};
	vertex_desc.Usage = D3D11_USAGE_DYNAMIC;
	vertex_desc.ByteWidth = static_cast<UINT>(sizeof(Vertex3D) * g_mesh_vertices.size());
	vertex_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA vertex_data{};
	vertex_data.pSysMem = g_mesh_vertices.data();
	g_device->CreateBuffer(&vertex_desc, &vertex_data, &g_vertex_buffer);

	D3D11_BUFFER_DESC index_desc{};
	index_desc.Usage = D3D11_USAGE_DEFAULT;
	index_desc.ByteWidth = static_cast<UINT>(sizeof(unsigned int) * g_mesh_indices.size());
	index_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA index_data{};
	index_data.pSysMem = g_mesh_indices.data();
	g_device->CreateBuffer(&index_desc, &index_data, &g_index_buffer);
}

void RebuildHeightTexture()
{
	UpdateTerrainHeightTexture();
}
}

void MeshFieldRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	g_device = device;
	g_context = context;

	LoadTerrainHeightMap();
	CreateAuthoredHeightTexture();
	CreateFlatHeightTexture();
	InitializeTerrainHeightCompute();
	BuildProceduralTerrain();
	RebuildGpuBuffers();
	RebuildHeightTexture();

	g_field_texture_id0 = TextureManager::Load(L"resource/texture/rock.png");
	g_field_texture_id1 = TextureManager::Load(L"resource/texture/stone_floor.png");
	g_field_texture_id2 = TextureManager::Load(L"resource/texture/grass.png");
	ShaderField_Initialize(g_device, g_context);
}

void MeshFieldRenderer::SetFlatMode(bool is_flat)
{
	if (g_is_flat_mesh_field == is_flat)
	{
		return;
	}

	g_is_flat_mesh_field = is_flat;
}

void MeshFieldRenderer::Finalize()
{
	ShaderField_Finalize();
	SAFE_RELEASE(g_vertex_buffer);
	SAFE_RELEASE(g_index_buffer);
	SAFE_RELEASE(g_flat_height_texture_srv);
	SAFE_RELEASE(g_flat_height_texture);
	SAFE_RELEASE(g_authored_height_texture_srv);
	SAFE_RELEASE(g_authored_height_texture);
	SAFE_RELEASE(g_height_constant_buffer);
	SAFE_RELEASE(g_height_readback_texture);
	SAFE_RELEASE(g_height_texture_uav);
	SAFE_RELEASE(g_height_texture_srv);
	SAFE_RELEASE(g_height_texture);
	SAFE_RELEASE(g_height_compute_shader);
	g_render_height_override_srv = nullptr;
}

void MeshFieldRenderer::Draw()
{
	ShaderField_Begin();
	Backend::DX11::Sampler::SetAnisotropicFilter();
	ShaderField_SetHeightMap(g_render_height_override_srv != nullptr ? g_render_height_override_srv : HeightSRV());

	TextureManager::SetTexture(g_field_texture_id0, 0);
	TextureManager::SetTexture(g_field_texture_id1, 1);
	TextureManager::SetTexture(g_field_texture_id2, 3);

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &g_vertex_buffer, &stride, &offset);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R32_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const float offset_x = kFieldMeshHCount * kFieldMeshWidth * 0.5f;
	const float offset_z = kFieldMeshVCount * kFieldMeshDepth * 0.5f;
	ShaderField_SetWorldMatrix(XMMatrixTranslation(-offset_x, 0.0f, -offset_z));
	ShaderField_SetMaterialColor({1.0f, 1.0f, 1.0f, 1.0f});

	g_context->DrawIndexed(static_cast<UINT>(g_mesh_indices.size()), 0, 0);
	ShaderField_SetHeightMap(nullptr);
	ShaderField_SetTerrainClassificationMap(nullptr);
}

void MeshFieldRenderer::DrawMeshOnly()
{
	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &g_vertex_buffer, &stride, &offset);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R32_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->DrawIndexed(static_cast<UINT>(g_mesh_indices.size()), 0, 0);
}

float MeshFieldRenderer::GetHeight(float x, float z)
{
	if (g_is_flat_mesh_field || g_original_heights.size() != static_cast<size_t>(kNumVertex))
	{
		return 0.0f;
	}

	const float width = kFieldMeshHCount * kFieldMeshWidth;
	const float depth = kFieldMeshVCount * kFieldMeshDepth;
	const float local_x = x + (width * 0.5f);
	const float local_z = z + (depth * 0.5f);

	const int grid_x = static_cast<int>(local_x / kFieldMeshWidth);
	const int grid_z = static_cast<int>(local_z / kFieldMeshDepth);

	if (grid_x < 0 || grid_x >= kFieldMeshHCount || grid_z < 0 || grid_z >= kFieldMeshVCount)
	{
		return 0.0f;
	}

	const int idx_tl = grid_x + kFieldMeshHVertexCount * grid_z;
	const int idx_tr = (grid_x + 1) + kFieldMeshHVertexCount * grid_z;
	const int idx_bl = grid_x + kFieldMeshHVertexCount * (grid_z + 1);
	const int idx_br = (grid_x + 1) + kFieldMeshHVertexCount * (grid_z + 1);

	const float y_tl = g_original_heights[static_cast<size_t>(idx_tl)];
	const float y_tr = g_original_heights[static_cast<size_t>(idx_tr)];
	const float y_bl = g_original_heights[static_cast<size_t>(idx_bl)];
	const float y_br = g_original_heights[static_cast<size_t>(idx_br)];

	const float ratio_x = (local_x - static_cast<float>(grid_x) * kFieldMeshWidth) / kFieldMeshWidth;
	const float ratio_z = (local_z - static_cast<float>(grid_z) * kFieldMeshDepth) / kFieldMeshDepth;

	if (ratio_x + ratio_z <= 1.0f)
	{
		return y_tl + (y_tr - y_tl) * ratio_x + (y_bl - y_tl) * ratio_z;
	}

	return y_br + (y_bl - y_br) * (1.0f - ratio_x) + (y_tr - y_br) * (1.0f - ratio_z);
}

void MeshFieldRenderer::ApplyTerrainSettings(const TerrainSettings& settings)
{
	if (g_terrain_settings == settings)
	{
		return;
	}

	g_terrain_settings = settings;
	RebuildHeightTexture();
}

const TerrainSettings& MeshFieldRenderer::GetTerrainSettings()
{
	return g_terrain_settings;
}

float MeshFieldRenderer::GetSuggestedWaterHeight()
{
	return std::max(0.0f, g_terrain_settings.base_height - g_terrain_settings.lake_depth - 2.4f);
}

float MeshFieldRenderer::FieldWidth()
{
	return kFieldMeshHCount * kFieldMeshWidth;
}

float MeshFieldRenderer::FieldDepth()
{
	return kFieldMeshVCount * kFieldMeshDepth;
}

Backend::RenderShaderResource MeshFieldRenderer::HeightResource()
{
	return Backend::RenderShaderResource(HeightSRV());
}

ID3D11ShaderResourceView* MeshFieldRenderer::HeightSRV()
{
	if (g_is_flat_mesh_field || g_height_texture_srv == nullptr)
	{
		return g_flat_height_texture_srv;
	}

	return g_height_texture_srv;
}

void MeshFieldRenderer::SetRenderHeightSRV(ID3D11ShaderResourceView* srv)
{
	g_render_height_override_srv = srv;
}

void MeshField_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	MeshFieldRenderer::Initialize(pDevice, pContext);
}

void MeshField_Finalize(void)
{
	MeshFieldRenderer::Finalize();
}

void MeshField_Draw()
{
	MeshFieldRenderer::Draw();
}

void MeshField_SetFlatMode(bool isFlat)
{
	MeshFieldRenderer::SetFlatMode(isFlat);
}

void MeshField_DrawMeshOnly()
{
	MeshFieldRenderer::DrawMeshOnly();
}

float MeshField_GetHeight(float x, float z)
{
	return MeshFieldRenderer::GetHeight(x, z);
}
