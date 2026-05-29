// --------------------------------------------
// 3D image quad [sprite3d.cpp]
// ============================================

#include "sprite3d.h"

#include <d3d11.h>
#include <DirectXMath.h>
#include <algorithm>
#include <cstring>
#include <vector>

#include "compute_texture_dimensions.h"
#include "direct3d.h"
#include "shader_sprite3d_cutout.h"
#include "shader_sprite3d_cutout_instanced.h"
#include "shader_sprite3d_shadow_instanced.h"
#include "shader_sprite3d_transparent_instanced.h"
#include "shader_water.h"
#include "shader3d_unlit.h"
#include "texture.h"

using namespace DirectX;

namespace
{
struct Vertex3D
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT4 color;
	XMFLOAT2 texcoord;
};

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
ID3D11Buffer* g_vertex_buffer = nullptr;
ID3D11Buffer* g_index_buffer = nullptr;
ID3D11Buffer* g_water_vertex_buffer = nullptr;
ID3D11Buffer* g_water_index_buffer = nullptr;
UINT g_water_index_count = 0;
ID3D11Buffer* g_instance_buffer = nullptr;
ID3D11ShaderResourceView* g_instance_buffer_srv = nullptr;
size_t g_instance_buffer_capacity = 0;
ID3D11Buffer* g_transparent_instance_buffer = nullptr;
size_t g_transparent_instance_buffer_capacity = 0;
constexpr int kWaterGridResolution =
	static_cast<int>(ComputeTextureDimensions::kWaterMeshResolution);

struct InstanceData
{
	XMFLOAT4 world0;
	XMFLOAT4 world1;
	XMFLOAT4 world2;
	XMFLOAT4 world3;
	XMFLOAT4 lodColor;
};

void FillTransparentInstanceData(
	const std::vector<XMFLOAT4X4>& world_matrices,
	const std::vector<XMFLOAT4>* instance_colors,
	std::vector<InstanceData>& instance_data)
{
	instance_data.resize(world_matrices.size());
	for (size_t i = 0; i < world_matrices.size(); ++i)
	{
		const auto world = XMLoadFloat4x4(&world_matrices[i]);
		XMFLOAT4X4 transpose{};
		XMStoreFloat4x4(&transpose, XMMatrixTranspose(world));

		instance_data[i].world0 = { transpose._11, transpose._12, transpose._13, transpose._14 };
		instance_data[i].world1 = { transpose._21, transpose._22, transpose._23, transpose._24 };
		instance_data[i].world2 = { transpose._31, transpose._32, transpose._33, transpose._34 };
		instance_data[i].world3 = { transpose._41, transpose._42, transpose._43, transpose._44 };
		instance_data[i].lodColor =
			(instance_colors != nullptr && i < instance_colors->size())
				? (*instance_colors)[i]
				: XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
	}
}

