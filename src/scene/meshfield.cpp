// ----------------------------------------------------
// Procedural mesh field renderer [meshfield.cpp]
// ====================================================
// Created by: Jerry
// Updated : 2026-03-26
// ----------------------------------------------------
#include "meshfield.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "camera.h"
#include <d3d11.h>
#include <DirectXMath.h>

#include "compute_texture_dimensions.h"
#include "terrain_height_field.h"
#include "direct3d.h"
#include "sampler.h"
#include "shader_field.h"
#include "texture.h"

using namespace DirectX;

namespace
{
struct TerrainClipmapConfig
{
	int resolution;
	float interval;
};

constexpr std::array<TerrainClipmapConfig, 1> kTerrainClipmapConfigs = {
	TerrainClipmapConfig{
		static_cast<int>(ComputeTextureDimensions::kTerrainMeshResolution),
		ComputeTextureDimensions::kTerrainMeshInterval},
};

struct Vertex3D
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT4 color;
	XMFLOAT2 texcoord;
};

struct TerrainClipmapLevel
{
	ID3D11Buffer* vertex_buffer = nullptr;
	ID3D11Buffer* index_buffer = nullptr;
	UINT index_count = 0;
	int resolution = 0;
	float interval = 1.0f;
	float side_length = 1.0f;
	float inner_side_length = 0.0f;
};

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;

int g_field_texture_id0 = -1;
int g_field_texture_id1 = -1;
int g_field_texture_id2 = -1;

std::array<TerrainClipmapLevel, kTerrainClipmapConfigs.size()> g_terrain_clipmaps{};
ID3D11ShaderResourceView* g_render_height_override_srv = nullptr;
ID3D11ShaderResourceView* g_render_normal_override_srv = nullptr;

void BuildTerrainClipmapMesh(
	int resolution,
	float interval,
	float inner_side_length,
	std::vector<Vertex3D>& vertices,
	std::vector<unsigned int>& indices)
{
	const int mesh_h_vertex_count = resolution;
	const int mesh_v_vertex_count = resolution;
	const int mesh_h_count = mesh_h_vertex_count - 1;
	const int mesh_v_count = mesh_v_vertex_count - 1;
	const float side_length = interval * static_cast<float>(mesh_h_vertex_count);
	const float half_side_length = side_length * 0.5f;
	const float inner_half_extent = inner_side_length * 0.5f;

	vertices.resize(mesh_h_vertex_count * mesh_v_vertex_count);
	indices.clear();
	indices.reserve(6u * static_cast<size_t>(mesh_h_count) * static_cast<size_t>(mesh_v_count));

	for (int z = 0; z < mesh_v_vertex_count; ++z)
	{
		for (int x = 0; x < mesh_h_vertex_count; ++x)
		{
			const int index = x + mesh_h_vertex_count * z;
			const float local_x = (static_cast<float>(z) - static_cast<float>(mesh_v_vertex_count) * 0.5f) * interval;
			const float local_z = (static_cast<float>(x) - static_cast<float>(mesh_h_vertex_count) * 0.5f) * interval;

			vertices[index].position = {local_x, 0.0f, local_z};
			vertices[index].normal = {0.0f, 1.0f, 0.0f};
			vertices[index].color = {1.0f, 1.0f, 1.0f, 1.0f};
			vertices[index].texcoord = {
				static_cast<float>(x) / static_cast<float>(std::max(mesh_h_count, 1)),
				static_cast<float>(z) / static_cast<float>(std::max(mesh_v_count, 1))};
		}
	}

	for (int v = 0; v < mesh_v_count; ++v)
	{
		for (int h = 0; h < mesh_h_count; ++h)
		{
			const float center_x = (static_cast<float>(h) + 0.5f) * interval - half_side_length;
			const float center_z = (static_cast<float>(v) + 0.5f) * interval - half_side_length;
			const bool inside_inner_hole =
				inner_half_extent > 0.0f &&
				std::abs(center_x) < inner_half_extent &&
				std::abs(center_z) < inner_half_extent;
			if (inside_inner_hole)
			{
				continue;
			}

			const unsigned int i0 = static_cast<unsigned int>(h + (v + 0) * mesh_h_vertex_count);
			const unsigned int i1 = static_cast<unsigned int>(h + (v + 1) * mesh_h_vertex_count + 1);
			indices.push_back(i0);
			indices.push_back(i0 + 1);
			indices.push_back(i1);
			indices.push_back(i0);
			indices.push_back(i1);
			indices.push_back(i1 - 1);
		}
	}
}

