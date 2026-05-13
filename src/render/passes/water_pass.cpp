#include "water_pass.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <DirectXMath.h>

#include "cube.h"
#include "direct3d.h"
#include "render_backend_dx11.h"
#include "render_frame_context.h"
#include "render_scene.h"
#include "render_resource_usage.h"
#include "render_water_surface.h"
#include "debug_menu.h"
#include "shader_water.h"
#include "shader3d.h"
#include "shader3d_unlit.h"
#include "sprite3d.h"

using namespace DirectX;

namespace
{
constexpr bool kEnableWaterSurfaceRender = true;
}

WaterPass::WaterPass(RenderBackendDX11& backend)
	: backend_(backend)
{
}

void WaterPass::drawSurfaceLayer(ID3D11ShaderResourceView* texture_srv,
								 const XMMATRIX& world,
								 const XMFLOAT4& color) const
{
	if (texture_srv == nullptr)
	{
		return;
	}

	Cube_DrawMaterialSRV(texture_srv,
						 world,
						 color,
						 RenderState{ DepthMode::ReadOnly, BlendMode::Alpha, CullMode::None },
						 MaterialType::Unlit);
}

void WaterPass::drawSurfacePlaneLayer(ID3D11ShaderResourceView* texture_srv,
									  const XMMATRIX& world,
									  const XMFLOAT4& color) const
{
	if (texture_srv == nullptr)
	{
		return;
	}

	Sprite3D_DrawTransparentSRV(texture_srv, world, color);
}

void WaterPass::drawSurfacePlaneHighlightLayer(ID3D11ShaderResourceView* texture_srv,
											   const XMMATRIX& world,
											   const XMFLOAT4& color) const
{
	if (texture_srv == nullptr)
	{
		return;
	}

	Sprite3D_DrawAdditiveSRV(texture_srv, world, color);
}

std::string_view WaterPass::name() const
{
	return "WaterPass";
}

std::span<const RenderResourceUsage> WaterPass::resources() const
{
	static constexpr std::array usages = {
		RenderResourceUsage{ RenderResourceId::SceneDepth, RenderResourceAccess::Read },
		RenderResourceUsage{ RenderResourceId::SceneColor, RenderResourceAccess::ReadWrite }
	};
	return usages;
}

