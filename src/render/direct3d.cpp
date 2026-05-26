// --------------------------------------------
// Direct3Dの初期化関連 [direct3d.cpp]
// ============================================
// Created by: Jerry
// Date: 2025-06-18
// Version: 1.0
// --------------------------------------------
#include <d3d11.h>
#include "direct3d.h"
#include "debug_ostream.h"
using namespace DirectX;

#pragma comment(lib, "d3d11.lib")
// #pragma comment(lib, "dxgi.lib")

#if defined(DEBUG) || defined(_DEBUG)
	#pragma comment(lib, "DirectXTex_Debug.lib")
#else
	#pragma comment(lib, "DirectXTex_Release.lib")
#endif

namespace Backend::DX11
{
/* 各種インターフェース */
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11BlendState* g_pBlendStateMultiply = nullptr;
static ID3D11BlendState* g_pBlendStateAdd = nullptr;
static ID3D11DepthStencilState* g_pDepthStencilStateDepthDisable = nullptr;
static ID3D11DepthStencilState* g_pDepthStencilStateDepthEnable = nullptr;
static ID3D11DepthStencilState* g_pDepthStencilStateDepthWriteDisable = nullptr;
static ID3D11RasterizerState* g_pRasterizerStateCullBack = nullptr;
static ID3D11RasterizerState* g_pRasterizerStateCullFront = nullptr;
static ID3D11RasterizerState* g_pRasterizerStateCullNone = nullptr;
static bool g_VSyncEnabled = false;

/* バックバッファ関連 */
static ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
static ID3D11Texture2D* g_pDepthStencilBuffer = nullptr;
static ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
static D3D11_TEXTURE2D_DESC g_BackBufferDesc{};
static D3D11_VIEWPORT g_Viewport{};

static bool configureBackBuffer(); // バックバッファの設定・生成
static void releaseBackBuffer(); // バックバッファの解放

///* オフスクリーンレンダリング関連 */
//static ID3D11Texture2D* g_pOffscreenBuffer = nullptr;
//static ID3D11RenderTargetView* g_pOffscreenRenderTargetView = nullptr;
//static ID3D11ShaderResourceView* g_pOffscreenShaderResourceView = nullptr;
//static ID3D11Texture2D* g_pOffscreenDepthStencilBuffer = nullptr;
//static ID3D11DepthStencilView* g_pOffscreenDepthStencilView = nullptr;
//static D3D11_TEXTURE2D_DESC g_OffscreenDesc{};
//static D3D11_VIEWPORT g_OffscreenViewport{};

/* オフスクリーンレンダリング(scene) */
static ID3D11Texture2D* g_pSceneTex = nullptr;
static ID3D11RenderTargetView* g_pSceneRTV = nullptr;
static ID3D11ShaderResourceView* g_pSceneSRV = nullptr;
static ID3D11Texture2D* g_pSceneDepthTex = nullptr;
static ID3D11DepthStencilView* g_pSceneDSV = nullptr;
static ID3D11DepthStencilView* g_pSceneReadOnlyDSV = nullptr;
static ID3D11ShaderResourceView* g_pSceneDepthSRV = nullptr;
static ID3D11Texture2D* g_pSceneMSAATex = nullptr;
static ID3D11RenderTargetView* g_pSceneMSAARTV = nullptr;
static ID3D11Texture2D* g_pSceneMSAADepthTex = nullptr;
static ID3D11DepthStencilView* g_pSceneMSAADSV = nullptr;
static UINT g_SceneMSAASampleCount = 1;
static UINT g_SceneMSAAQuality = 0;
static bool g_SceneMSAAEnabled = false;
static D3D11_VIEWPORT            g_SceneViewport = {};

/* オフスクリーンレンダリング(minimap) */
static ID3D11Texture2D* g_pMiniMapTex = nullptr;
static ID3D11RenderTargetView* g_pMiniMapRTV = nullptr;
static ID3D11ShaderResourceView* g_pMiniMapSRV = nullptr;
static ID3D11Texture2D* g_pMiniMapDepthTex = nullptr;
static ID3D11DepthStencilView* g_pMiniMapDSV = nullptr;
static D3D11_VIEWPORT            g_MiniMapViewport = {};

/* オフスクリーンレンダリング(プレイヤー) */
static ID3D11Texture2D* g_pPlayerTex = nullptr;
static ID3D11RenderTargetView* g_pPlayerRTV = nullptr;
static ID3D11ShaderResourceView* g_pPlayerSRV = nullptr;
static ID3D11Texture2D* g_pPlayerDepthTex = nullptr;
static ID3D11DepthStencilView* g_pPlayerDSV = nullptr;
static D3D11_VIEWPORT            g_PlayerViewport = {};


//ID3D11ShaderResourceView* Direct3D_GetOffscreenSRV()
//{
//	return g_pOffscreenShaderResourceView;
//}

//static bool configureOffscreenBuffer(); // オフスクリーンバッファの設定・生成
//static void releaseOffscreenBuffer(); // オフスクリーンバッファの解放
static constexpr UINT kRequestedSceneMSAASamples = 4;
static constexpr DXGI_FORMAT kSceneColorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
static void ConfigureSceneMSAA();
static bool CreateSceneMSAATargets(int width, int height);
static void ReleaseSceneMSAATargets();
static bool CreateSceneReadOnlyDepthView();
static bool MakeRenderTarget(int w, int h,
	ID3D11Texture2D** ppTex,
	ID3D11RenderTargetView** ppRTV,
	ID3D11ShaderResourceView** ppSRV,
	ID3D11Texture2D** ppDepthTex,
	ID3D11DepthStencilView** ppDSV,
	D3D11_VIEWPORT* pViewport);
static bool MakeRenderTarget(int w, int h,
	ID3D11Texture2D** ppTex,
	ID3D11RenderTargetView** ppRTV,
	ID3D11ShaderResourceView** ppSRV,
	ID3D11Texture2D** ppDepthTex,
	ID3D11DepthStencilView** ppDSV,
	ID3D11ShaderResourceView** ppDepthSRV,
	D3D11_VIEWPORT* pViewport);
static bool MakeRenderTarget(int w, int h,
	ID3D11Texture2D** ppTex,
	ID3D11RenderTargetView** ppRTV,
	ID3D11ShaderResourceView** ppSRV,
	ID3D11Texture2D** ppDepthTex,
	ID3D11DepthStencilView** ppDSV,
	DXGI_FORMAT colorFormat,
	ID3D11ShaderResourceView** ppDepthSRV,
	D3D11_VIEWPORT* pViewport);
static bool MakeRenderTarget(int w, int h,
	ID3D11Texture2D** ppTex,
	ID3D11RenderTargetView** ppRTV,
	ID3D11ShaderResourceView** ppSRV,
	ID3D11Texture2D** ppDepthTex,
	ID3D11DepthStencilView** ppDSV,
	D3D11_VIEWPORT* pViewport)
{
	return MakeRenderTarget(w, h, ppTex, ppRTV, ppSRV, ppDepthTex, ppDSV, nullptr, pViewport);
}
static bool MakeRenderTarget(int w, int h,
	ID3D11Texture2D** ppTex,
	ID3D11RenderTargetView** ppRTV,
	ID3D11ShaderResourceView** ppSRV,
	ID3D11Texture2D** ppDepthTex,
	ID3D11DepthStencilView** ppDSV,
	ID3D11ShaderResourceView** ppDepthSRV,
	D3D11_VIEWPORT* pViewport)
{
	return MakeRenderTarget(
		w,
		h,
		ppTex,
		ppRTV,
		ppSRV,
		ppDepthTex,
		ppDSV,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		ppDepthSRV,
		pViewport);
}
static bool MakeRenderTarget(int w, int h,
	ID3D11Texture2D** ppTex,
	ID3D11RenderTargetView** ppRTV,
	ID3D11ShaderResourceView** ppSRV,
	ID3D11Texture2D** ppDepthTex,
	ID3D11DepthStencilView** ppDSV,
	DXGI_FORMAT colorFormat,
	ID3D11ShaderResourceView** ppDepthSRV,
	D3D11_VIEWPORT* pViewport)
{
	HRESULT hr;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = w;
	texDesc.Height = h;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = colorFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	hr = g_pDevice->CreateTexture2D(&texDesc, nullptr, ppTex);
	if (FAILED(hr)) return false;

	hr = g_pDevice->CreateRenderTargetView(*ppTex, nullptr, ppRTV);
	if (FAILED(hr)) return false;

	hr = g_pDevice->CreateShaderResourceView(*ppTex, nullptr, ppSRV);
	if (FAILED(hr)) return false;

	// デプスステンシルバッファの生成
	D3D11_TEXTURE2D_DESC depthDesc = texDesc;
	depthDesc.Format = ppDepthSRV != nullptr ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | (ppDepthSRV != nullptr ? D3D11_BIND_SHADER_RESOURCE : 0);

	hr = g_pDevice->CreateTexture2D(&depthDesc, nullptr, ppDepthTex);
	if (FAILED(hr)) return false;

	D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
	dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsv_desc.Texture2D.MipSlice = 0;
	hr = g_pDevice->CreateDepthStencilView(*ppDepthTex, &dsv_desc, ppDSV);
	if (FAILED(hr)) return false;

	if (ppDepthSRV != nullptr)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC depth_srv_desc = {};
		depth_srv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		depth_srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		depth_srv_desc.Texture2D.MipLevels = 1;
		hr = g_pDevice->CreateShaderResourceView(*ppDepthTex, &depth_srv_desc, ppDepthSRV);
		if (FAILED(hr)) return false;
	}

