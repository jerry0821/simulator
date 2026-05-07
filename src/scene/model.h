#pragma once

#include <unordered_map>
#include <vector>

#include <d3d11.h>
#include <DirectXMath.h>
#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/matrix4x4.h"
#pragma comment (lib, "assimp-vc143-mt.lib")

#include "collision.h"
#include "render_state.h"

struct MODEL
{
	const aiScene* AiScene = nullptr;

	ID3D11Buffer** VertexBuffer;
	ID3D11Buffer** IndexBuffer;

	std::unordered_map<std::string, ID3D11ShaderResourceView*> Texture;

	Collision::AABB localAABB; // モデルのローカルAABB(動かしてない)
};





MODEL* ModelLoad(const char* FileName, float scale = 1.0f, bool bBlender = false);
void ModelRelease(MODEL* model);

void ModelUnlitDraw(MODEL* pModel,
					const DirectX::XMMATRIX& mtxWorld,
					RenderState render_state = RenderState{});
void ModelDraw(MODEL* pModel, const DirectX::XMMATRIX& mtxWorld,float alpha = 1.0f);
void ModelDrawInstanced(MODEL* pModel,
						const std::vector<DirectX::XMFLOAT4X4>& world_matrices,
						float alpha = 1.0f,
						RenderState render_state = RenderState{ DepthMode::ReadWrite, BlendMode::Opaque, CullMode::Back });
void ModelDrawGrassInstanced(MODEL* pModel,
							 const std::vector<DirectX::XMFLOAT4X4>& world_matrices,
							 ID3D11ShaderResourceView* wind_field_srv,
							 float time_seconds,
							 float field_uv_scale,
							 float bend_scale,
							 float alpha = 1.0f,
							 RenderState render_state = RenderState{ DepthMode::ReadWrite, BlendMode::Opaque, CullMode::Back });
void ModelDrawShadow(MODEL* pModel, const DirectX::XMMATRIX& mtxWorld);

void ModelToonDraw(MODEL* pModel, const DirectX::XMMATRIX& mtxWorld, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj,const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

Collision::AABB Model_GetAABB(MODEL* model, const DirectX::XMFLOAT3& position);

void ModelDrawRaw(MODEL* pModel);
