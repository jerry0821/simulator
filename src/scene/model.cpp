
#include <assert.h>
#include <algorithm>
#include <cstring>
#include "direct3d.h"
#include "texture.h"
#include "model.h"
#include <DirectXMath.h>
using namespace DirectX;

#include "WICTextureLoader11.h"
#include "debug_ostream.h"
#include "material_pass.h"
#include "shader3d.h"
#include "shader3d_instanced.h"
#include "shader_grass_instanced.h"
#include "shader3d_unlit.h"
#include "shader_toon.h"
#include "shader_shadow.h"
#include "sampler.h"

#include "camera.h"
#include "camera.h"

int g_default_texture_id = -1;

static int g_WhiteTexId = -1;

// 頂点構造体
struct Vertex3D
{
	XMFLOAT3 position;	// 頂点座標
	XMFLOAT3 normal;	// 法線
	XMFLOAT4 color;		// 色
	XMFLOAT2 texcoord;	// テクスチャーUV
};

static int g_TextureWhite = -1;
static ID3D11Buffer* g_pModelInstanceBuffer = nullptr;
static size_t g_ModelInstanceBufferCapacity = 0;

MODEL* ModelLoad(const char* FileName, float scale, bool bBlender)
{
	MODEL* model = new MODEL;

	if (g_WhiteTexId == -1) {
		g_WhiteTexId = Texture_Load(L"resource/texture/white.png");
		// 防呆：如果 white.png 讀不到，就暫時用 0 號貼圖頂著，總比隱形好
		if (g_WhiteTexId < 0) g_WhiteTexId = 0;
	}
	
	//model->AiScene = aiImportFile(FileName, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded);
	model->AiScene = aiImportFile(FileName, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded | aiProcess_OptimizeMeshes);
	if (!model->AiScene) {
		OutputDebugStringA(aiGetErrorString());
		return nullptr;
	}

	// fbxのファイルパスだけ取得
	const std::string modelPath(FileName);
	size_t pos = modelPath.find_last_of("/\\");
	std::string directory;
	if (pos != std::string::npos) {
		directory = modelPath.substr(0, pos);
	}
	else {
		directory = "";
	}

	// 頂点バッファ、インデックスバッファ生成
	model->VertexBuffer = new ID3D11Buffer * [model->AiScene->mNumMeshes];
	model->IndexBuffer = new ID3D11Buffer * [model->AiScene->mNumMeshes];

	for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
	{
		aiMesh* mesh = model->AiScene->mMeshes[m];

		// 頂点バッファ生成
		{
			Vertex3D* vertex = new Vertex3D[mesh->mNumVertices];

			for (unsigned int v = 0; v < mesh->mNumVertices; v++)
			{
				if (bBlender)
				{
					vertex[v].position = XMFLOAT3(mesh->mVertices[v].x * scale, -mesh->mVertices[v].z * scale, mesh->mVertices[v].y * scale);
					vertex[v].normal = XMFLOAT3(mesh->mNormals[v].x, -mesh->mNormals[v].z, mesh->mNormals[v].y);
				}
				else
				{
					vertex[v].position = XMFLOAT3(mesh->mVertices[v].x * scale, mesh->mVertices[v].y * scale, mesh->mVertices[v].z * scale);
					vertex[v].normal = XMFLOAT3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
				}

				vertex[v].texcoord = XMFLOAT2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);

				if (mesh->HasVertexColors(0))
				{
					vertex[v].color = XMFLOAT4(
						mesh->mColors[0][v].r,
						mesh->mColors[0][v].g,
						mesh->mColors[0][v].b,
						mesh->mColors[0][v].a  // toon shader 粗細
					);
				}
				else
				{
					// 防呆：如果模型忘記匯出顏色，給一個預設值 (0.5 代表法線 0,0,0)
					vertex[v].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
				}
				
				// ローカルAABB計算
				if (v == 0 && m == 0){
					model->localAABB.min = vertex[v].position;
					model->localAABB.max = vertex[v].position;
				}
				else {
					model->localAABB.min.x = std::min(model->localAABB.min.x, vertex[v].position.x);
					model->localAABB.min.y = std::min(model->localAABB.min.y, vertex[v].position.y);
					model->localAABB.min.z = std::min(model->localAABB.min.z, vertex[v].position.z);
					model->localAABB.max.x = std::max(model->localAABB.max.x, vertex[v].position.x);
					model->localAABB.max.y = std::max(model->localAABB.max.y, vertex[v].position.y);
					model->localAABB.max.z = std::max(model->localAABB.max.z, vertex[v].position.z);
				}
			}

			D3D11_BUFFER_DESC bd;
			ZeroMemory(&bd, sizeof(bd));
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = sizeof(Vertex3D) * mesh->mNumVertices;
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bd.CPUAccessFlags = 0;

			D3D11_SUBRESOURCE_DATA sd;
			ZeroMemory(&sd, sizeof(sd));
			sd.pSysMem = vertex;

			Direct3D_GetDevice()->CreateBuffer(&bd, &sd, &model->VertexBuffer[m]);

			delete[] vertex;
		}

		// インデックスバッファ生成
		{
			unsigned int* index = new unsigned int[mesh->mNumFaces * 3];

			for (unsigned int f = 0; f < mesh->mNumFaces; f++)
			{
				const aiFace* face = &mesh->mFaces[f];

				assert(face->mNumIndices == 3);

				index[f * 3 + 0] = face->mIndices[0];
				index[f * 3 + 1] = face->mIndices[1];
				index[f * 3 + 2] = face->mIndices[2];
			}

			D3D11_BUFFER_DESC bd;
			ZeroMemory(&bd, sizeof(bd));
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = sizeof(unsigned int) * mesh->mNumFaces * 3;
			bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
			bd.CPUAccessFlags = 0;

			D3D11_SUBRESOURCE_DATA sd;
			ZeroMemory(&sd, sizeof(sd));
			sd.pSysMem = index;

			Direct3D_GetDevice()->CreateBuffer(&bd, &sd, &model->IndexBuffer[m]);

			delete[] index;
		}

	}

	g_TextureWhite = Texture_Load(L"resource/texture/white.png");

	// FBXにテクスチャが内包されている場合
	for (unsigned int texture_index = 0; texture_index < model->AiScene->mNumTextures; texture_index++)
{
    aiTexture* tex = model->AiScene->mTextures[texture_index];

    ID3D11ShaderResourceView* srv = nullptr;

    // ★ 情況 1：PNG / JPG 壓縮檔
    if (tex->mHeight == 0)
    {
        ID3D11Resource* res = nullptr;

        CreateWICTextureFromMemory(
            Direct3D_GetDevice(),
            Direct3D_GetContext(),
            (const uint8_t*)tex->pcData,
            tex->mWidth,
            &res,
            &srv);

        if (res) res->Release();
    }
    // ★ 情況 2：RAW RGBA（這就是你現在炸的原因）
    else
    {
		std::vector<unsigned char> rgba(tex->mWidth* tex->mHeight * 4);
		for (unsigned int pixel_index = 0; pixel_index < tex->mWidth * tex->mHeight; pixel_index++) {
			rgba[pixel_index * 4 + 0] = tex->pcData[pixel_index].r;
			rgba[pixel_index * 4 + 1] = tex->pcData[pixel_index].g;
			rgba[pixel_index * 4 + 2] = tex->pcData[pixel_index].b;
			rgba[pixel_index * 4 + 3] = tex->pcData[pixel_index].a;
		} 
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = tex->mWidth;
		desc.Height = tex->mHeight;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;


        
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA data{};
        //data.pSysMem = tex->pcData;
		data.pSysMem = rgba.data();
        data.SysMemPitch = tex->mWidth * 4;

        ID3D11Texture2D* texture = nullptr;
        Direct3D_GetDevice()->CreateTexture2D(&desc, &data, &texture);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        Direct3D_GetDevice()->CreateShaderResourceView(texture, &srvDesc, &srv);

        texture->Release();
    }

    assert(srv);
    model->Texture[tex->mFilename.C_Str()] = srv;
}

	// テクスチャ外部型
	for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
	{
		aiString filename;
		aiMaterial* pMaterial = model->AiScene->mMaterials[model->AiScene->mMeshes[m]->mMaterialIndex];

		if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &filename) == AI_SUCCESS)
		{
			OutputDebugStringA("FBX finding texture: ");
			OutputDebugStringA(filename.C_Str());
			OutputDebugStringA("\n");

			if (filename.length == 0)
			{
				continue;
			}
			if (model->Texture.count(filename.C_Str()))
			{
				continue;
			}
			ID3D11ShaderResourceView* pTextureSrv = nullptr;
			ID3D11Resource* resource = nullptr;

			//std::string texfilename = directory + "/" + filename.C_Str();

			// 取得 FBX 內部儲存的貼圖路徑
			std::string fbxTexPath(filename.C_Str());

			// 從完整路徑中只取出「檔名」
			size_t lastSlash = fbxTexPath.find_last_of("/\\");
			std::string justTheFilename = fbxTexPath;
			if (lastSlash != std::string::npos) {
				justTheFilename = fbxTexPath.substr(lastSlash + 1);
			}

			// 用 FBX 所在的目錄，組合上「檔名」
			std::string texfilename = directory + "/" + justTheFilename;

			int len = MultiByteToWideChar(CP_UTF8, 0, texfilename.c_str(), -1, nullptr, 0);
			wchar_t* pWideFilename = new wchar_t[len];
			MultiByteToWideChar(CP_UTF8, 0, texfilename.c_str(), -1, pWideFilename, len);

			CreateWICTextureFromFile(
				Direct3D_GetDevice(),
				Direct3D_GetContext(),
				pWideFilename,
				&resource,
				&pTextureSrv);



			delete[] pWideFilename;

			assert(pTextureSrv);

			resource->Release(); //!!!!!!

			model->Texture[filename.C_Str()] = pTextureSrv;
		}
		
	}
	
	return model;
}