	// ビューポートの設定
	pViewport->TopLeftX = 0.0f;
	pViewport->TopLeftY = 0.0f;
	pViewport->Width = static_cast<FLOAT>(w);
	pViewport->Height = static_cast<FLOAT>(h);
	pViewport->MinDepth = 0.0f;
	pViewport->MaxDepth = 1.0f;

	return true;
}

static bool AreSceneMSAATargetsReady()
{
	return g_SceneMSAASampleCount > 1 &&
		g_pSceneMSAATex != nullptr &&
		g_pSceneMSAARTV != nullptr &&
		g_pSceneMSAADepthTex != nullptr &&
		g_pSceneMSAADSV != nullptr;
}

static bool IsSceneMSAAActive()
{
	return g_SceneMSAAEnabled && AreSceneMSAATargetsReady();
}

static ID3D11RenderTargetView* ActiveSceneRTV()
{
	return IsSceneMSAAActive() ? g_pSceneMSAARTV : g_pSceneRTV;
}

static ID3D11DepthStencilView* ActiveSceneDSV()
{
	return IsSceneMSAAActive() ? g_pSceneMSAADSV : g_pSceneDSV;
}

static void UnbindSceneShaderResources()
{
	ID3D11ShaderResourceView* null_srvs[8] = {
		nullptr, nullptr, nullptr, nullptr,
		nullptr, nullptr, nullptr, nullptr
	};
	g_pDeviceContext->VSSetShaderResources(0, 1, null_srvs);
	g_pDeviceContext->PSSetShaderResources(0, 8, null_srvs);
}