void EnsureGeometry()
{
	if (g_vertex_buffer != nullptr && g_index_buffer != nullptr)
	{
		return;
	}

	const Vertex3D vertices[] = {
		{{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
		{{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
		{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
		{{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
	};

	const unsigned short indices[] = { 0, 1, 2, 2, 1, 3 };

	D3D11_BUFFER_DESC vb_desc{};
	vb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	vb_desc.ByteWidth = sizeof(vertices);
	vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vb_data{};
	vb_data.pSysMem = vertices;
	g_device->CreateBuffer(&vb_desc, &vb_data, &g_vertex_buffer);

	D3D11_BUFFER_DESC ib_desc{};
	ib_desc.Usage = D3D11_USAGE_IMMUTABLE;
	ib_desc.ByteWidth = sizeof(indices);
	ib_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA ib_data{};
	ib_data.pSysMem = indices;
	g_device->CreateBuffer(&ib_desc, &ib_data, &g_index_buffer);
}

void EnsureWaterGeometry()
{
	if (g_water_vertex_buffer != nullptr && g_water_index_buffer != nullptr)
	{
		return;
	}

	constexpr int vertex_side = kWaterGridResolution;
	constexpr int quad_count = (vertex_side - 1) * (vertex_side - 1);
	const UINT vertex_count = static_cast<UINT>(vertex_side * vertex_side);
	const UINT index_count = static_cast<UINT>(quad_count * 6);

	std::vector<Vertex3D> vertices(vertex_count);
	std::vector<unsigned int> indices(index_count);

	for (int y = 0; y < vertex_side; ++y)
	{
		for (int x = 0; x < vertex_side; ++x)
		{
			const float u = static_cast<float>(x) / static_cast<float>(vertex_side - 1);
			const float v = static_cast<float>(y) / static_cast<float>(vertex_side - 1);
			Vertex3D& vertex = vertices[static_cast<size_t>(y) * vertex_side + x];
			const float grid_x =
				(static_cast<float>(y) - static_cast<float>(vertex_side) * 0.5f) *
				ComputeTextureDimensions::kWaterMeshInterval;
			const float grid_z =
				(static_cast<float>(x) - static_cast<float>(vertex_side) * 0.5f) *
				ComputeTextureDimensions::kWaterMeshInterval;
			vertex.position = { grid_x, 0.0f, grid_z };
			vertex.normal = { 0.0f, 1.0f, 0.0f };
			vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };
			vertex.texcoord = { u, v };
		}
	}

	UINT index_cursor = 0;
	for (int y = 0; y < vertex_side - 1; ++y)
	{
		for (int x = 0; x < vertex_side - 1; ++x)
		{
			const unsigned int top_left = static_cast<unsigned int>(y * vertex_side + x);
			const unsigned int top_right = top_left + 1;
			const unsigned int bottom_left = top_left + vertex_side;
			const unsigned int bottom_right = bottom_left + 1;

			indices[index_cursor++] = top_left;
			indices[index_cursor++] = top_right;
			indices[index_cursor++] = bottom_right;
			indices[index_cursor++] = top_left;
			indices[index_cursor++] = bottom_right;
			indices[index_cursor++] = bottom_left;
		}
	}

	D3D11_BUFFER_DESC vb_desc{};
	vb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	vb_desc.ByteWidth = static_cast<UINT>(sizeof(Vertex3D) * vertices.size());
	vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vb_data{};
	vb_data.pSysMem = vertices.data();
	g_device->CreateBuffer(&vb_desc, &vb_data, &g_water_vertex_buffer);

	D3D11_BUFFER_DESC ib_desc{};
	ib_desc.Usage = D3D11_USAGE_IMMUTABLE;
	ib_desc.ByteWidth = static_cast<UINT>(sizeof(unsigned int) * indices.size());
	ib_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA ib_data{};
	ib_data.pSysMem = indices.data();
	g_device->CreateBuffer(&ib_desc, &ib_data, &g_water_index_buffer);

	g_water_index_count = index_count;
}

void EnsureInstanceBuffer(size_t instance_count)
{
	if (g_instance_buffer != nullptr && g_instance_buffer_capacity >= instance_count)
	{
		return;
	}

	SAFE_RELEASE(g_instance_buffer);
	SAFE_RELEASE(g_instance_buffer_srv);
	g_instance_buffer_capacity = std::max<size_t>(instance_count, 64);

	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.ByteWidth = static_cast<UINT>(sizeof(InstanceData) * g_instance_buffer_capacity);
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = sizeof(InstanceData);

	g_device->CreateBuffer(&desc, nullptr, &g_instance_buffer);
	if (g_instance_buffer != nullptr)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
		srv_desc.Format = DXGI_FORMAT_UNKNOWN;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srv_desc.Buffer.FirstElement = 0;
		srv_desc.Buffer.NumElements = static_cast<UINT>(g_instance_buffer_capacity);
		g_device->CreateShaderResourceView(g_instance_buffer, &srv_desc, &g_instance_buffer_srv);
	}
}

void EnsureTransparentInstanceBuffer(size_t instance_count)
{
	if (g_transparent_instance_buffer != nullptr &&
		g_transparent_instance_buffer_capacity >= instance_count)
	{
		return;
	}

	SAFE_RELEASE(g_transparent_instance_buffer);
	g_transparent_instance_buffer_capacity = std::max<size_t>(instance_count, 64);

	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.ByteWidth = static_cast<UINT>(sizeof(InstanceData) * g_transparent_instance_buffer_capacity);
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	g_device->CreateBuffer(&desc, nullptr, &g_transparent_instance_buffer);
}
}

void Sprite3D_Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	g_device = device;
	g_context = context;
	EnsureGeometry();
	ShaderSprite3D_Cutout_Initialize();
	ShaderSprite3D_CutoutInstanced_Initialize();
	ShaderSprite3D_ShadowInstanced_Initialize();
	ShaderSprite3D_TransparentInstanced_Initialize();
	ShaderWater_Initialize();
}

void Sprite3D_Finalize()
{
	ShaderSprite3D_TransparentInstanced_Finalize();
	ShaderSprite3D_ShadowInstanced_Finalize();
	ShaderSprite3D_CutoutInstanced_Finalize();
	ShaderSprite3D_Cutout_Finalize();
	ShaderWater_Finalize();
	SAFE_RELEASE(g_instance_buffer);
	SAFE_RELEASE(g_instance_buffer_srv);
	SAFE_RELEASE(g_transparent_instance_buffer);
	SAFE_RELEASE(g_water_index_buffer);
	SAFE_RELEASE(g_water_vertex_buffer);
	SAFE_RELEASE(g_index_buffer);
	SAFE_RELEASE(g_vertex_buffer);
	g_context = nullptr;
	g_device = nullptr;
}

void Sprite3D_Draw(int texid, const XMMATRIX& world_matrix, const XMFLOAT4& color)
{
	if (texid < 0 || g_device == nullptr || g_context == nullptr)
	{
		return;
	}

	EnsureGeometry();
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr)
	{
		return;
	}

	Shader3D_Unlit_SetWorldMatrix(world_matrix);
	Shader3D_Unlit_SetMaterialColor(color);
	Shader3D_Unlit_Begin();
	Texture_SetTexture(texid);

	Direct3D_SetCullMode(Direct3DCullMode::None);
	Direct3D_SetBlendStateDisable();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &g_vertex_buffer, &stride, &offset);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->DrawIndexed(6, 0, 0);
}

void Sprite3D_DrawTransparent(int texid, const XMMATRIX& world_matrix, const XMFLOAT4& color)
{
	if (texid < 0 || g_device == nullptr || g_context == nullptr)
	{
		return;
	}

	EnsureGeometry();
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr)
	{
		return;
	}

	Shader3D_Unlit_SetWorldMatrix(world_matrix);
	Shader3D_Unlit_SetMaterialColor(color);
	Shader3D_Unlit_Begin();
	Texture_SetTexture(texid);

	Direct3D_SetCullMode(Direct3DCullMode::None);
	Direct3D_SetAlphaBlendAdd();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(false);

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &g_vertex_buffer, &stride, &offset);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->DrawIndexed(6, 0, 0);

	Direct3D_SetBlendStateDisable();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);
	Direct3D_SetCullMode(Direct3DCullMode::Back);
}