void ModelRelease(MODEL* model)
{
	for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
	{
		model->VertexBuffer[m]->Release();
		model->IndexBuffer[m]->Release();
	}

	delete[] model->VertexBuffer;
	delete[] model->IndexBuffer;

	for (std::pair<const std::string, ID3D11ShaderResourceView*> pair : model->Texture)
	{
		pair.second->Release();
	}

	aiReleaseImport(model->AiScene);


	delete model;
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

void EnsureModelInstanceBuffer(size_t instance_count)
{
	if (g_pModelInstanceBuffer != nullptr && g_ModelInstanceBufferCapacity >= instance_count)
	{
		return;
	}

	SAFE_RELEASE(g_pModelInstanceBuffer);
	g_ModelInstanceBufferCapacity = std::max<size_t>(instance_count, 64);

	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.ByteWidth = static_cast<UINT>(sizeof(InstanceData) * g_ModelInstanceBufferCapacity);
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	Direct3D_GetDevice()->CreateBuffer(&desc, nullptr, &g_pModelInstanceBuffer);
}
}
	

void ModelUnlitDraw(MODEL* pModel,
					const DirectX::XMMATRIX& mtxWorld,
					RenderState render_state)
{
	const MaterialPass material_pass{
		MaterialType::Unlit,
		render_state
	};

	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	for (unsigned int m = 0; m < pModel->AiScene->mNumMeshes; m++)
	{
		aiString texture;
		aiMaterial* pMaterial = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
		pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texture);

		const bool has_bound_texture =
			texture != aiString("") &&
			pModel->Texture.count(texture.C_Str()) > 0 &&
			pModel->Texture[texture.C_Str()] != nullptr;

		if (has_bound_texture)
		{
			Direct3D_GetContext()->PSSetShaderResources(0, 1, &pModel->Texture[texture.C_Str()]);
			material_pass.begin(mtxWorld, { 1.0f, 1.0f, 1.0f, 1.0f });
		}
		else
		{
			Texture_SetTexture(g_WhiteTexId);
			aiColor3D diffuse;
			pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
			material_pass.begin(mtxWorld, { diffuse.r, diffuse.g, diffuse.b, 1.0f });
		}


		// マテリアル設定
		// aiMaterial* pMaterial = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];

		// 頂点バッファ設定
		UINT stride = sizeof(Vertex3D);
		UINT offset = 0;
		Direct3D_GetContext()->IASetVertexBuffers(0, 1, &pModel->VertexBuffer[m], &stride, &offset);

		// インデックスバッファ設定
		Direct3D_GetContext()->IASetIndexBuffer(pModel->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

		// 描画
		Direct3D_GetContext()->DrawIndexed(pModel->AiScene->mMeshes[m]->mNumFaces * 3, 0, 0);
	}

	material_pass.end();
}