static void ConfigureSceneMSAA()
{
	g_SceneMSAASampleCount = 1;
	g_SceneMSAAQuality = 0;
	g_SceneMSAAEnabled = false;

	if (g_pDevice == nullptr)
	{
		return;
	}

	UINT color_quality = 0;
	UINT depth_quality = 0;
	const HRESULT color_result = g_pDevice->CheckMultisampleQualityLevels(
		kSceneColorFormat,
		kRequestedSceneMSAASamples,
		&color_quality);
	const HRESULT depth_result = g_pDevice->CheckMultisampleQualityLevels(
		DXGI_FORMAT_D24_UNORM_S8_UINT,
		kRequestedSceneMSAASamples,
		&depth_quality);

	if (SUCCEEDED(color_result) &&
		SUCCEEDED(depth_result) &&
		color_quality > 0 &&
		depth_quality > 0)
	{
		const UINT matched_quality = color_quality < depth_quality ? color_quality : depth_quality;
		g_SceneMSAASampleCount = kRequestedSceneMSAASamples;
		g_SceneMSAAQuality = matched_quality - 1;
		g_SceneMSAAEnabled = true;
	}
}

static void ReleaseSceneMSAATargets()
{
	SAFE_RELEASE(g_pSceneMSAADSV);
	SAFE_RELEASE(g_pSceneMSAADepthTex);
	SAFE_RELEASE(g_pSceneMSAARTV);
	SAFE_RELEASE(g_pSceneMSAATex);
}

