// ----------------------------------------------------
//  [cube.h]
// ====================================================
// Created by: Jerry
// Date: 2025-09-09
// Version: 1.0
// ----------------------------------------------------
#include "cube.h"
#include <algorithm>
#include <cstring>
#include "direct3d.h"
#include <DirectXMath.h>
using namespace DirectX;
#include "shader3d.h"
#include "shader_grass_instanced.h"
#include "shader3d_instanced.h"
#include "shader_shadow.h"
#include "material_pass.h"
#include "sampler.h"
#include "texture.h"
#include <vector>

static constexpr int NUM_VERTEX = 4 * 6; 
static constexpr int NUM_INDEX = 3 * 2 * 6; 

static ID3D11Buffer* g_pVertexBuffer = nullptr; 
static ID3D11Buffer* g_pIndexBuffer = nullptr; // index buffer

static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;
static ID3D11Buffer* g_pInstanceBuffer = nullptr;
static size_t g_InstanceBufferCapacity = 0;


struct Vertex3D
{
	XMFLOAT3 position;	
	XMFLOAT3 normal;	
	XMFLOAT4 color;		
	XMFLOAT2 texcoord;	// UV
};

static Vertex3D g_CubeVertex[24]{
	{{ -0.5f,  0.5f, -0.5f}, {0.0f,0.0f,-1.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{ 0.75f,  0.5f}}, //A 
	{{  0.5f, -0.5f, -0.5f}, {0.0f,0.0f,-1.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{  1.0f,  1.0f}}, //C
	{{ -0.5f, -0.5f, -0.5f}, {0.0f,0.0f,-1.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{ 0.75f,  1.0f}}, //B

	{{  0.5f,  0.5f, -0.5f}, {0.0f,0.0f,-1.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{  1.0f,  0.5f}}, //D

	{{ -0.5f,  0.5f,  0.5f}, {-1.0f,0.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{  0.5f,  0.5f}}, //E 
	{{ -0.5f, -0.5f, -0.5f}, {-1.0f,0.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{ 0.75f,  1.0f}}, //B
	{{ -0.5f, -0.5f,  0.5f}, {-1.0f,0.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{  0.5f,  1.0f}}, //F
							 
	{{ -0.5f,  0.5f, -0.5f}, {-1.0f,0.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{ 0.75f,  0.5f}}, //A

	{{ -0.5f,  0.5f,  0.5f}, {0.0f,1.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{  0.5f,  0.0f}}, //E 
	{{  0.5f,  0.5f, -0.5f}, {0.0f,1.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{ 0.75f,  0.5f}}, //D
	{{ -0.5f,  0.5f, -0.5f}, {0.0f,1.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{  0.5f,  0.5f}}, //A
							 
	{{  0.5f,  0.5f,  0.5f}, {0.0f,1.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{ 0.75f,  0.0f}}, //H

	{{  0.5f,  0.5f, -0.5f}, {1.0f,0.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{ 0.75f,  0.0f}}, //D 
	{{  0.5f, -0.5f,  0.5f}, {1.0f,0.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{  1.0f,  0.5f}}, //G
	{{  0.5f, -0.5f, -0.5f}, {1.0f,0.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{ 0.75f,  0.5f}}, //C
							 
	{{  0.5f,  0.5f,  0.5f}, {1.0f,0.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{  1.0f,  0.0f}}, //H

	{{ -0.5f, -0.5f, -0.5f}, {0.0f,-1.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{  0.0f,  0.5f}}, //B 
	{{  0.5f, -0.5f,  0.5f}, {0.0f,-1.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{ 0.25f,  1.0f}}, //G
	{{ -0.5f, -0.5f,  0.5f}, {0.0f,-1.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{  0.0f,  1.0f}}, //F
							 				  
	{{  0.5f, -0.5f, -0.5f}, {0.0f,-1.0f,0.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{ 0.25f,  0.5f}}, //C

	{{  0.5f,  0.5f,  0.5f}, {0.0f,0.0f,1.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{ 0.25f,  0.5f}}, //H 
	{{ -0.5f, -0.5f,  0.5f}, {0.0f,0.0f,1.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{  0.5f,  1.0f}}, //F
	{{  0.5f, -0.5f,  0.5f}, {0.0f,0.0f,1.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{ 0.25f,  1.0f}}, //G
							 
	{{ -0.5f,  0.5f,  0.5f}, {0.0f,0.0f,1.0f},{ 1.0f, 1.0f, 1.0f, 1.0f},{  0.5f,  0.5f}}, //E
}; 

static unsigned short g_CubeIndex[]{
	0,1,2,0,3,1,
	4,5,6,4,7,5,
	8,9,10,8,11,9,
	12,13,14,12,15,13,
	16,17,18,16,19,17,
	20,21,22,20,23,21
};

void Cube_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	g_pDevice = pDevice;
	g_pContext = pContext;

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex3D) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = g_CubeVertex;

	g_pDevice->CreateBuffer(&bd, &sd, &g_pVertexBuffer);

	// index buffer
	bd.ByteWidth = sizeof(unsigned short) * NUM_INDEX;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	sd.pSysMem = g_CubeIndex;

	g_pDevice->CreateBuffer(&bd, &sd, &g_pIndexBuffer);


}

void Cube_Finalize(void)
{
	SAFE_RELEASE(g_pInstanceBuffer);
	SAFE_RELEASE(g_pVertexBuffer);
	SAFE_RELEASE(g_pIndexBuffer);
}

void Cube_Update(double elapsed_Time)
{
	elapsed_Time; 
}

void Cube_Draw(int texId, const DirectX::XMMATRIX& mtxWorld)
{
	Cube_DrawMaterial(
		texId,
		mtxWorld,
		{ 0.7f, 0.7f, 0.7f, 1.0f },
		RenderState{ DepthMode::ReadWrite, BlendMode::Opaque, CullMode::Back });
}

void Cube_DrawMaterial(int texId,
					   const DirectX::XMMATRIX& mtxWorld,
					   const DirectX::XMFLOAT4& material_color,
					   RenderState render_state,
					   MaterialType material_type)
{
	const MaterialPass material_pass{ material_type, render_state };
	material_pass.begin(mtxWorld, material_color);

	Texture_SetTexture(texId);

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	Direct3D_GetContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	Direct3D_GetContext()->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Direct3D_GetContext()->DrawIndexed(NUM_INDEX, 0, 0);
	material_pass.end();
}

void Cube_DrawMaterialSRV(ID3D11ShaderResourceView* texture_srv,
						  const DirectX::XMMATRIX& mtxWorld,
						  const DirectX::XMFLOAT4& material_color,
						  RenderState render_state,
						  MaterialType material_type)
{
	const MaterialPass material_pass{ material_type, render_state };
	material_pass.begin(mtxWorld, material_color);

	Texture_SetExternalSRV(texture_srv);

	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	Direct3D_GetContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	Direct3D_GetContext()->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Direct3D_GetContext()->DrawIndexed(NUM_INDEX, 0, 0);
	material_pass.end();
}

namespace
{
struct InstanceData
{
	XMFLOAT4 world0;
	XMFLOAT4 world1;
	XMFLOAT4 world2;
	XMFLOAT4 world3;
};

void EnsureInstanceBuffer(size_t instance_count)
{
	if (g_pInstanceBuffer != nullptr && g_InstanceBufferCapacity >= instance_count)
	{
		return;
	}

	SAFE_RELEASE(g_pInstanceBuffer);
	g_InstanceBufferCapacity = std::max<size_t>(instance_count, 64);

	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.ByteWidth = static_cast<UINT>(sizeof(InstanceData) * g_InstanceBufferCapacity);
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	Direct3D_GetDevice()->CreateBuffer(&desc, nullptr, &g_pInstanceBuffer);
}
}

void Cube_DrawInstanced(int texId,
						const std::vector<DirectX::XMFLOAT4X4>& world_matrices,
						const DirectX::XMFLOAT4& material_color,
						RenderState render_state)
{
	if (world_matrices.empty())
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
	}

	EnsureInstanceBuffer(instance_data.size());

	D3D11_MAPPED_SUBRESOURCE mapped{};
	Direct3D_GetContext()->Map(g_pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, instance_data.data(), sizeof(InstanceData) * instance_data.size());
	Direct3D_GetContext()->Unmap(g_pInstanceBuffer, 0);

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

	Shader3DInstanced_Begin();
	Shader3DInstanced_SetMaterialColor(material_color);
	Texture_SetTexture(texId);

	UINT strides[2] = { sizeof(Vertex3D), sizeof(InstanceData) };
	UINT offsets[2] = { 0, 0 };
	ID3D11Buffer* buffers[2] = { g_pVertexBuffer, g_pInstanceBuffer };
	Direct3D_GetContext()->IASetVertexBuffers(0, 2, buffers, strides, offsets);
	Direct3D_GetContext()->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Direct3D_GetContext()->DrawIndexedInstanced(
		NUM_INDEX,
		static_cast<UINT>(world_matrices.size()),
		0,
		0,
		0);

	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);
	Direct3D_SetCullMode(Direct3DCullMode::Back);
	Direct3D_SetBlendStateDisable();
}

void Cube_DrawGrassInstanced(int texId,
							 const std::vector<DirectX::XMFLOAT4X4>& world_matrices,
							 const DirectX::XMFLOAT4& material_color,
							 ID3D11ShaderResourceView* wind_field_srv,
							 float time_seconds,
							 float field_uv_scale,
							 float bend_scale,
							 RenderState render_state)
{
	if (world_matrices.empty())
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
	}

	EnsureInstanceBuffer(instance_data.size());

	D3D11_MAPPED_SUBRESOURCE mapped{};
	Direct3D_GetContext()->Map(g_pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, instance_data.data(), sizeof(InstanceData) * instance_data.size());
	Direct3D_GetContext()->Unmap(g_pInstanceBuffer, 0);

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

	Backend::DX11::Sampler::SetLinearFilter();
	ShaderGrassInstanced_Begin();
	ShaderGrassInstanced_SetMaterialColor(material_color);
	ShaderGrassInstanced_SetWindField(wind_field_srv);
	ShaderGrassInstanced_SetWindSettings(time_seconds, field_uv_scale, bend_scale);
	Texture_SetTexture(texId);

	UINT strides[2] = { sizeof(Vertex3D), sizeof(InstanceData) };
	UINT offsets[2] = { 0, 0 };
	ID3D11Buffer* buffers[2] = { g_pVertexBuffer, g_pInstanceBuffer };
	Direct3D_GetContext()->IASetVertexBuffers(0, 2, buffers, strides, offsets);
	Direct3D_GetContext()->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Direct3D_GetContext()->DrawIndexedInstanced(
		NUM_INDEX,
		static_cast<UINT>(world_matrices.size()),
		0,
		0,
		0);

	ShaderGrassInstanced_Clear();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);
	Direct3D_SetCullMode(Direct3DCullMode::Back);
	Direct3D_SetBlendStateDisable();
}

void Cube_DrawShadow(const DirectX::XMMATRIX& mtxWorld)
{
	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	Direct3D_GetContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	Direct3D_GetContext()->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ShaderShadow_SetWorldMatrix(mtxWorld);
	Direct3D_GetContext()->DrawIndexed(NUM_INDEX, 0, 0);
}

Collision::AABB Cube_GetAABB(const DirectX::XMFLOAT3& position)
{
	return {
		{ position.x - 0.5f,
		  position.y - 0.5f,
		  position.z - 0.5f,
		},
		{ position.x + 0.5f,
		  position.y + 0.5f,
		  position.z + 0.5f,}
	};
}