void ModelDraw(MODEL* pModel, const DirectX::XMMATRIX& mtxWorld,float alpha)
{
	const bool uses_alpha_blend = alpha < 0.999f;
	const MaterialPass material_pass{
		MaterialType::Lit,
		RenderState{
			DepthMode::ReadWrite,
			uses_alpha_blend ? BlendMode::Alpha : BlendMode::Opaque,
			CullMode::Back
		}
	};

	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

   	for (unsigned int m = 0; m < pModel->AiScene->mNumMeshes; m++)
	{
		aiString texture;
		aiMaterial* pMaterial = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
		pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texture);

		const bool has_bound_texture =
			texture != aiString("") &&
			pModel->Texture.count(texture.C_Str()) > 0 &&
			pModel->Texture[texture.C_Str()] != nullptr;

		if (has_bound_texture)
		{
			Direct3D_GetContext()->PSSetShaderResources(0, 1, &pModel->Texture[texture.C_Str()]);
			material_pass.begin(mtxWorld, { 1.0f, 1.0f, 1.0f, alpha });
		}
		else
		{
			Texture_SetTexture(g_WhiteTexId);
			aiColor3D diffuse;
			pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
			material_pass.begin(mtxWorld, { diffuse.r, diffuse.g, diffuse.b, alpha });
		}


		// マテリアル設定
		// aiMaterial* pMaterial = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];

		// 頂点バッファ設定
		UINT stride = sizeof(Vertex3D);
		UINT offset = 0;
		Direct3D_GetContext()->IASetVertexBuffers(0, 1, &pModel->VertexBuffer[m], &stride, &offset);

		// インデックスバッファ設定
		Direct3D_GetContext()->IASetIndexBuffer(pModel->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

		// 描画
		Direct3D_GetContext()->DrawIndexed(pModel->AiScene->mMeshes[m]->mNumFaces * 3, 0, 0);
	}

	material_pass.end();
}