static bool CreateSceneMSAATargets(int width, int height)
{
	ReleaseSceneMSAATargets();

	if (g_pDevice == nullptr || g_SceneMSAASampleCount <= 1)
	{
		return true;
	}

	D3D11_TEXTURE2D_DESC color_desc{};
	color_desc.Width = width;
	color_desc.Height = height;
	color_desc.MipLevels = 1;
	color_desc.ArraySize = 1;
	color_desc.Format = kSceneColorFormat;
	color_desc.SampleDesc.Count = g_SceneMSAASampleCount;
	color_desc.SampleDesc.Quality = g_SceneMSAAQuality;
	color_desc.Usage = D3D11_USAGE_DEFAULT;
	color_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

	if (FAILED(g_pDevice->CreateTexture2D(&color_desc, nullptr, &g_pSceneMSAATex)))
	{
		ReleaseSceneMSAATargets();
		return false;
	}

	if (FAILED(g_pDevice->CreateRenderTargetView(g_pSceneMSAATex, nullptr, &g_pSceneMSAARTV)))
	{
		ReleaseSceneMSAATargets();
		return false;
	}

	D3D11_TEXTURE2D_DESC depth_desc = color_desc;
	depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	if (FAILED(g_pDevice->CreateTexture2D(&depth_desc, nullptr, &g_pSceneMSAADepthTex)))
	{
		ReleaseSceneMSAATargets();
		return false;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
	dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;

	if (FAILED(g_pDevice->CreateDepthStencilView(g_pSceneMSAADepthTex, &dsv_desc, &g_pSceneMSAADSV)))
	{
		ReleaseSceneMSAATargets();
		return false;
	}

	return true;
}

static bool CreateSceneReadOnlyDepthView()
{
	SAFE_RELEASE(g_pSceneReadOnlyDSV);
	if (g_pDevice == nullptr || g_pSceneDepthTex == nullptr)
	{
		return false;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
	dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsv_desc.Texture2D.MipSlice = 0;
	dsv_desc.Flags = D3D11_DSV_READ_ONLY_DEPTH;
	return SUCCEEDED(g_pDevice->CreateDepthStencilView(
		g_pSceneDepthTex,
		&dsv_desc,
		&g_pSceneReadOnlyDSV));
}

static void ReleaseRenderTarget(
	ID3D11Texture2D*& pTex,
	ID3D11RenderTargetView*& pRTV,
	ID3D11ShaderResourceView*& pSRV,
	ID3D11Texture2D*& pDepthTex,
	ID3D11DepthStencilView*& pDSV)
{
	SAFE_RELEASE(pSRV);
	SAFE_RELEASE(pRTV);
	SAFE_RELEASE(pTex);
	SAFE_RELEASE(pDSV);
	SAFE_RELEASE(pDepthTex);
}
} // namespace Backend::DX11

using namespace Backend::DX11;


bool Direct3D_Initialize(HWND hWnd)
{
	/* デバイス、スワップチェーン、コンテキスト生成 */
	DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
	swap_chain_desc.Windowed = TRUE;	// full screen
	swap_chain_desc.BufferCount = 2;	// 裏画面が何個用意する
	// swap_chain_desc.BufferDesc.Width = 0;
	// swap_chain_desc.BufferDesc.Height = 0;
	// ⇒ ウィンドウサイズに合わせて自動的に設定される
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	// 色のformat
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;	// 何に使う、ここは絵を各場所で使う
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	//swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // ベンチマークの時はこっち
	swap_chain_desc.OutputWindow = hWnd;

	/*
	IDXGIFactory1* pFactory;
	CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
	IDXGIAdapter1* pAdapter;
	pFactory->EnumAdapters1(1, &pAdapter); // セカンダリアダプタを取得
	pFactory->Release();
	DXGI_ADAPTER_DESC1 desc;
	pAdapter->GetDesc1(&desc); // アダプタの情報を取得して確認したい場合
	pAdapter->Release(); // D3D11CreateDeviceAndSwapChain()の第１引数に渡して利用し終わったら解放する
	*/

	UINT device_flags = 0;

#if defined(DEBUG) || defined(_DEBUG)
	device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};

	D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		device_flags,
		levels,
		ARRAYSIZE(levels),
		D3D11_SDK_VERSION,
		&swap_chain_desc,
		&g_pSwapChain,	// 大事
		&g_pDevice,	// 大事
		&feature_level,
		&g_pDeviceContext	// 大事
	);

	if (FAILED(hr)) {
		MessageBox(hWnd, "Direct3Dの初期化に失敗しました", "エラー", MB_OK);
		return false;
	}

	ConfigureSceneMSAA();

	if (!configureBackBuffer()) {
		MessageBox(hWnd, "バックバッファの設定に失敗しました", "エラー", MB_OK);
		return false;
	}

	// オフスクリーン関連のリソース作成
	// メイン画面の裏で描画するためのバッファ
	int screenW = Direct3D_GetBackBufferWidth();
	int screenH = Direct3D_GetBackBufferHeight();
	if (!MakeRenderTarget(screenW, screenH,
		&g_pSceneTex, &g_pSceneRTV, &g_pSceneSRV, &g_pSceneDepthTex, &g_pSceneDSV,
		kSceneColorFormat, &g_pSceneDepthSRV, &g_SceneViewport))
		return false;
	if (!CreateSceneReadOnlyDepthView())
		return false;
	if (!CreateSceneMSAATargets(screenW, screenH))
	{
		hal::dout << "Direct3D_Initialize(): 4x MSAA scene target creation failed; falling back to single-sample scene rendering." << std::endl;
		g_SceneMSAASampleCount = 1;
		g_SceneMSAAQuality = 0;
		g_SceneMSAAEnabled = false;
		ReleaseSceneMSAATargets();
	}

	// ミニマップ用のオフスクリーンバッファ
	if (!MakeRenderTarget(512, 512,
		&g_pMiniMapTex, &g_pMiniMapRTV, &g_pMiniMapSRV, &g_pMiniMapDepthTex, &g_pMiniMapDSV, &g_MiniMapViewport))
		return false;
	// プレイヤー用のオフスクリーンバッファ
	if (!MakeRenderTarget(screenW, screenH,
		&g_pPlayerTex, &g_pPlayerRTV, &g_pPlayerSRV, &g_pPlayerDepthTex, &g_pPlayerDSV, &g_PlayerViewport))
		return false;

	// RGB A -> 好きに使っていい値、基本は透明の表現に使う
	// αテスト、αブレンド

	// ブレンドステート設定
	D3D11_BLEND_DESC bd = {};
	bd.AlphaToCoverageEnable = FALSE;
	bd.IndependentBlendEnable = FALSE;
	bd.RenderTarget[0].BlendEnable = TRUE;	// αブレンドするしない

	/*------- 透過ブレンドの設定----------*/
	// src ... ソース（今から描く絵（色）） dest　...　すでに絵描かれた絵（色）

	// RGB
	bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;	// 演算子
	// SrcRGB * SrcBlend + DestRGB * (1 - DestBlend)

	// A
	bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	// SrcBlendAlpha * 1 + DestBlendAlpha * 0

	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	g_pDevice->CreateBlendState(&bd, &g_pBlendStateMultiply);
	/*-----------------------------------*/

	/*------- 加算ブレンドの設定----------*/

	// src ... ソース（今から描く絵（色）） dest　...　すでに絵描かれた絵（色）

	// RGB
	bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	// SrcRGB * SrcBlend + DestRGB * (1 - DestBlend)
	// SrcBlendAlpha * 1 + DestBlendAlpha * 0

	g_pDevice->CreateBlendState(&bd, &g_pBlendStateAdd);

	/*-----------------------------------*/

	Direct3D_SetAlphaBlendTransparent();

	// HACK: 3Dの場合、関数化のほうがいい
	
	// 深度ステンシルステート設定
	D3D11_DEPTH_STENCIL_DESC dsd = {};
	dsd.DepthFunc = D3D11_COMPARISON_LESS;
	dsd.StencilEnable = FALSE;
	dsd.DepthEnable = FALSE; // 無効にする
	dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

	g_pDevice->CreateDepthStencilState(&dsd, &g_pDepthStencilStateDepthDisable);

	dsd.StencilEnable = FALSE;
	dsd.DepthEnable = TRUE;
	dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	g_pDevice->CreateDepthStencilState(&dsd, &g_pDepthStencilStateDepthEnable);

	
	dsd.StencilEnable = FALSE;
	dsd.DepthEnable = TRUE;
	// dsd.DepthFunc = D3D11_COMPARISON_ALWAYS; // 不管其他的深度　絶対描く
	dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	g_pDevice->CreateDepthStencilState(&dsd, &g_pDepthStencilStateDepthWriteDisable);


	Direct3D_SetDepthEnable(true);

	// ラスタライザステートの作成
	D3D11_RASTERIZER_DESC rd = {};
	rd.FillMode = D3D11_FILL_SOLID;
	//rd.FillMode = D3D11_FILL_WIREFRAME;
	rd.CullMode = D3D11_CULL_BACK;
	//rd.CullMode = D3D11_CULL_NONE;
	rd.DepthClipEnable = TRUE;
	rd.MultisampleEnable = g_SceneMSAASampleCount > 1;
	g_pDevice->CreateRasterizerState(&rd, &g_pRasterizerStateCullBack);

	rd.CullMode = D3D11_CULL_FRONT;
	g_pDevice->CreateRasterizerState(&rd, &g_pRasterizerStateCullFront);

	rd.CullMode = D3D11_CULL_NONE;
	g_pDevice->CreateRasterizerState(&rd, &g_pRasterizerStateCullNone);

	// デバイスコンテキストにラスタライザーステートを設定
	Direct3D_SetCullMode(Direct3DCullMode::Back);

	return true;
}

