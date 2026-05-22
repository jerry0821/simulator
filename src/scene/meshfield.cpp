// ----------------------------------------------------
// Procedural mesh field renderer [meshfield.cpp]
// ====================================================
// Created by: Jerry
// Updated : 2026-03-26
// ----------------------------------------------------
#include "meshfield.h"

#include <algorithm>
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
constexpr int kFieldMeshHCount = static_cast<int>(ComputeTextureDimensions::kTerrainMeshResolution);
constexpr int kFieldMeshVCount = static_cast<int>(ComputeTextureDimensions::kTerrainMeshResolution);
constexpr float kFieldMeshWidth =
	ComputeTextureDimensions::kWorldSideLength / static_cast<float>(kFieldMeshHCount);
constexpr float kFieldMeshDepth =
	ComputeTextureDimensions::kWorldSideLength / static_cast<float>(kFieldMeshVCount);

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

std::vector<Vertex3D> g_mesh_vertices;
std::vector<unsigned int> g_mesh_indices;
ID3D11ShaderResourceView* g_render_height_override_srv = nullptr;
ID3D11ShaderResourceView* g_render_normal_override_srv = nullptr;

void BuildProceduralTerrain()
{
	g_mesh_vertices.resize(kNumVertex);
	g_mesh_indices.resize(kNumIndex);

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
	RebuildGpuBuffers();

	g_field_texture_id0 = TextureManager::Load(L"resource/texture/rock.png");
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
	SAFE_RELEASE(g_vertex_buffer);
	SAFE_RELEASE(g_index_buffer);
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

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &g_vertex_buffer, &stride, &offset);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R32_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const float offset_x = kFieldMeshHCount * kFieldMeshWidth * 0.5f;
	const float offset_z = kFieldMeshVCount * kFieldMeshDepth * 0.5f;

	// Finite-world terrain should stay anchored in world space. Camera-snapping
	// was useful for the old repeating patch, but makes the terrain appear to
	// slide under the camera after switching to a clamped heightfield.
	ShaderField_SetWorldMatrix(XMMatrixTranslation(-offset_x, 0.0f, -offset_z));
	ShaderField_SetMaterialColor({1.0f, 1.0f, 1.0f, 1.0f});

	g_context->DrawIndexed(static_cast<UINT>(g_mesh_indices.size()), 0, 0);
	ShaderField_SetHeightMap(nullptr);
	ShaderField_SetTerrainNormalMap(nullptr);
	ShaderField_SetTerrainVegetationSuitabilityMap(nullptr);
	ShaderField_SetTerrainSurfaceDataMap(nullptr);
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