void Sprite3D_DrawTransparentSRV(ID3D11ShaderResourceView* texture_srv,
								 const XMMATRIX& world_matrix,
								 const XMFLOAT4& color)
{
	if (texture_srv == nullptr || g_device == nullptr || g_context == nullptr)
	{
		return;
	}

	EnsureGeometry();
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr)
	{
		return;
	}

	Shader3D_Unlit_SetWorldMatrix(world_matrix);
	Shader3D_Unlit_SetMaterialColor(color);
	Shader3D_Unlit_Begin();
	Texture_SetExternalSRV(texture_srv);

	Direct3D_SetCullMode(Direct3DCullMode::None);
	Direct3D_SetAlphaBlendAdd();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(false);

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &g_vertex_buffer, &stride, &offset);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->DrawIndexed(6, 0, 0);

	Direct3D_SetBlendStateDisable();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);
	Direct3D_SetCullMode(Direct3DCullMode::Back);
}

void Sprite3D_DrawAdditiveSRV(ID3D11ShaderResourceView* texture_srv,
							  const XMMATRIX& world_matrix,
							  const XMFLOAT4& color)
{
	if (texture_srv == nullptr || g_device == nullptr || g_context == nullptr)
	{
		return;
	}

	EnsureGeometry();
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr)
	{
		return;
	}

	Shader3D_Unlit_SetWorldMatrix(world_matrix);
	Shader3D_Unlit_SetMaterialColor(color);
	Shader3D_Unlit_Begin();
	Texture_SetExternalSRV(texture_srv);

	Direct3D_SetCullMode(Direct3DCullMode::None);
	Direct3D_SetAlphaBlendAdd();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(false);

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &g_vertex_buffer, &stride, &offset);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->DrawIndexed(6, 0, 0);

	Direct3D_SetBlendStateDisable();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);
	Direct3D_SetCullMode(Direct3DCullMode::Back);
}