void Direct3D_Finalize()
{
	releaseBackBuffer();

	// Same as below
	SAFE_RELEASE(g_pRasterizerStateCullBack);
	SAFE_RELEASE(g_pRasterizerStateCullFront);
	SAFE_RELEASE(g_pRasterizerStateCullNone);
	SAFE_RELEASE(g_pDepthStencilStateDepthDisable);
	SAFE_RELEASE(g_pDepthStencilStateDepthEnable);
	SAFE_RELEASE(g_pBlendStateMultiply);
	SAFE_RELEASE(g_pBlendStateAdd);

	
	//releaseOffscreenBuffer();

	SAFE_RELEASE(g_pSwapChain);
	SAFE_RELEASE(g_pDeviceContext);
	SAFE_RELEASE(g_pDevice);

	SAFE_RELEASE(g_pSceneSRV);
	SAFE_RELEASE(g_pSceneRTV);
	SAFE_RELEASE(g_pSceneTex);
	SAFE_RELEASE(g_pSceneDepthSRV);
	SAFE_RELEASE(g_pSceneReadOnlyDSV);
	SAFE_RELEASE(g_pSceneDSV);
	SAFE_RELEASE(g_pSceneDepthTex);
	ReleaseSceneMSAATargets();

	SAFE_RELEASE(g_pMiniMapSRV);
	SAFE_RELEASE(g_pMiniMapRTV);
	SAFE_RELEASE(g_pMiniMapTex);
	SAFE_RELEASE(g_pMiniMapDSV);
	SAFE_RELEASE(g_pMiniMapDepthTex);

	SAFE_RELEASE(g_pPlayerSRV);
	SAFE_RELEASE(g_pPlayerRTV);
	SAFE_RELEASE(g_pPlayerTex);
	SAFE_RELEASE(g_pPlayerDSV);
	SAFE_RELEASE(g_pPlayerDepthTex);
}

void Direct3D_Resize(int width, int height) {
	if (g_pDevice == nullptr) return;

	releaseBackBuffer();
	ReleaseSceneMSAATargets();
	SAFE_RELEASE(g_pSceneDepthSRV);
	SAFE_RELEASE(g_pSceneReadOnlyDSV);
	ReleaseRenderTarget(g_pSceneTex, g_pSceneRTV, g_pSceneSRV, g_pSceneDepthTex, g_pSceneDSV);

	g_pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

	configureBackBuffer();

	MakeRenderTarget(width, height,
		&g_pSceneTex, &g_pSceneRTV, &g_pSceneSRV, &g_pSceneDepthTex, &g_pSceneDSV,
		kSceneColorFormat, &g_pSceneDepthSRV, &g_SceneViewport);
	CreateSceneReadOnlyDepthView();
	if (!CreateSceneMSAATargets(width, height))
	{
		hal::dout << "Direct3D_Resize(): MSAA scene target recreation failed; falling back to single-sample scene rendering." << std::endl;
		g_SceneMSAASampleCount = 1;
		g_SceneMSAAQuality = 0;
		g_SceneMSAAEnabled = false;
		ReleaseSceneMSAATargets();
	}
}


void Direct3D_Clear()
{
	
}

void Direct3D_Present()
{
	// スワップチェーンの表示
	// 貯まった描画コマンドをグラフィックに転送
	g_pSwapChain->Present(g_VSyncEnabled ? 1 : 0, 0);
}

void Direct3D_SetVSyncEnabled(bool enabled)
{
	g_VSyncEnabled = enabled;
}

bool Direct3D_IsVSyncEnabled()
{
	return g_VSyncEnabled;
}

bool Direct3D_SetSceneMSAAEnabled(bool enabled)
{
	if (!enabled)
	{
		g_SceneMSAAEnabled = false;
		return false;
	}

	if (g_pDevice == nullptr || g_SceneMSAASampleCount <= 1)
	{
		g_SceneMSAAEnabled = false;
		return false;
	}

	if (!AreSceneMSAATargetsReady())
	{
		const unsigned int width = Direct3D_GetBackBufferWidth();
		const unsigned int height = Direct3D_GetBackBufferHeight();
		if (!CreateSceneMSAATargets(static_cast<int>(width), static_cast<int>(height)))
		{
			hal::dout << "Direct3D_SetSceneMSAAEnabled(): failed to recreate MSAA scene target." << std::endl;
			g_SceneMSAAEnabled = false;
			return false;
		}
	}

	g_SceneMSAAEnabled = true;
	return true;
}

bool Direct3D_ToggleSceneMSAA()
{
	return Direct3D_SetSceneMSAAEnabled(!g_SceneMSAAEnabled);
}

bool Direct3D_IsSceneMSAAEnabled()
{
	return IsSceneMSAAActive();
}

unsigned int Direct3D_GetBackBufferWidth()
{
	return g_BackBufferDesc.Width;
}
unsigned int Direct3D_GetBackBufferHeight()
{
	return g_BackBufferDesc.Height;
}

ID3D11Device* Direct3D_GetDevice()
{
	return g_pDevice;
}

ID3D11DeviceContext* Direct3D_GetContext()
{
	return g_pDeviceContext;
}

void Direct3D_SetAlphaBlendTransparent()
{
	float blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	g_pDeviceContext->OMSetBlendState(g_pBlendStateMultiply, blend_factor, 0xffffffff);
}

void Direct3D_SetAlphaBlendAdd()
{
	float blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	g_pDeviceContext->OMSetBlendState(g_pBlendStateAdd, blend_factor, 0xffffffff);
}

void Direct3D_SetBlendStateDisable()
{
	float blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	g_pDeviceContext->OMSetBlendState(nullptr, blend_factor, 0xffffffff);
}

