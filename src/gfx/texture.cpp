// ----------------------------------------------------
// テクスチャ描画 [texture.cpp]
// ====================================================
// Created by: Jerry
// Date: 2025-06-18
// Version: 1.0
// ----------------------------------------------------

#include "texture.h"
#include "direct3d.h"
#include <string>
#include "WICTextureLoader11.h"
using namespace DirectX;

static constexpr int TEXTURE_MAX = 1024; // テクスチャ管理最大数

namespace Backend::DX11
{
	struct NativeTextureResource
	{
		std::wstring filename;
		unsigned int width = 0;
		unsigned int height = 0;
		ID3D11Resource* pTexture = nullptr;
		ID3D11ShaderResourceView* pTextureView = nullptr;
	};


	static NativeTextureResource g_Textures[TEXTURE_MAX]{};
	static int g_SetTextureIndex = -1;

	// DX11 native objects stay in backend scope.
	static ID3D11Device* g_pDevice = nullptr;
	static ID3D11DeviceContext* g_pContext = nullptr;
}


void TextureManager::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	for (Backend::DX11::NativeTextureResource& t : Backend::DX11::g_Textures){
		t.pTexture = nullptr;
		t.pTextureView = nullptr;
		t.filename.clear();
		t.width = 0;
		t.height = 0;
	}

	Backend::DX11::g_SetTextureIndex = -1;

	// デバイスとデバイスコンテキストの保存
	Backend::DX11::g_pDevice = pDevice;
	Backend::DX11::g_pContext = pContext;
}

void TextureManager::Finalize()
{
	TextureManager::ReleaseAll();
}

int TextureManager::Load(const wchar_t* pFiliname)
{
	// すでに読み込んだファイルは読み込まない
	for (int i = 0; i < TEXTURE_MAX; i++) {
		if (Backend::DX11::g_Textures[i].filename == pFiliname) {
			return i;
		}
	}

	// 空いている管理領域を探す
	for (int i = 0; i < TEXTURE_MAX; i++)
	{

		if (Backend::DX11::g_Textures[i].pTexture) continue; // 使用中

		HRESULT hr;

		hr = CreateWICTextureFromFile(
			Backend::DX11::g_pDevice,
			Backend::DX11::g_pContext,
			pFiliname,
			&Backend::DX11::g_Textures[i].pTexture,
			&Backend::DX11::g_Textures[i].pTextureView);
		
		ID3D11Texture2D* pTexture = (ID3D11Texture2D*)Backend::DX11::g_Textures[i].pTexture;
		D3D11_TEXTURE2D_DESC t2desc;
		pTexture->GetDesc(&t2desc);
		Backend::DX11::g_Textures[i].width = t2desc.Width;
		Backend::DX11::g_Textures[i].height = t2desc.Height;

		// テクスチャの読み込み
		//TexMetadata metadata;
		//ScratchImage image;
		//
		//HRESULT hr = LoadFromWICFile(pFiliname, WIC_FLAGS_NONE, &metadata, image);

		if (FAILED(hr)) {
			MessageBoxW(nullptr, L"テクスチャの初期化に失敗しました", pFiliname, MB_OK | MB_ICONERROR);
			return -1;
		}

		Backend::DX11::g_Textures[i].filename = pFiliname;
		//g_Textures[i].width = (unsigned int) metadata.width;
		//g_Textures[i].height = (unsigned int) metadata.height;

		//hr = CreateShaderResourceView(g_pDevice,
		//	image.GetImages(), image.GetImageCount(), metadata, &g_Textures[i].pTexture);
		return i;
	}

	return -1; // 管理領域が満杯になったら返す
}

void TextureManager::ReleaseAll()
{
	for (Backend::DX11::NativeTextureResource& t : Backend::DX11::g_Textures) {
		t.filename.clear();
		SAFE_RELEASE(t.pTexture);
		SAFE_RELEASE(t.pTextureView);
		t.width = 0;
		t.height = 0;
	}
}

void TextureManager::SetTexture(int texid, int slot)
{
	//if (texid < 0) return;
	//
	//g_SetTextureIndex = texid;
	//
	//// テクスチャ設定
	//g_pContext->PSSetShaderResources(slot, 1, & g_Textures[texid].pTextureView);

	if (texid < 0)
	{
		ID3D11ShaderResourceView* nullSRV = nullptr;
		// 將該 Slot 設定為 NULL (清空貼圖)
		Backend::DX11::g_pContext->PSSetShaderResources(slot, 1, &nullSRV);
		Backend::DX11::g_SetTextureIndex = -1;
		return;
	}

	Backend::DX11::g_SetTextureIndex = texid;

	// テクスチャ設定
	Backend::DX11::g_pContext->PSSetShaderResources(
		slot, 1, &Backend::DX11::g_Textures[texid].pTextureView);
}

void TextureManager::SetExternalSRV(ID3D11ShaderResourceView* srv, int slot)
{
	Backend::DX11::g_SetTextureIndex = -1;
	Backend::DX11::g_pContext->PSSetShaderResources(slot, 1, &srv);
}


unsigned int TextureManager::Width(int texid)
{
	if (texid < 0) return 0;

	return Backend::DX11::g_Textures[texid].width;
}

unsigned int TextureManager::Height(int texid)
{
	if (texid < 0) return 0;

	return Backend::DX11::g_Textures[texid].height;
}

ID3D11ShaderResourceView* TextureManager::GetSRV(int texture_id)
{
	if (texture_id < 0 || texture_id >= TEXTURE_MAX)
	{
		return nullptr;
	}

	if (Backend::DX11::g_Textures[texture_id].pTextureView == nullptr)
	{
		return nullptr;
	}

	return Backend::DX11::g_Textures[texture_id].pTextureView;
}

void Texture_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	TextureManager::Initialize(pDevice, pContext);
}

void Texture_Finalize(void)
{
	TextureManager::Finalize();
}

int Texture_Load(const wchar_t* pFiliname)
{
	return TextureManager::Load(pFiliname);
}

void Texture_AllRelease()
{
	TextureManager::ReleaseAll();
}

void Texture_SetTexture(int texid, int slot)
{
	TextureManager::SetTexture(texid, slot);
}

void Texture_SetExternalSRV(ID3D11ShaderResourceView* srv, int slot)
{
	TextureManager::SetExternalSRV(srv, slot);
}

unsigned int Texture_Width(int texid)
{
	return TextureManager::Width(texid);
}

unsigned int Texture_Height(int texid)
{
	return TextureManager::Height(texid);
}

ID3D11ShaderResourceView* Texture_GetSRV(int texture_id)
{
	return TextureManager::GetSRV(texture_id);
}