void Sprite3D_DrawWaterSRV(ID3D11ShaderResourceView* terrain_height_srv,
						   ID3D11ShaderResourceView* water_velocity_srv,
						   ID3D11ShaderResourceView* water_sediment_srv,
						   ID3D11ShaderResourceView* terrain_normal_srv,
						   ID3D11ShaderResourceView* scene_depth_srv,
						   const XMMATRIX& world_matrix,
						   const XMFLOAT4& color,
						   const XMFLOAT3& camera_position,
						   float highlight_strength)
{
	if (terrain_height_srv == nullptr || g_device == nullptr || g_context == nullptr)
	{
		return;
	}

	EnsureWaterGeometry();
	if (g_water_vertex_buffer == nullptr || g_water_index_buffer == nullptr || g_water_index_count == 0)
	{
		return;
	}

	ShaderWater_SetWorldMatrix(world_matrix);
	ShaderWater_SetMaterialColor(color);
	ShaderWater_SetCameraPosition(camera_position);
	ShaderWater_SetSurfaceSettings(highlight_strength);
	ShaderWater_SetTerrainHeight(terrain_height_srv);
	ShaderWater_SetWaterVelocity(water_velocity_srv);
	ShaderWater_SetWaterSediment(water_sediment_srv);
	ShaderWater_SetTerrainNormal(terrain_normal_srv);
	ShaderWater_SetSceneDepth(scene_depth_srv);
	static float water_time = 0.0f;
	water_time += 1.0f / 60.0f;
	ShaderWater_SetAnimationSettings(water_time);


	ShaderWater_Begin();

	Direct3D_SetCullMode(Direct3DCullMode::None);
	Direct3D_SetAlphaBlendTransparent();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(false);

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &g_water_vertex_buffer, &stride, &offset);
	g_context->IASetIndexBuffer(g_water_index_buffer, DXGI_FORMAT_R32_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->DrawIndexed(g_water_index_count, 0, 0);

	ShaderWater_End();
	Direct3D_SetBlendStateDisable();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);
	Direct3D_SetCullMode(Direct3DCullMode::Back);
}

void Sprite3D_DrawTransparentInstanced(
	int texid,
	const std::vector<XMFLOAT4X4>& world_matrices,
	const XMFLOAT4& color)
{
	if (texid < 0 || g_device == nullptr || g_context == nullptr || world_matrices.empty())
	{
		return;
	}

	EnsureGeometry();
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr)
	{
		return;
	}

	std::vector<InstanceData> instance_data;
	FillTransparentInstanceData(world_matrices, nullptr, instance_data);

	EnsureTransparentInstanceBuffer(instance_data.size());

	D3D11_MAPPED_SUBRESOURCE mapped{};
	g_context->Map(g_transparent_instance_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, instance_data.data(), sizeof(InstanceData) * instance_data.size());
	g_context->Unmap(g_transparent_instance_buffer, 0);

	Sprite3D_DrawTransparentInstancedBuffer(
		texid,
		g_transparent_instance_buffer,
		static_cast<unsigned int>(world_matrices.size()),
		color);
}