void Direct3D_SetCullMode(Direct3DCullMode cull_mode)
{
	switch (cull_mode)
	{
	case Direct3DCullMode::None:
		g_pDeviceContext->RSSetState(g_pRasterizerStateCullNone);
		break;

	case Direct3DCullMode::Front:
		g_pDeviceContext->RSSetState(g_pRasterizerStateCullFront);
		break;

	case Direct3DCullMode::Back:
	default:
		g_pDeviceContext->RSSetState(g_pRasterizerStateCullBack);
		break;
	}
}

void Direct3D_SetDepthEnable(bool enable)
{
	if (enable) {
		g_pDeviceContext->OMSetDepthStencilState(g_pDepthStencilStateDepthEnable, NULL);
	}
	else {
		g_pDeviceContext->OMSetDepthStencilState(g_pDepthStencilStateDepthDisable, NULL);
	}
}

void Direct3D_SetDepthWrite(bool enable)
{
	if (enable) {
		g_pDeviceContext->OMSetDepthStencilState(
			g_pDepthStencilStateDepthEnable, NULL);
	}
	else {
		g_pDeviceContext->OMSetDepthStencilState(
			g_pDepthStencilStateDepthWriteDisable, NULL);
	}
}


DirectX::XMMATRIX Direct3D_MatrixViewport() 
{
	float half_width = Direct3D_GetBackBufferWidth() * 0.5f;
	float half_height = Direct3D_GetBackBufferHeight() * 0.5f;
	float min_depth = g_Viewport.MinDepth;
	float max_depth = g_Viewport.MaxDepth;


	
	return DirectX::XMMATRIX(
		half_width, 0.0f, 0.0f, 0.0f,
		0.0f, -half_height, 0.0f, 0.0f,
		0.0f, 0.0f, (max_depth - min_depth), 0.0f, // [3, 2] 元素
		half_width, half_height, min_depth, 1.0f  // [4, 3] 元素
	);

}

DirectX::XMFLOAT3 Direct3D_ScreenToWorld(int x, int y, float depth, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
	XMMATRIX x_view{ XMLoadFloat4x4(&view) };
	XMMATRIX x_proj{ XMLoadFloat4x4(&projection)};
	XMVECTOR x_point{
		static_cast<float>(x),
		static_cast<float>(y),
		depth,
		1.0f
	};

	XMMATRIX inv{ XMMatrixInverse(nullptr, x_view * x_proj * Direct3D_MatrixViewport()) };
	
	x_point = XMVector3TransformCoord(x_point, inv);

	XMFLOAT3 ret;

	XMStoreFloat3(&ret, x_point);

	return ret;
}

DirectX::XMFLOAT2 Direct3D_WorldToScreen(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
	XMMATRIX x_view{ XMLoadFloat4x4(&view) };
	XMMATRIX x_proj{ XMLoadFloat4x4(&projection) };
	XMVECTOR x_point{ XMLoadFloat3 (&position)};

	x_point = XMVector3TransformCoord (x_point , x_view * x_proj * Direct3D_MatrixViewport());

	XMFLOAT2 ret;

	XMStoreFloat2(&ret, x_point);

	return ret;
}

void Direct3D_ClearBackbuffer()
{
	float clear_color[4] = { 0.5f, 0.8f, 1.0f, 1.0f };
	g_pDeviceContext->ClearRenderTargetView(g_pRenderTargetView, clear_color);
	g_pDeviceContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	
}

void Direct3D_SetBackbuffer()
{
	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	g_pDeviceContext->PSSetShaderResources(0, 1, nullSRV);

	g_pDeviceContext->RSSetViewports(1, &g_Viewport); // ビューポートの設定

	// レンダーターゲットビューとデプスステンシルビューの設定 
	g_pDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);
}

void Direct3D_UnbindRenderTargets()
{
}

//void Direct3D_ClearOffscreen()
//{
//	float clear_color[4] = { 0.0f, 0.8f, 0.8f, 1.0f };
//	g_pDeviceContext->ClearRenderTargetView(g_pOffscreenRenderTargetView, clear_color);
//	g_pDeviceContext->ClearDepthStencilView(g_pOffscreenDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
//}
//
//void Direct3D_SetOffscreen()
//{
//	g_pDeviceContext->RSSetViewports(1, &g_OffscreenViewport); // ビューポートの設定
//
//	// レンダーターゲットビューとデプスステンシルビューの設定 
//	g_pDeviceContext->OMSetRenderTargets(1, &g_pOffscreenRenderTargetView, g_pOffscreenDepthStencilView);
//}

//void Direct3D_SetOffscreenTexture(int slot)
//{
//	// テクスチャ設定
//	g_pDeviceContext->PSSetShaderResources(slot, 1, &g_pOffscreenShaderResourceView);
//}