void ReleaseTerrainClipmapLevel(TerrainClipmapLevel& level)
{
	SAFE_RELEASE(level.vertex_buffer);
	SAFE_RELEASE(level.index_buffer);
	level.index_count = 0;
}

void RebuildGpuBuffers(
	TerrainClipmapLevel& level,
	const std::vector<Vertex3D>& vertices,
	const std::vector<unsigned int>& indices)
{
	ReleaseTerrainClipmapLevel(level);

	if (vertices.empty() || indices.empty())
	{
		return;
	}

	D3D11_BUFFER_DESC vertex_desc{};
	vertex_desc.Usage = D3D11_USAGE_DEFAULT;
	vertex_desc.ByteWidth = static_cast<UINT>(sizeof(Vertex3D) * vertices.size());
	vertex_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertex_data{};
	vertex_data.pSysMem = vertices.data();
	g_device->CreateBuffer(&vertex_desc, &vertex_data, &level.vertex_buffer);

	D3D11_BUFFER_DESC index_desc{};
	index_desc.Usage = D3D11_USAGE_DEFAULT;
	index_desc.ByteWidth = static_cast<UINT>(sizeof(unsigned int) * indices.size());
	index_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA index_data{};
	index_data.pSysMem = indices.data();
	g_device->CreateBuffer(&index_desc, &index_data, &level.index_buffer);
	level.index_count = static_cast<UINT>(indices.size());
}

void BuildProceduralTerrain()
{
	std::vector<Vertex3D> mesh_vertices;
	std::vector<unsigned int> mesh_indices;
	float previous_side_length = 0.0f;

	for (size_t level_index = 0; level_index < g_terrain_clipmaps.size(); ++level_index)
	{
		TerrainClipmapLevel& level = g_terrain_clipmaps[level_index];
		level.resolution = kTerrainClipmapConfigs[level_index].resolution;
		level.interval = kTerrainClipmapConfigs[level_index].interval;
		level.side_length = level.interval * static_cast<float>(level.resolution);
		level.inner_side_length = previous_side_length;

		BuildTerrainClipmapMesh(
			level.resolution,
			level.interval,
			level.inner_side_length,
			mesh_vertices,
			mesh_indices);
		RebuildGpuBuffers(level, mesh_vertices, mesh_indices);

		previous_side_length = level.side_length;
	}
}

float SnapToInterval(float value, float interval)
{
	return std::floor(value / interval) * interval;
}

void DrawTerrainClipmapLevel(const TerrainClipmapLevel& level, const XMFLOAT3& camera_position)
{
	if (level.vertex_buffer == nullptr || level.index_buffer == nullptr || level.index_count == 0)
	{
		return;
	}

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &level.vertex_buffer, &stride, &offset);
	g_context->IASetIndexBuffer(level.index_buffer, DXGI_FORMAT_R32_UINT, 0);

	const float snapped_center_x = SnapToInterval(camera_position.x, level.interval);
	const float snapped_center_z = SnapToInterval(camera_position.z, level.interval);
	ShaderField_SetWorldMatrix(XMMatrixTranslation(snapped_center_x, 0.0f, snapped_center_z));

	g_context->DrawIndexed(level.index_count, 0, 0);
}
}

void MeshFieldRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	g_device = device;
	g_context = context;

	if (!TerrainHeightField::Initialize(g_device, g_context))
	{
		OutputDebugStringA("[MeshField] Failed to initialize terrain height field.\n");
	}
	BuildProceduralTerrain();

	g_field_texture_id0 = TextureManager::Load(L"resource/texture/stone_floor.png");
	g_field_texture_id1 = TextureManager::Load(L"resource/texture/stone_floor.png");
	g_field_texture_id2 = TextureManager::Load(L"resource/texture/grass.png");
	ShaderField_Initialize(g_device, g_context);
}

void MeshFieldRenderer::SetFlatMode(bool is_flat)
{
	if (TerrainHeightField::IsFlatMode() == is_flat)
	{
		return;
	}

	TerrainHeightField::SetFlatMode(is_flat);
}

void MeshFieldRenderer::Finalize()
{
	ShaderField_Finalize();
	for (TerrainClipmapLevel& level : g_terrain_clipmaps)
	{
		ReleaseTerrainClipmapLevel(level);
	}
	TerrainHeightField::Finalize();
	g_render_height_override_srv = nullptr;
	g_render_normal_override_srv = nullptr;
}

void MeshFieldRenderer::Draw()
{
	ShaderField_Begin();
	Backend::DX11::Sampler::SetAnisotropicFilter();
	ShaderField_SetHeightMap(g_render_height_override_srv != nullptr ? g_render_height_override_srv : HeightSRV());
	ShaderField_SetTerrainNormalMap(g_render_normal_override_srv);

	TextureManager::SetTexture(g_field_texture_id0, 0);
	TextureManager::SetTexture(g_field_texture_id1, 1);
	TextureManager::SetTexture(g_field_texture_id2, 3);

	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ShaderField_SetMaterialColor({1.0f, 1.0f, 1.0f, 1.0f});

	const XMFLOAT3 camera_position = Camera_GetPosition();
	for (auto level_it = g_terrain_clipmaps.rbegin(); level_it != g_terrain_clipmaps.rend(); ++level_it)
	{
		DrawTerrainClipmapLevel(*level_it, camera_position);
	}

	ShaderField_SetHeightMap(nullptr);
	ShaderField_SetTerrainNormalMap(nullptr);
	ShaderField_SetTerrainVegetationSuitabilityMap(nullptr);
	ShaderField_SetTerrainSurfaceDataMap(nullptr);
}

void MeshFieldRenderer::DrawMeshOnly()
{
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	const XMFLOAT3 camera_position = Camera_GetPosition();
	for (auto level_it = g_terrain_clipmaps.rbegin(); level_it != g_terrain_clipmaps.rend(); ++level_it)
	{
		DrawTerrainClipmapLevel(*level_it, camera_position);
	}
}

float MeshFieldRenderer::GetHeight(float x, float z)
{
	return TerrainHeightField::GetHeight(x, z);
}

void MeshFieldRenderer::ApplyTerrainSettings(const TerrainSettings& settings)
{
	TerrainHeightField::ApplyTerrainSettings(settings);
}

const TerrainSettings& MeshFieldRenderer::GetTerrainSettings()
{
	return TerrainHeightField::GetTerrainSettings();
}

float MeshFieldRenderer::GetSuggestedWaterHeight()
{
	return TerrainHeightField::GetSuggestedWaterHeight();
}

float MeshFieldRenderer::FieldWidth()
{
	return TerrainHeightField::FieldWidth();
}

float MeshFieldRenderer::FieldDepth()
{
	return TerrainHeightField::FieldDepth();
}

Backend::RenderShaderResource MeshFieldRenderer::HeightResource()
{
	return TerrainHeightField::HeightResource();
}

ID3D11ShaderResourceView* MeshFieldRenderer::HeightSRV()
{
	return TerrainHeightField::HeightSRV();
}

void MeshFieldRenderer::SetRenderHeightSRV(ID3D11ShaderResourceView* srv)
{
	g_render_height_override_srv = srv;
}

void MeshFieldRenderer::SetRenderNormalSRV(ID3D11ShaderResourceView* srv)
{
	g_render_normal_override_srv = srv;
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