void Sprite3D_DrawTransparentInstancedColored(
	int texid,
	const std::vector<XMFLOAT4X4>& world_matrices,
	const std::vector<XMFLOAT4>& instance_colors,
	const XMFLOAT4& color)
{
	if (texid < 0 || g_device == nullptr || g_context == nullptr ||
		world_matrices.empty() || instance_colors.size() < world_matrices.size())
	{
		return;
	}

	EnsureGeometry();
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr)
	{
		return;
	}

	std::vector<InstanceData> instance_data;
	FillTransparentInstanceData(world_matrices, &instance_colors, instance_data);

	EnsureTransparentInstanceBuffer(instance_data.size());

	D3D11_MAPPED_SUBRESOURCE mapped{};
	g_context->Map(g_transparent_instance_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, instance_data.data(), sizeof(InstanceData) * instance_data.size());
	g_context->Unmap(g_transparent_instance_buffer, 0);

	Sprite3D_DrawTransparentInstancedBuffer(
		texid,
		g_transparent_instance_buffer,
		static_cast<unsigned int>(world_matrices.size()),
		color);
}

void Sprite3D_DrawAdditiveInstancedColored(
	int texid,
	const std::vector<XMFLOAT4X4>& world_matrices,
	const std::vector<XMFLOAT4>& instance_colors,
	const XMFLOAT4& color)
{
	if (texid < 0 || g_device == nullptr || g_context == nullptr ||
		world_matrices.empty() || instance_colors.size() < world_matrices.size())
	{
		return;
	}

	EnsureGeometry();
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr)
	{
		return;
	}

	std::vector<InstanceData> instance_data;
	FillTransparentInstanceData(world_matrices, &instance_colors, instance_data);

	EnsureTransparentInstanceBuffer(instance_data.size());

	D3D11_MAPPED_SUBRESOURCE mapped{};
	g_context->Map(g_transparent_instance_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, instance_data.data(), sizeof(InstanceData) * instance_data.size());
	g_context->Unmap(g_transparent_instance_buffer, 0);

	Sprite3D_DrawAdditiveInstancedBuffer(
		texid,
		g_transparent_instance_buffer,
		static_cast<unsigned int>(world_matrices.size()),
		color);
}

void Sprite3D_DrawTransparentInstancedBuffer(
	int texid,
	ID3D11Buffer* instance_buffer,
	unsigned int instance_count,
	const XMFLOAT4& color)
{
	if (texid < 0 || g_device == nullptr || g_context == nullptr || instance_buffer == nullptr || instance_count == 0)
	{
		return;
	}

	EnsureGeometry();
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr)
	{
		return;
	}

	ShaderSprite3D_TransparentInstanced_SetMaterialColor(color);
	ShaderSprite3D_TransparentInstanced_Begin();
	Texture_SetTexture(texid);

	Direct3D_SetCullMode(Direct3DCullMode::None);
	Direct3D_SetAlphaBlendTransparent();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(false);

	UINT strides[2] = { sizeof(Vertex3D), sizeof(InstanceData) };
	UINT offsets[2] = { 0, 0 };
	ID3D11Buffer* buffers[2] = { g_vertex_buffer, instance_buffer };
	g_context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->DrawIndexedInstanced(6, instance_count, 0, 0, 0);

	ShaderSprite3D_TransparentInstanced_Clear();
	Direct3D_SetBlendStateDisable();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);
	Direct3D_SetCullMode(Direct3DCullMode::Back);
}