bool Backend::DX11::configureBackBuffer()
{
	HRESULT hr;

	ID3D11Texture2D* back_buffer_pointer = nullptr;

	// バックバッファの取得
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer_pointer);

	if (FAILED(hr)) {
		hal::dout << "バックバッファの取得に失敗しました" << std::endl;
		return false;
	}

	// バックバッファのレンダーターゲットビューの生成
	hr = g_pDevice->CreateRenderTargetView(back_buffer_pointer, nullptr, &g_pRenderTargetView);

	if (FAILED(hr)) {
		back_buffer_pointer->Release();
		hal::dout << "バックバッファのレンダーターゲットビューの生成に失敗しました" << std::endl;
		return false;
	}

	// バックバッファの状態（情報）を取得
	back_buffer_pointer->GetDesc(&g_BackBufferDesc);

	back_buffer_pointer->Release(); // バックバッファのポインタは不要なので解放

	// デプスステンシルバッファの生成
	D3D11_TEXTURE2D_DESC depth_stencil_desc{};
	depth_stencil_desc.Width = g_BackBufferDesc.Width;
	depth_stencil_desc.Height = g_BackBufferDesc.Height;
	depth_stencil_desc.MipLevels = 1;
	depth_stencil_desc.ArraySize = 1;
	depth_stencil_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_stencil_desc.SampleDesc.Count = 1;
	depth_stencil_desc.SampleDesc.Quality = 0;
	depth_stencil_desc.Usage = D3D11_USAGE_DEFAULT;
	depth_stencil_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depth_stencil_desc.CPUAccessFlags = 0;
	depth_stencil_desc.MiscFlags = 0;
	hr = g_pDevice->CreateTexture2D(&depth_stencil_desc, nullptr, &g_pDepthStencilBuffer);

	if (FAILED(hr)) {
		hal::dout << "デプスステンシルバッファの生成に失敗しました" << std::endl;
		return false;
	}

	// デプスステンシルビューの生成
	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = depth_stencil_desc.Format;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	depth_stencil_view_desc.Flags = 0;
	hr = g_pDevice->CreateDepthStencilView(g_pDepthStencilBuffer, &depth_stencil_view_desc, &g_pDepthStencilView);

	if (FAILED(hr)) {
		hal::dout << "デプスステンシルビューの生成に失敗しました" << std::endl;
		return false;
	}

	// ビューポートの設定 
	g_Viewport.TopLeftX = 0.0f;
	g_Viewport.TopLeftY = 0.0f;
	g_Viewport.Width = static_cast<FLOAT>(g_BackBufferDesc.Width);
	g_Viewport.Height = static_cast<FLOAT>(g_BackBufferDesc.Height);
	g_Viewport.MinDepth = 0.0f;
	g_Viewport.MaxDepth = 1.0f;
	
	//g_pDeviceContext->RSSetViewports(1, &g_Viewport); // ビューポートの設定

	return true;
}

void Backend::DX11::releaseBackBuffer()
{
	// Same as below
	SAFE_RELEASE(g_pRenderTargetView);
	SAFE_RELEASE(g_pDepthStencilBuffer);
	SAFE_RELEASE(g_pDepthStencilView);
}

void Direct3D_SetSceneRenderTarget()
{
	UnbindSceneShaderResources();

	g_pDeviceContext->RSSetViewports(1, &g_SceneViewport); 
	ID3D11RenderTargetView* scene_rtv = ActiveSceneRTV();
	g_pDeviceContext->OMSetRenderTargets(1, &scene_rtv, ActiveSceneDSV());
}

void Direct3D_SetSceneRenderTargetReadOnlyDepth()
{
	UnbindSceneShaderResources();

	g_pDeviceContext->RSSetViewports(1, &g_SceneViewport);
	ID3D11RenderTargetView* scene_rtv = ActiveSceneRTV();
	g_pDeviceContext->OMSetRenderTargets(
		1,
		&scene_rtv,
		IsSceneMSAAActive()
			? g_pSceneMSAADSV
			: (g_pSceneReadOnlyDSV != nullptr ? g_pSceneReadOnlyDSV : g_pSceneDSV));
}

void Direct3D_SetSceneDepthOnlyRenderTarget()
{
	UnbindSceneShaderResources();

	g_pDeviceContext->RSSetViewports(1, &g_SceneViewport);
	g_pDeviceContext->OMSetRenderTargets(0, nullptr, g_pSceneDSV);
}

void Direct3D_SetSceneColorOnlyRenderTarget()
{
	UnbindSceneShaderResources();

	g_pDeviceContext->RSSetViewports(1, &g_SceneViewport);
	ID3D11RenderTargetView* scene_rtv = ActiveSceneRTV();
	g_pDeviceContext->OMSetRenderTargets(1, &scene_rtv, nullptr);
}

void Direct3D_SetMiniMapRenderTarget()
{
	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	g_pDeviceContext->PSSetShaderResources(1, 1, nullSRV);

	g_pDeviceContext->RSSetViewports(1, &g_MiniMapViewport); 
	g_pDeviceContext->OMSetRenderTargets(1, &g_pMiniMapRTV, g_pMiniMapDSV);
}

void Direct3D_SetPlayerRenderTarget()
{
	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	g_pDeviceContext->PSSetShaderResources(2, 1, nullSRV);

	g_pDeviceContext->RSSetViewports(1, &g_PlayerViewport); 
	g_pDeviceContext->OMSetRenderTargets(1, &g_pPlayerRTV, g_pPlayerDSV);
}

