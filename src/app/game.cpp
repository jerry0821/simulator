// ----------------------------------------------------
// 
// 繧ｲ繝ｼ繝譛ｬ菴・[game.cpp]
// 
// ====================================================
// Created by: Jerry
// Date: 2025-09-09
// ----------------------------------------------------
#include "game.h"
#include "cube.h"
#include "shader3d.h"
#include "shader3d_instanced.h"
#include "shader3d_unlit.h"
#include "grid.h"
#include "camera.h"
#include "meshfield.h"
#include "light.h"
#include "model.h"
#include "compute_noise_texture.h"
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
using namespace DirectX;
#include "key_logger.h"
#include "mouse.h"
#include "sampler.h"
#include "sky.h"
#include "direct3d.h"

#include "pad_logger.h"


#include "billboard.h"
#include "texture.h"
#include "sprite_anim.h"

#include "sprite.h"
#include "resource_manager.h"
#include "imgui/imgui.h"
#include "collision.h"


#include "render_frame_context.h"
#include "render_water_surface.h"
#include "render_state.h"
#include "sprite3d.h"


static ID3D11RasterizerState* g_pWireframeState = nullptr;
static ID3D11RasterizerState* g_pSolidState = nullptr;

namespace
{
constexpr bool kDrawSkyDebug = true;
constexpr bool kDrawWaterDebug = true;
constexpr bool kDrawTerrainDebug = true;
}

void GameController::Initialize()
{
	D3D11_RASTERIZER_DESC wfDesc = {};
	wfDesc.FillMode = D3D11_FILL_WIREFRAME;
	wfDesc.CullMode = D3D11_CULL_NONE;
	wfDesc.DepthClipEnable = TRUE;
	Direct3D_GetDevice()->CreateRasterizerState(&wfDesc, &g_pWireframeState);

	D3D11_RASTERIZER_DESC solidDesc = {};
	solidDesc.FillMode = D3D11_FILL_SOLID;
	solidDesc.CullMode = D3D11_CULL_BACK;
	solidDesc.DepthClipEnable = TRUE;
	Direct3D_GetDevice()->CreateRasterizerState(&solidDesc, &g_pSolidState);

	m_test_texture = TextureManager::Load(L"resource/texture/cube.png");

	ResourceManager::Initialize();

	SpriteAnim_Initialize();
	Camera_Initialize({ 0.0f, 3.0f, -6.0f }, { 0.0f,-0.6f,0.8f }, { 1.0f,0.0f,0.0f });
	m_map_controller.Initialize();
	m_map_controller.SetTerrainVisible(kDrawTerrainDebug);
	Sky_Initialize();

	Light_SetAmbient(XMFLOAT3(0.3f, 0.3f, 0.3f));

	const XMFLOAT4 init_dir = XMFLOAT4(-0.5f, -1.0f, -0.5f, 0.0f);
	const XMFLOAT4 init_col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	Light_SetDirectionalWorld(init_dir, init_col);
	Light_SetSpecularWorld(XMFLOAT3(0, 0, 0), 20.0f, XMFLOAT4(1, 1, 1, 1));
}

void GameController::Finalize()
{
	m_map_controller.Finalize();
	Sky_Finalize();
	ResourceManager::Finalize();
}

void GameController::Update(double elapsed_time)
{
	Camera_Update(elapsed_time);
}

void GameController::Draw(const RenderFrameContext& frame_context)
{
	XMMATRIX view = XMLoadFloat4x4(&frame_context.globals.view_matrix);
	XMMATRIX proj = XMLoadFloat4x4(&frame_context.globals.projection_matrix);

	Shader3D_Unlit_SetViewMatrix(view);
	Shader3D_Unlit_SetProjMatrix(proj);
	Shader3D_SetViewMatrix(view);
	Shader3D_SetProjMatrix(proj);
	Shader3DInstanced_SetViewMatrix(view);
	Shader3DInstanced_SetProjMatrix(proj);
	if (kDrawSkyDebug)
	{
		Sky_SetPosition(frame_context.globals.camera_position);
		Sky_Draw();
	}

	m_map_controller.Draw(frame_context);
}

void GameController::DrawDepthPrePass(const RenderFrameContext& frame_context)
{
	m_map_controller.DrawDepthPrePass(frame_context);
}

bool GameController::GetWaterSurfaceDesc(WaterSurfaceDesc& out_desc)
{
	if (!kDrawWaterDebug)
	{
		out_desc = WaterSurfaceDesc{};
		return false;
	}
	return m_map_controller.GetWaterSurfaceDesc(out_desc);
}

void GameController::DrawTransparency(const RenderFrameContext& frame_context)
{
	// Transparency path is separate from opaque.
	m_map_controller.DrawTransparency(frame_context);
}

void GameController::DrawParticles(const RenderFrameContext& frame_context)
{
	m_map_controller.DrawParticles(frame_context);
}

void GameController::DrawShadow(const RenderFrameContext& frame_context)
{
	m_map_controller.DrawShadow(&frame_context);
}