void Sprite3D_DrawAdditiveInstancedBuffer(
	int texid,
	ID3D11Buffer* instance_buffer,
	unsigned int instance_count,
	const XMFLOAT4& color)
{
	if (texid < 0 || g_device == nullptr || g_context == nullptr || instance_buffer == nullptr || instance_count == 0)
	{
		return;
	}

	EnsureGeometry();
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr)
	{
		return;
	}

	ShaderSprite3D_TransparentInstanced_SetMaterialColor(color);
	ShaderSprite3D_TransparentInstanced_Begin();
	Texture_SetTexture(texid);

	Direct3D_SetCullMode(Direct3DCullMode::None);
	Direct3D_SetAlphaBlendAdd();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(false);

	UINT strides[2] = { sizeof(Vertex3D), sizeof(InstanceData) };
	UINT offsets[2] = { 0, 0 };
	ID3D11Buffer* buffers[2] = { g_vertex_buffer, instance_buffer };
	g_context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->DrawIndexedInstanced(6, instance_count, 0, 0, 0);

	ShaderSprite3D_TransparentInstanced_Clear();
	Direct3D_SetBlendStateDisable();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);
	Direct3D_SetCullMode(Direct3DCullMode::Back);
}

void Sprite3D_DrawAdditiveInstancedBufferOverlay(
	int texid,
	ID3D11Buffer* instance_buffer,
	unsigned int instance_count,
	const XMFLOAT4& color)
{
	if (texid < 0 || g_device == nullptr || g_context == nullptr || instance_buffer == nullptr || instance_count == 0)
	{
		return;
	}

	EnsureGeometry();
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr)
	{
		return;
	}

	ShaderSprite3D_TransparentInstanced_SetMaterialColor(color);
	ShaderSprite3D_TransparentInstanced_Begin();
	Texture_SetTexture(texid);

	Direct3D_SetCullMode(Direct3DCullMode::None);
	Direct3D_SetAlphaBlendAdd();
	Direct3D_SetDepthEnable(false);
	Direct3D_SetDepthWrite(false);

	UINT strides[2] = { sizeof(Vertex3D), sizeof(InstanceData) };
	UINT offsets[2] = { 0, 0 };
	ID3D11Buffer* buffers[2] = { g_vertex_buffer, instance_buffer };
	g_context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->DrawIndexedInstanced(6, instance_count, 0, 0, 0);

	ShaderSprite3D_TransparentInstanced_Clear();
	Direct3D_SetBlendStateDisable();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);
	Direct3D_SetCullMode(Direct3DCullMode::Back);
}

void Sprite3D_DrawCutout(int texid, const XMMATRIX& world_matrix, const XMFLOAT4& color)
{
	if (texid < 0 || g_device == nullptr || g_context == nullptr)
	{
		return;
	}

	EnsureGeometry();
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr)
	{
		return;
	}

	ShaderSprite3D_Cutout_SetWorldMatrix(world_matrix);
	ShaderSprite3D_Cutout_SetMaterialColor(color);
	ShaderSprite3D_Cutout_Begin();
	Texture_SetTexture(texid);

	Direct3D_SetCullMode(Direct3DCullMode::None);
	Direct3D_SetBlendStateDisable();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &g_vertex_buffer, &stride, &offset);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->DrawIndexed(6, 0, 0);
}