void WaterPass::execute(const RenderFrameContext& frame_context)
{
	if (!kEnableWaterSurfaceRender)
	{
		return;
	}

	// Water is rendered after opaque geometry.
	backend_.bindScenePass();

	if (frame_context.render_scene == nullptr)
	{
		return;
	}

	WaterSurfaceDesc water_desc{};
	if (frame_context.resources.terrain_water.HasWaterSurfaceDesc())
	{
		water_desc = frame_context.resources.terrain_water.water_surface_desc;
	}
	else if (frame_context.render_scene == nullptr ||
			 !frame_context.render_scene->getWaterSurfaceDesc(water_desc) ||
			 !water_desc.enabled)
	{
		return;
	}

	const XMMATRIX view = XMLoadFloat4x4(&frame_context.globals.view_matrix);
	const XMMATRIX proj = XMLoadFloat4x4(&frame_context.globals.projection_matrix);
	Shader3D_SetViewMatrix(view);
	Shader3D_SetProjMatrix(proj);
	Shader3D_Unlit_SetViewMatrix(view);
	Shader3D_Unlit_SetProjMatrix(proj);
	ShaderWater_SetViewMatrix(view);
	ShaderWater_SetProjMatrix(proj);
	const TerrainWaterFrameState& terrain_water = frame_context.resources.terrain_water;
	ID3D11ShaderResourceView* water_mask_srv =
		terrain_water.water_mask.isValid()
			? terrain_water.water_mask.shaderResourceView()
			: nullptr;
	ID3D11ShaderResourceView* water_surface_height_srv =
		DebugMenu_IsWaterSurfaceDeformationEnabled() && terrain_water.water_surface_height_texture.isValid()
			? terrain_water.water_surface_height_texture.shaderResourceView()
			: nullptr;
	ID3D11ShaderResourceView* water_interaction_srv =
		terrain_water.water_interaction_data.isValid()
			? terrain_water.water_interaction_data.shaderResourceView()
			: nullptr;
	ID3D11ShaderResourceView* surface_water_srv =
		terrain_water.surface_water.isValid()
			? terrain_water.surface_water.shaderResourceView()
			: nullptr;
	ID3D11ShaderResourceView* flow_field_srv =
		terrain_water.surface_water_flow_preview.isValid()
			? terrain_water.surface_water_flow_preview.shaderResourceView()
			: nullptr;
	ID3D11ShaderResourceView* surface_flow_srv =
		terrain_water.surface_water_flow.isValid()
			? terrain_water.surface_water_flow.shaderResourceView()
			: nullptr;
	ID3D11ShaderResourceView* wind_field_srv =
		frame_context.resources.wind_field.isValid()
			? frame_context.resources.wind_field.shaderResourceView()
			: nullptr;
	ID3D11ShaderResourceView* scene_depth_srv =
		frame_context.resources.scene_depth.isValid()
			? frame_context.resources.scene_depth.shaderResourceView()
			: nullptr;
	const XMMATRIX water_world =
		XMMatrixScaling(water_desc.size_x, water_desc.size_z, 1.0f) *
		XMMatrixRotationX(XM_PIDIV2) *
		XMMatrixTranslation(water_desc.center_x, water_desc.height, water_desc.center_z);
	const XMMATRIX flow_world =
		XMMatrixScaling(water_desc.size_x, water_desc.size_z, 1.0f) *
		XMMatrixRotationX(XM_PIDIV2) *
		XMMatrixTranslation(water_desc.center_x, water_desc.height + 0.02f, water_desc.center_z);
	const XMMATRIX highlight_world =
		XMMatrixScaling(water_desc.size_x, water_desc.size_z, 1.0f) *
		XMMatrixRotationX(XM_PIDIV2) *
		XMMatrixTranslation(water_desc.center_x, water_desc.height + 0.004f, water_desc.center_z);
	XMFLOAT4 base_color = water_desc.base_color;
	base_color.x *= 0.92f;
	base_color.y *= 0.95f;
	base_color.z *= 1.02f;
	base_color.w = std::clamp(base_color.w * 0.60f, 0.10f, 0.26f);
	const float ripple_strength = std::clamp(water_desc.ripple_strength, 0.0f, 3.0f);
	const float wind_influence = std::clamp(water_desc.wind_influence, 0.0f, 3.0f);
	XMFLOAT4 flow_color = {
		0.86f + 0.08f * wind_influence,
		0.92f + 0.06f * wind_influence,
		1.00f,
		std::clamp(0.26f + 0.12f * ripple_strength + 0.08f * wind_influence, 0.22f, 0.52f)
	};
	XMFLOAT4 highlight_color = {
		water_desc.base_color.x * 0.95f,
		water_desc.base_color.y * 0.98f,
		water_desc.base_color.z * 1.02f,
		water_desc.base_color.w * 0.42f
	};

	drawSurfacePlaneLayer(water_mask_srv, water_world, base_color);
	Direct3D_SetSceneColorOnlyRenderTarget();
	Sprite3D_DrawWaterSRV(
		water_mask_srv,
		water_surface_height_srv,
		surface_water_srv,
		wind_field_srv,
		water_interaction_srv,
		scene_depth_srv,
		highlight_world,
		flow_color,
		frame_context.globals.camera_position,
		static_cast<float>(frame_context.globals.time_seconds),
		water_desc.center_x,
		water_desc.center_z,
		water_desc.size_x,
		water_desc.size_z,
		3.2f + water_desc.edge_emphasis * 1.4f,
		0.62f + ripple_strength * 0.36f);

	(void)water_surface_height_srv;
	(void)surface_water_srv;
	(void)highlight_world;
	(void)highlight_color;
	(void)surface_flow_srv;
	(void)flow_field_srv;
	(void)water_interaction_srv;
	(void)wind_field_srv;
	(void)scene_depth_srv;
}