void ModelDrawInstanced(MODEL* pModel,
						const std::vector<DirectX::XMFLOAT4X4>& world_matrices,
						float alpha,
						RenderState render_state)
{
	if (pModel == nullptr || world_matrices.empty())
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

	EnsureModelInstanceBuffer(instance_data.size());

	D3D11_MAPPED_SUBRESOURCE mapped{};
	Direct3D_GetContext()->Map(g_pModelInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, instance_data.data(), sizeof(InstanceData) * instance_data.size());
	Direct3D_GetContext()->Unmap(g_pModelInstanceBuffer, 0);

	const BlendMode blend_mode =
		(alpha < 0.999f && render_state.blend_mode == BlendMode::Opaque)
			? BlendMode::Alpha
			: render_state.blend_mode;
	if (blend_mode == BlendMode::Alpha)
	{
		Direct3D_SetAlphaBlendTransparent();
	}
	else if (blend_mode == BlendMode::Additive)
	{
		Direct3D_SetAlphaBlendAdd();
	}
	else
	{
		Direct3D_SetBlendStateDisable();
	}

	Direct3D_SetDepthEnable(render_state.depth_mode != DepthMode::Disabled);
	Direct3D_SetDepthWrite(render_state.depth_mode == DepthMode::ReadWrite);
	switch (render_state.cull_mode)
	{
	case CullMode::None:
		Direct3D_SetCullMode(Direct3DCullMode::None);
		break;
	case CullMode::Front:
		Direct3D_SetCullMode(Direct3DCullMode::Front);
		break;
	case CullMode::Back:
	default:
		Direct3D_SetCullMode(Direct3DCullMode::Back);
		break;
	}

	Shader3DInstanced_Begin();
	Shader3DInstanced_SetMaterialColor({ 1.0f, 1.0f, 1.0f, alpha });

	ID3D11DeviceContext* ctx = Direct3D_GetContext();
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (unsigned int m = 0; m < pModel->AiScene->mNumMeshes; ++m)
	{
		aiString texture;
		aiMaterial* pMaterial = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
		pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texture);

		XMFLOAT4 material_color = { 1.0f, 1.0f, 1.0f, alpha };
		const bool has_bound_texture =
			texture != aiString("") &&
			pModel->Texture.count(texture.C_Str()) > 0 &&
			pModel->Texture[texture.C_Str()] != nullptr;

		if (has_bound_texture)
		{
			ctx->PSSetShaderResources(0, 1, &pModel->Texture[texture.C_Str()]);
		}
		else
		{
			Texture_SetTexture(g_WhiteTexId);
			aiColor3D diffuse;
			pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
			material_color = { diffuse.r, diffuse.g, diffuse.b, alpha };
		}

		Shader3DInstanced_SetMaterialColor(material_color);

		UINT strides[2] = { sizeof(Vertex3D), sizeof(InstanceData) };
		UINT offsets[2] = { 0, 0 };
		ID3D11Buffer* buffers[2] = { pModel->VertexBuffer[m], g_pModelInstanceBuffer };
		ctx->IASetVertexBuffers(0, 2, buffers, strides, offsets);
		ctx->IASetIndexBuffer(pModel->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);
		ctx->DrawIndexedInstanced(
			pModel->AiScene->mMeshes[m]->mNumFaces * 3,
			static_cast<UINT>(world_matrices.size()),
			0,
			0,
			0);
	}

	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);
	Direct3D_SetCullMode(Direct3DCullMode::Back);
	Direct3D_SetBlendStateDisable();
}