void Sprite3D_DrawCutoutInstanced(
	int texid,
	const std::vector<XMFLOAT4X4>& world_matrices,
	const XMFLOAT4& color)
{
	if (texid < 0 || g_device == nullptr || g_context == nullptr || world_matrices.empty())
	{
		return;
	}

	EnsureGeometry();
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr)
	{
		return;
	}

	std::vector<InstanceData> instance_data(world_matrices.size());
	for (size_t i = 0; i < world_matrices.size(); ++i)
	{
		const auto world = XMLoadFloat4x4(&world_matrices[i]);
		XMFLOAT4X4 transpose{};
		XMStoreFloat4x4(&transpose, XMMatrixTranspose(world));

		instance_data[i].world0 = { transpose._11, transpose._12, transpose._13, transpose._14 };
		instance_data[i].world1 = { transpose._21, transpose._22, transpose._23, transpose._24 };
		instance_data[i].world2 = { transpose._31, transpose._32, transpose._33, transpose._34 };
		instance_data[i].world3 = { transpose._41, transpose._42, transpose._43, transpose._44 };
		instance_data[i].lodColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	}

	EnsureInstanceBuffer(instance_data.size());

	D3D11_MAPPED_SUBRESOURCE mapped{};
	g_context->Map(g_instance_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, instance_data.data(), sizeof(InstanceData) * instance_data.size());
	g_context->Unmap(g_instance_buffer, 0);

	Sprite3D_DrawCutoutInstancedBuffer(
		texid,
		g_instance_buffer_srv,
		static_cast<unsigned int>(world_matrices.size()),
		color);
}

void Sprite3D_DrawCutoutInstancedBuffer(
	int texid,
	ID3D11ShaderResourceView* instance_srv,
	unsigned int instance_count,
	const XMFLOAT4& color)
{
	if (texid < 0 || g_device == nullptr || g_context == nullptr || instance_srv == nullptr || instance_count == 0)
	{
		return;
	}

	EnsureGeometry();
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr)
	{
		return;
	}

	ShaderSprite3D_CutoutInstanced_SetMaterialColor(color);
	ShaderSprite3D_CutoutInstanced_SetInstanceBuffer(instance_srv);
	ShaderSprite3D_CutoutInstanced_Begin();
	Texture_SetTexture(texid);

	Direct3D_SetCullMode(Direct3DCullMode::None);
	Direct3D_SetBlendStateDisable();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &g_vertex_buffer, &stride, &offset);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->DrawIndexedInstanced(6, instance_count, 0, 0, 0);
	ShaderSprite3D_CutoutInstanced_Clear();
}

void Sprite3D_DrawCutoutInstancedIndirectBuffer(
	int texid,
	ID3D11ShaderResourceView* instance_srv,
	ID3D11Buffer* args_buffer,
	const XMFLOAT4& color)
{
	if (texid < 0 || g_device == nullptr || g_context == nullptr || instance_srv == nullptr || args_buffer == nullptr)
	{
		return;
	}

	EnsureGeometry();
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr)
	{
		return;
	}

	ShaderSprite3D_CutoutInstanced_SetMaterialColor(color);
	ShaderSprite3D_CutoutInstanced_SetInstanceBuffer(instance_srv);
	ShaderSprite3D_CutoutInstanced_Begin();
	Texture_SetTexture(texid);

	Direct3D_SetCullMode(Direct3DCullMode::None);
	Direct3D_SetBlendStateDisable();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &g_vertex_buffer, &stride, &offset);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->DrawIndexedInstancedIndirect(args_buffer, 0);
	ShaderSprite3D_CutoutInstanced_Clear();
}

void Sprite3D_DrawCutoutShadowInstancedIndirectBuffer(
	int texid,
	ID3D11ShaderResourceView* instance_srv,
	ID3D11Buffer* args_buffer)
{
	if (texid < 0 || g_device == nullptr || g_context == nullptr || instance_srv == nullptr || args_buffer == nullptr)
	{
		return;
	}

	EnsureGeometry();
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr)
	{
		return;
	}

	ShaderSprite3D_ShadowInstanced_SetInstanceBuffer(instance_srv);
	ShaderSprite3D_ShadowInstanced_Begin();
	Texture_SetTexture(texid);

	Direct3D_SetCullMode(Direct3DCullMode::None);
	Direct3D_SetBlendStateDisable();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &g_vertex_buffer, &stride, &offset);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->DrawIndexedInstancedIndirect(args_buffer, 0);
	ShaderSprite3D_ShadowInstanced_Clear();
}