void Direct3D_ClearScene()
{
	float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; 
	g_pDeviceContext->ClearRenderTargetView(ActiveSceneRTV(), clear_color);
	g_pDeviceContext->ClearDepthStencilView(ActiveSceneDSV(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	if (IsSceneMSAAActive())
	{
		g_pDeviceContext->ClearDepthStencilView(g_pSceneDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
	}
}

void Direct3D_ClearSceneColor()
{
	float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	g_pDeviceContext->ClearRenderTargetView(ActiveSceneRTV(), clear_color);
}

void Direct3D_ClearSceneDepth()
{
	g_pDeviceContext->ClearDepthStencilView(ActiveSceneDSV(), D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void Direct3D_ClearSceneSampleDepth()
{
	g_pDeviceContext->ClearDepthStencilView(g_pSceneDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void Direct3D_ClearMiniMap()
{
	float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	g_pDeviceContext->ClearRenderTargetView(g_pMiniMapRTV, clear_color);
	g_pDeviceContext->ClearDepthStencilView(g_pMiniMapDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void Direct3D_ClearPlayer()
{
	float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	g_pDeviceContext->ClearRenderTargetView(g_pPlayerRTV, clear_color);
	g_pDeviceContext->ClearDepthStencilView(g_pPlayerDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void Direct3D_ResolveSceneMSAA()
{
	if (!IsSceneMSAAActive())
	{
		return;
	}

	UnbindSceneShaderResources();
	g_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	g_pDeviceContext->ResolveSubresource(g_pSceneTex, 0, g_pSceneMSAATex, 0, kSceneColorFormat);
}

void Direct3D_SetCustomRenderTarget(ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDSV)
{
	if (pRTV == nullptr) pRTV = g_pRenderTargetView;

	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };

	g_pDeviceContext->PSSetShaderResources(0, 1, nullSRV);

	g_pDeviceContext->RSSetViewports(1, &g_Viewport);

	g_pDeviceContext->OMSetRenderTargets(1, &pRTV, pDSV);
}

//getter
ID3D11ShaderResourceView* Direct3D_GetSceneSRV() { return g_pSceneSRV; }
ID3D11ShaderResourceView* Direct3D_GetSceneDepthSRV() { return g_pSceneDepthSRV; }
ID3D11DepthStencilView* Direct3D_GetSceneDSV(){return g_pSceneDSV;}

ID3D11ShaderResourceView* Direct3D_GetMiniMapSRV() { return g_pMiniMapSRV; }
ID3D11DepthStencilView* Direct3D_GetMiniMapDSV() { return g_pMiniMapDSV; }

ID3D11ShaderResourceView* Direct3D_GetPlayerSRV() { return g_pPlayerSRV; }
ID3D11DepthStencilView* Direct3D_GetPlayerDSV() { return g_pPlayerDSV; }




//bool configureOffscreenBuffer()
//{
//	HRESULT hr;
//
//	g_OffscreenDesc.Width = 512;
//	g_OffscreenDesc.Height = 512;
//	g_OffscreenDesc.MipLevels = 1;							// 必要なら後で自動生成する	
//	g_OffscreenDesc.ArraySize = 1;
//	g_OffscreenDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	// sRGBを使うならDXGI_FORMAT_R8G8B8A8_UNORM_SRGB
//	g_OffscreenDesc.SampleDesc.Count = 1;					// MSAA するなら >1
//	g_OffscreenDesc.SampleDesc.Quality = 0;
//	g_OffscreenDesc.Usage = D3D11_USAGE_DEFAULT;
//	g_OffscreenDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
//	g_OffscreenDesc.CPUAccessFlags = 0;
//	g_OffscreenDesc.MiscFlags = 0;
//	g_pDevice->CreateTexture2D(&g_OffscreenDesc, nullptr, &g_pOffscreenBuffer);
//	g_pDevice->CreateRenderTargetView(g_pOffscreenBuffer, nullptr, &g_pOffscreenRenderTargetView);
//	g_pDevice->CreateShaderResourceView(g_pOffscreenBuffer, nullptr, &g_pOffscreenShaderResourceView);
//
//
//	// デプスステンシルバッファの生成
//	D3D11_TEXTURE2D_DESC depth_stencil_desc{};
//	depth_stencil_desc.Width = g_OffscreenDesc.Width;
//	depth_stencil_desc.Height = g_OffscreenDesc.Height;
//	depth_stencil_desc.MipLevels = 1;
//	depth_stencil_desc.ArraySize = 1;
//	depth_stencil_desc.Format = DXGI_FORMAT_D32_FLOAT;	// DXGI_FORMAT_D24_UNORM_S8_UINT;
//	depth_stencil_desc.SampleDesc.Count = 1;
//	depth_stencil_desc.SampleDesc.Quality = 0;
//	depth_stencil_desc.Usage = D3D11_USAGE_DEFAULT;
//	depth_stencil_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
//	depth_stencil_desc.CPUAccessFlags = 0;
//	depth_stencil_desc.MiscFlags = 0;
//	hr = g_pDevice->CreateTexture2D(&depth_stencil_desc, nullptr, &g_pOffscreenDepthStencilBuffer);
//
//	if (FAILED(hr)) {
//		hal::dout << "デプスステンシルバッファの生成に失敗しました" << std::endl;
//		return false;
//	}
//
//	// デプスステンシルビューの生成
//	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
//	depth_stencil_view_desc.Format = depth_stencil_desc.Format;
//	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
//	depth_stencil_view_desc.Texture2D.MipSlice = 0;
//	depth_stencil_view_desc.Flags = 0;
//	g_pDevice->CreateDepthStencilView(g_pOffscreenDepthStencilBuffer, &depth_stencil_view_desc, &g_pOffscreenDepthStencilView);
//
//	if (FAILED(hr)) {
//		hal::dout << "デプスステンシルビューの生成に失敗しました" << std::endl;
//		return false;
//	}
//
//	// ビューポートの設定 
//	g_OffscreenViewport.TopLeftX = 0.0f;
//	g_OffscreenViewport.TopLeftY = 0.0f;
//	g_OffscreenViewport.Width = static_cast<FLOAT>(g_OffscreenDesc.Width);
//	g_OffscreenViewport.Height = static_cast<FLOAT>(g_OffscreenDesc.Height);
//	g_OffscreenViewport.MinDepth = 0.0f;
//	g_OffscreenViewport.MaxDepth = 1.0f;
//	
//	//g_pDeviceContext->RSSetViewports(1, &g_OffscreenViewport); // ビューポートの設定
//
//	return true;
//}
//
//void releaseOffscreenBuffer()
//{
//	SAFE_RELEASE(g_pOffscreenShaderResourceView);
//	SAFE_RELEASE(g_pOffscreenRenderTargetView);
//	SAFE_RELEASE(g_pOffscreenBuffer);
//	SAFE_RELEASE(g_pOffscreenDepthStencilView);
//	SAFE_RELEASE(g_pOffscreenDepthStencilBuffer);
//}