void ModelDrawGrassInstanced(MODEL* pModel,
							 const std::vector<DirectX::XMFLOAT4X4>& world_matrices,
							 ID3D11ShaderResourceView* wind_field_srv,
							 float time_seconds,
							 float field_uv_scale,
							 float bend_scale,
							 float alpha,
							 RenderState render_state)
{
	if (pModel == nullptr || world_matrices.empty())
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

	EnsureModelInstanceBuffer(instance_data.size());

	D3D11_MAPPED_SUBRESOURCE mapped{};
	Direct3D_GetContext()->Map(g_pModelInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, instance_data.data(), sizeof(InstanceData) * instance_data.size());
	Direct3D_GetContext()->Unmap(g_pModelInstanceBuffer, 0);

	const BlendMode blend_mode =
		(alpha < 0.999f && render_state.blend_mode == BlendMode::Opaque)
			? BlendMode::Alpha
			: render_state.blend_mode;
	if (blend_mode == BlendMode::Alpha)
	{
		Direct3D_SetAlphaBlendTransparent();
	}
	else if (blend_mode == BlendMode::Additive)
	{
		Direct3D_SetAlphaBlendAdd();
	}
	else
	{
		Direct3D_SetBlendStateDisable();
	}

	Direct3D_SetDepthEnable(render_state.depth_mode != DepthMode::Disabled);
	Direct3D_SetDepthWrite(render_state.depth_mode == DepthMode::ReadWrite);
	switch (render_state.cull_mode)
	{
	case CullMode::None:
		Direct3D_SetCullMode(Direct3DCullMode::None);
		break;
	case CullMode::Front:
		Direct3D_SetCullMode(Direct3DCullMode::Front);
		break;
	case CullMode::Back:
	default:
		Direct3D_SetCullMode(Direct3DCullMode::Back);
		break;
	}

	Backend::DX11::Sampler::SetLinearFilter();
	ShaderGrassInstanced_SetWindField(wind_field_srv);
	ShaderGrassInstanced_SetWindSettings(time_seconds, field_uv_scale, bend_scale);
	ShaderGrassInstanced_Begin();
	ShaderGrassInstanced_SetMaterialColor({ 1.0f, 1.0f, 1.0f, alpha });

	ID3D11DeviceContext* ctx = Direct3D_GetContext();
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (unsigned int mesh_index = 0; mesh_index < pModel->AiScene->mNumMeshes; ++mesh_index)
	{
		aiString texture;
		aiMaterial* pMaterial = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[mesh_index]->mMaterialIndex];
		pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texture);

		XMFLOAT4 material_color = { 1.0f, 1.0f, 1.0f, alpha };
		const bool has_bound_texture =
			texture != aiString("") &&
			pModel->Texture.count(texture.C_Str()) > 0 &&
			pModel->Texture[texture.C_Str()] != nullptr;

		if (has_bound_texture)
		{
			ctx->PSSetShaderResources(0, 1, &pModel->Texture[texture.C_Str()]);
		}
		else
		{
			Texture_SetTexture(g_WhiteTexId);
			aiColor3D diffuse;
			pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
			material_color = { diffuse.r, diffuse.g, diffuse.b, alpha };
		}

		ShaderGrassInstanced_SetMaterialColor(material_color);

		UINT strides[2] = { sizeof(Vertex3D), sizeof(InstanceData) };
		UINT offsets[2] = { 0, 0 };
		ID3D11Buffer* buffers[2] = { pModel->VertexBuffer[mesh_index], g_pModelInstanceBuffer };
		ctx->IASetVertexBuffers(0, 2, buffers, strides, offsets);
		ctx->IASetIndexBuffer(pModel->IndexBuffer[mesh_index], DXGI_FORMAT_R32_UINT, 0);
		ctx->DrawIndexedInstanced(
			pModel->AiScene->mMeshes[mesh_index]->mNumFaces * 3,
			static_cast<UINT>(world_matrices.size()),
			0,
			0,
			0);
	}

	ShaderGrassInstanced_Clear();
	Direct3D_SetDepthEnable(true);
	Direct3D_SetDepthWrite(true);
	Direct3D_SetCullMode(Direct3DCullMode::Back);
	Direct3D_SetBlendStateDisable();
}

