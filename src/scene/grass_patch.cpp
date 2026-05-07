#include "grass_patch.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "direct3d.h"
#include "sampler.h"
#include "shader_grass_instanced.h"
#include "shader_shadow.h"
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

struct InstanceData
{
	XMFLOAT4 world0;
	XMFLOAT4 world1;
	XMFLOAT4 world2;
	XMFLOAT4 world3;
};

constexpr float kHalfWidth = 0.5f;
constexpr float kBottomY = -0.5f;
constexpr float kTopY = 0.5f;

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
ID3D11Buffer* g_vertex_buffer = nullptr;
ID3D11Buffer* g_index_buffer = nullptr;
ID3D11Buffer* g_instance_buffer = nullptr;
size_t g_instance_capacity = 0;

void EnsureGeometry()
{
	if (g_vertex_buffer != nullptr && g_index_buffer != nullptr)
	{
		return;
	}

	const XMFLOAT3 normal = { 0.0f, 0.0f, 1.0f };
	const XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
	const Vertex3D vertices[] = {
		{{-kHalfWidth, kTopY, 0.0f}, normal, color, {0.0f, 1.0f}},
		{{ kHalfWidth, kTopY, 0.0f}, normal, color, {1.0f, 1.0f}},
		{{-kHalfWidth, kBottomY, 0.0f}, normal, color, {0.0f, 0.0f}},
		{{ kHalfWidth, kBottomY, 0.0f}, normal, color, {1.0f, 0.0f}}
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

void EnsureInstanceBuffer(size_t instance_count)
{
	if (g_instance_buffer != nullptr && g_instance_capacity >= instance_count)
	{
		return;
	}

	SAFE_RELEASE(g_instance_buffer);
	g_instance_capacity = std::max<size_t>(instance_count, 64);

	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.ByteWidth = static_cast<UINT>(sizeof(InstanceData) * g_instance_capacity);
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	g_device->CreateBuffer(&desc, nullptr, &g_instance_buffer);
}

void FillInstanceData(const std::vector<XMFLOAT4X4>& world_matrices, std::vector<InstanceData>& instance_data)
{
	instance_data.resize(world_matrices.size());
	for (size_t i = 0; i < world_matrices.size(); ++i)
	{
		const XMMATRIX world = XMLoadFloat4x4(&world_matrices[i]);
		XMFLOAT4X4 transpose{};
		XMStoreFloat4x4(&transpose, XMMatrixTranspose(world));

		instance_data[i].world0 = { transpose._11, transpose._12, transpose._13, transpose._14 };
		instance_data[i].world1 = { transpose._21, transpose._22, transpose._23, transpose._24 };
		instance_data[i].world2 = { transpose._31, transpose._32, transpose._33, transpose._34 };
		instance_data[i].world3 = { transpose._41, transpose._42, transpose._43, transpose._44 };
	}
}

void ApplyRenderState(RenderState render_state)
{
	switch (render_state.blend_mode)
	{
	case BlendMode::Alpha:
		Direct3D_SetAlphaBlendTransparent();
		break;
	case BlendMode::Additive:
		Direct3D_SetAlphaBlendAdd();
		break;
	case BlendMode::Opaque:
	default:
		Direct3D_SetBlendStateDisable();
		break;
	}

	Direct3D_SetCullMode(static_cast<Direct3DCullMode>(render_state.cull_mode));
	Direct3D_SetDepthEnable(render_state.depth_mode != DepthMode::Disabled);
	Direct3D_SetDepthWrite(render_state.depth_mode == DepthMode::ReadWrite);
}

void RestoreRenderState()
{
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);
	Direct3D_SetCullMode(Direct3DCullMode::Back);
	Direct3D_SetBlendStateDisable();
}
}

void GrassPatch_Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	g_device = device;
	g_context = context;
	EnsureGeometry();
}

void GrassPatch_Finalize()
{
	SAFE_RELEASE(g_instance_buffer);
	SAFE_RELEASE(g_index_buffer);
	SAFE_RELEASE(g_vertex_buffer);
	g_instance_capacity = 0;
	g_context = nullptr;
	g_device = nullptr;
}

void GrassPatch_DrawInstanced(int tex_id,
							  const std::vector<DirectX::XMFLOAT4X4>& world_matrices,
							  const DirectX::XMFLOAT4& material_color,
							  ID3D11ShaderResourceView* wind_field_srv,
							  float time_seconds,
							  float field_uv_scale,
							  float bend_scale,
							  RenderState render_state)
{
	(void)wind_field_srv;
	(void)time_seconds;
	(void)field_uv_scale;
	(void)bend_scale;

	if (world_matrices.empty() || g_device == nullptr || g_context == nullptr)
	{
		return;
	}

	EnsureGeometry();

	std::vector<InstanceData> instance_data;
	FillInstanceData(world_matrices, instance_data);
	EnsureInstanceBuffer(instance_data.size());

	D3D11_MAPPED_SUBRESOURCE mapped{};
	g_context->Map(g_instance_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	std::memcpy(mapped.pData, instance_data.data(), sizeof(InstanceData) * instance_data.size());
	g_context->Unmap(g_instance_buffer, 0);

	ApplyRenderState(render_state);
	Backend::DX11::Sampler::SetLinearFilter();
	ShaderGrassInstanced_Begin();
	ShaderGrassInstanced_SetMaterialColor(material_color);
	ShaderGrassInstanced_SetWindField(nullptr);
	ShaderGrassInstanced_SetWindSettings(0.0f, 0.0f, 0.0f);
	Texture_SetTexture(tex_id);

	UINT strides[2] = { sizeof(Vertex3D), sizeof(InstanceData) };
	UINT offsets[2] = { 0, 0 };
	ID3D11Buffer* buffers[2] = { g_vertex_buffer, g_instance_buffer };
	g_context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->DrawIndexedInstanced(6, static_cast<UINT>(world_matrices.size()), 0, 0, 0);

	ShaderGrassInstanced_Clear();
	RestoreRenderState();
}

void GrassPatch_DrawShadow(const DirectX::XMMATRIX& world_matrix)
{
	if (g_vertex_buffer == nullptr || g_index_buffer == nullptr || g_context == nullptr)
	{
		return;
	}

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &g_vertex_buffer, &stride, &offset);
	g_context->IASetIndexBuffer(g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ShaderShadow_SetWorldMatrix(world_matrix);
	g_context->DrawIndexed(6, 0, 0);
}
