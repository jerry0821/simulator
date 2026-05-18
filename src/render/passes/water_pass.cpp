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
	ShaderWater_SetViewMatrix(view);
	ShaderWater_SetProjMatrix(proj);
	ID3D11ShaderResourceView* water_surface_height_srv =
		frame_context.resources.water_surface_height.isValid()
			? frame_context.resources.water_surface_height.shaderResourceView()
			: nullptr;
	const XMMATRIX water_world =
		XMMatrixScaling(water_desc.size_x, water_desc.size_z, 1.0f) *
		XMMatrixRotationX(XM_PIDIV2) *
		XMMatrixTranslation(water_desc.center_x, water_desc.height, water_desc.center_z);
	XMFLOAT4 base_color = water_desc.base_color;
	base_color.x *= 0.92f;
	base_color.y *= 0.95f;
	base_color.z *= 1.02f;
	base_color.w = std::clamp(base_color.w * 0.60f, 0.10f, 0.26f);

	Sprite3D_DrawWaterSRV(
		water_surface_height_srv,
		water_world,
		base_color);
}