void ModelDrawShadow(MODEL* pModel, const DirectX::XMMATRIX& mtxWorld)
{
	if (!pModel) return;

	ShaderShadow_SetWorldMatrix(mtxWorld);

	ID3D11DeviceContext* ctx = Direct3D_GetContext();
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (unsigned int m = 0; m < pModel->AiScene->mNumMeshes; m++)
	{
		UINT stride = sizeof(Vertex3D);
		UINT offset = 0;
		ctx->IASetVertexBuffers(0, 1, &pModel->VertexBuffer[m], &stride, &offset);
		ctx->IASetIndexBuffer(pModel->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);
		ctx->DrawIndexed(pModel->AiScene->mMeshes[m]->mNumFaces * 3, 0, 0);
	}
}

void ModelToonDraw(MODEL* pModel, const DirectX::XMMATRIX& mtxWorld, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj, const DirectX::XMFLOAT4& color)
{
	extern int g_ToonStepCount;

	if (!pModel) return;

	ShaderToon_SetOutlineWidth(0.02f);

	ShaderToon_SetWorldMatrix(mtxWorld);

	ShaderToon_SetViewMatrix(view);

	ShaderToon_SetProjMatrix(proj);

	Direct3D_SetDepthEnable(true);


	ShaderToon_SetMaterialColor(color, g_ToonStepCount);

	ShaderToon_Begin_Main();

	ID3D11DeviceContext* ctx = Direct3D_GetContext();
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	for (unsigned int m = 0; m < pModel->AiScene->mNumMeshes; m++)
	{
		aiString texture;
		aiMaterial* pMaterial = pModel->AiScene->mMaterials[pModel->AiScene->mMeshes[m]->mMaterialIndex];
		pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texture);

		if (texture != aiString(""))
		{
			ctx->PSSetShaderResources(0, 1, &pModel->Texture[texture.data]);
			ShaderToon_SetMaterialColor(color, g_ToonStepCount);
		}
		else
		{
			Texture_SetTexture(g_WhiteTexId);

			aiColor3D diffuse;
			pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
			ShaderToon_SetMaterialColor({ diffuse.r * color.x, diffuse.g * color.y, diffuse.b * color.z, 1 }, g_ToonStepCount);
		}


		UINT stride = sizeof(Vertex3D);
		UINT offset = 0;
		ctx->IASetVertexBuffers(0, 1, &pModel->VertexBuffer[m], &stride, &offset);
		ctx->IASetIndexBuffer(pModel->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

		ctx->DrawIndexed(pModel->AiScene->mMeshes[m]->mNumFaces * 3, 0, 0);
	}

	// outline
	ShaderToon_SetMaterialColor({ 0.0f, 0.0f, 0.0f, 1.0f }, g_ToonStepCount);

	ShaderToon_SetOutlineColor({ 0.0f, 0.0f, 0.0f, 1.0f });

	ShaderToon_Begin_Outline();

	ModelDrawRaw(pModel);

	Direct3D_SetDepthEnable(false);

	Direct3D_GetContext()->RSSetState(nullptr); // reset rss
}

Collision::AABB Model_GetAABB(MODEL* model, const DirectX::XMFLOAT3& position)
{
	return {
		{ position.x + model->localAABB.min.x,
		  position.y + model->localAABB.min.y,
		  position.z + model->localAABB.min.z,
		},
		{ position.x + model->localAABB.max.x,
		  position.y + model->localAABB.max.y,
		  position.z + model->localAABB.max.z}
	};
}


void ModelDrawRaw(MODEL* pModel)
{
	auto ctx = Direct3D_GetContext();

	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (unsigned int m = 0; m < pModel->AiScene->mNumMeshes; m++)
	{
		UINT stride = sizeof(Vertex3D);
		UINT offset = 0;

		ctx->IASetVertexBuffers(0, 1, &pModel->VertexBuffer[m], &stride, &offset);
		ctx->IASetIndexBuffer(pModel->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

		ctx->DrawIndexed(
			pModel->AiScene->mMeshes[m]->mNumFaces * 3,
			0, 0
		);
	}
}

