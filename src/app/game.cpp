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
#include "../compute/terrain_height_field.h"
#include "sphere.h"
#include "direct3d.h"
#include <algorithm>

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

#include "debug_menu.h"

void GameController::Update(double elapsed_time)
{
	static bool tab_pressed = false;
	if (GetAsyncKeyState(VK_TAB) & 0x8000) {
		if (!tab_pressed) {
			m_third_person_mode = !m_third_person_mode;
			tab_pressed = true;
		}
	} else {
		tab_pressed = false;
	}

	float move_speed = 30.0f * (float)elapsed_time;
	float turn_speed = 2.0f * (float)elapsed_time;
	
	if (GetAsyncKeyState('A') & 0x8000) m_sphere_yaw -= turn_speed;
	if (GetAsyncKeyState('D') & 0x8000) m_sphere_yaw += turn_speed;
	
	if (GetAsyncKeyState('W') & 0x8000) {
		m_sphere_pos.x += sinf(m_sphere_yaw) * move_speed;
		m_sphere_pos.z += cosf(m_sphere_yaw) * move_speed;
	}
	if (GetAsyncKeyState('S') & 0x8000) {
		m_sphere_pos.x -= sinf(m_sphere_yaw) * move_speed;
		m_sphere_pos.z -= cosf(m_sphere_yaw) * move_speed;
	}

	float surface_height = TerrainHeightField::GetHeight(m_sphere_pos.x, m_sphere_pos.z);
	
	m_sphere_vel_y -= 40.0f * (float)elapsed_time;
	m_sphere_pos.y += m_sphere_vel_y * (float)elapsed_time;

	if (m_sphere_pos.y <= surface_height + 1.0f) { // radius=1.0f
		m_sphere_pos.y = surface_height + 1.0f;
		m_sphere_vel_y = 0.0f;
		if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
			m_sphere_vel_y = 15.0f;
		}
	}

	if (m_third_person_mode) {
		float cam_dist = 6.0f;
		float cam_height = 2.5f;
		XMFLOAT3 cam_pos = {
			m_sphere_pos.x - sinf(m_sphere_yaw) * cam_dist,
			m_sphere_pos.y + cam_height,
			m_sphere_pos.z - cosf(m_sphere_yaw) * cam_dist
		};
		
		float cam_ground = TerrainHeightField::GetHeight(cam_pos.x, cam_pos.z);
		if (cam_pos.y < cam_ground + 0.5f) {
			cam_pos.y = cam_ground + 0.5f;
		}

		XMFLOAT3 front = {
			m_sphere_pos.x - cam_pos.x,
			(m_sphere_pos.y + 1.0f) - cam_pos.y,
			m_sphere_pos.z - cam_pos.z
		};
		float len = sqrtf(front.x*front.x + front.y*front.y + front.z*front.z);
		front.x /= len; front.y /= len; front.z /= len;
		XMFLOAT3 right = { front.z, 0.0f, -front.x };
		len = sqrtf(right.x*right.x + right.z*right.z);
		right.x /= len; right.z /= len;
		
		Camera_SetTransform(cam_pos, front, right);
	} else {
		Camera_Update(elapsed_time);
	}

	Mouse_State ms;
	Mouse_GetState(&ms);
	bool altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

	// Left click to inject water
	if (ms.leftButton && !altHeld)
	{
		XMFLOAT4X4 mtxView = Camera_GetMatrix();
		XMFLOAT3 test_near = Direct3D_ScreenToWorld(ms.x, ms.y, 0.0f, mtxView, Camera_GetPerspectiveMatrix());
		XMFLOAT3 test_far = Direct3D_ScreenToWorld(ms.x, ms.y, 1.0f, mtxView, Camera_GetPerspectiveMatrix());

		XMVECTOR vtest = XMLoadFloat3(&test_far) - XMLoadFloat3(&test_near);
		vtest = XMVector3Normalize(vtest);
		XMFLOAT3 dir;
		XMStoreFloat3(&dir, vtest);

		if (dir.y != 0.0f)
		{
			float t = -test_near.y / dir.y;
			if (t > 0.0f)
			{
				float hit_x = test_near.x + dir.x * t;
				float hit_z = test_near.z + dir.z * t;
				DebugMenu_TriggerTerrainInjectionAt(hit_x, hit_z);
			}
		}
	}

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

	XMMATRIX world = XMMatrixRotationY(m_sphere_yaw) * XMMatrixTranslation(m_sphere_pos.x, m_sphere_pos.y, m_sphere_pos.z);
	Sphere_Draw(-1, world, MaterialType::Lit);
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
