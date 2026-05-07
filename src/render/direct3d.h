/*==============================================================================

   Direct3Dの初期化関連 [direct3d.cpp]
														 Author : Youhei Sato
														 Date   : 2025/05/12
--------------------------------------------------------------------------------

==============================================================================*/
#ifndef DIRECT3D_H
#define DIRECT3D_H


#include <Windows.h>
#include <d3d11.h>
#include <DirectXMath.h>

enum class Direct3DCullMode
{
	None,
	Front,
	Back
};


// セーフリリースマクロ
#define SAFE_RELEASE(o) if (o) { (o)->Release(); o = NULL; }


bool Direct3D_Initialize(HWND hWnd); // Direct3Dの初期化
void Direct3D_Finalize(); // Direct3Dの終了処理


void Direct3D_Present(); // バックバッファの表示
void Direct3D_SetVSyncEnabled(bool enabled);
bool Direct3D_IsVSyncEnabled();

unsigned int Direct3D_GetBackBufferWidth();
unsigned int Direct3D_GetBackBufferHeight();

ID3D11Device* Direct3D_GetDevice();
ID3D11DeviceContext* Direct3D_GetContext();

//αブレンド設定関数
void Direct3D_SetAlphaBlendTransparent();	//透過処理
void Direct3D_SetAlphaBlendAdd();			//加算合成
void Direct3D_SetBlendStateDisable();		//ブレンド無効
void Direct3D_SetCullMode(Direct3DCullMode cull_mode);

// 深度バッファの設定
void Direct3D_SetDepthEnable(bool enable); // 深度バッファ無効

void Direct3D_SetDepthWrite(bool enable);

//ID3D11ShaderResourceView* Direct3D_GetOffscreenSRV();

//ビューポート行列の作成
DirectX::XMMATRIX Direct3D_MatrixViewport();

//スクリーン座標 + 3D座標 変換行列の作成
DirectX::XMFLOAT3 Direct3D_ScreenToWorld(
	int x,
	int y,
	float depth,
	const DirectX::XMFLOAT4X4 & view,
	const DirectX::XMFLOAT4X4 & projection
);

// 3D座標変換 -> スクリーン座標
DirectX::XMFLOAT2 Direct3D_WorldToScreen(
	const DirectX::XMFLOAT3& position,
	const DirectX::XMFLOAT4X4& view,
	const DirectX::XMFLOAT4X4& projection
);

// バックバッファのクリア
void Direct3D_ClearBackbuffer();

// バックバッファのレンダリングに切り替える
void Direct3D_SetBackbuffer();

void Direct3D_UnbindRenderTargets();

void Direct3D_SetSceneRenderTarget();
void Direct3D_SetSceneColorOnlyRenderTarget();
void Direct3D_SetMiniMapRenderTarget();
void Direct3D_SetPlayerRenderTarget();

void Direct3D_ClearScene();
void Direct3D_ClearSceneColor();
void Direct3D_ClearSceneDepth();
void Direct3D_ClearMiniMap();
void Direct3D_ClearPlayer();

void Direct3D_SetCustomRenderTarget(ID3D11RenderTargetView * pRTV, ID3D11DepthStencilView * pDSV);

ID3D11ShaderResourceView* Direct3D_GetSceneSRV();
ID3D11ShaderResourceView* Direct3D_GetSceneDepthSRV();
ID3D11DepthStencilView* Direct3D_GetSceneDSV();

ID3D11ShaderResourceView* Direct3D_GetMiniMapSRV();
ID3D11DepthStencilView* Direct3D_GetMiniMapDSV();

ID3D11ShaderResourceView* Direct3D_GetPlayerSRV();
ID3D11DepthStencilView* Direct3D_GetPlayerDSV();














// オフスクリーンレンダリングテクスチャのクリア
//void Direct3D_ClearOffscreen();

// テクスチャへのレンダリングに切り替える
//void Direct3D_SetOffscreen();

// オフスクリーンレンダリングテクスチャの設定
//void Direct3D_SetOffscreenTexture(int slot);

#endif // DIRECT3D_H

