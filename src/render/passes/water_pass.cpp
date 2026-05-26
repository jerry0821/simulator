#include "water_pass.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <DirectXMath.h>

#include "compute_texture_dimensions.h"
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
#include "terrain_height_field.h"


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

	// Water samples scene depth while depth-testing against it.
	backend_.bindScenePassReadOnlyDepth();

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
	XMFLOAT4X4 inverse_view_projection{};
	XMStoreFloat4x4(
		&inverse_view_projection,
		XMMatrixTranspose(XMMatrixInverse(nullptr, view * proj)));
	Shader3D_SetViewMatrix(view);
	Shader3D_SetProjMatrix(proj);
	Shader3D_Unlit_SetViewMatrix(view);
	Shader3D_Unlit_SetProjMatrix(proj);
	ShaderWater_SetViewMatrix(view);
	ShaderWater_SetProjMatrix(proj);
	ShaderWater_SetInverseViewProjection(inverse_view_projection);
	const TerrainWaterFrameState& terrain_water = frame_context.resources.terrain_water;
	ID3D11ShaderResourceView* terrain_height_srv =
		frame_context.resources.terrain_height.isValid()
			? frame_context.resources.terrain_height.shaderResourceView()
			: (terrain_water.terrain_height.isValid()
				? terrain_water.terrain_height.shaderResourceView()
				: (frame_context.resources.water_surface_height.isValid()
					? frame_context.resources.water_surface_height.shaderResourceView()
					: nullptr));
	ID3D11ShaderResourceView* water_velocity_srv =
		terrain_water.water_velocity.isValid()
			? terrain_water.water_velocity.shaderResourceView()
			: nullptr;
	ID3D11ShaderResourceView* water_sediment_srv =
		terrain_water.water_sediment.isValid()
			? terrain_water.water_sediment.shaderResourceView()
			: nullptr;
	ID3D11ShaderResourceView* terrain_normal_srv =
		frame_context.resources.terrain_normal.isValid()
			? frame_context.resources.terrain_normal.shaderResourceView()
			: nullptr;
	ID3D11ShaderResourceView* scene_depth_srv =
		frame_context.resources.scene_depth.isValid()
			? frame_context.resources.scene_depth.shaderResourceView()
			: nullptr;
	const float world_width = TerrainHeightField::FieldWidth();
	const float world_depth = TerrainHeightField::FieldDepth();
	const float patch_width = world_width * ComputeTextureDimensions::kWaterPatchCoverage;
	const float patch_depth = world_depth * ComputeTextureDimensions::kWaterPatchCoverage;
	const float patch_cell_width =
		patch_width / static_cast<float>(ComputeTextureDimensions::kWaterMeshResolution);
	const float patch_cell_depth =
		patch_depth / static_cast<float>(ComputeTextureDimensions::kWaterMeshResolution);
	const float snapped_x =
		std::floor(frame_context.globals.camera_position.x / patch_cell_width) * patch_cell_width;
	const float snapped_z =
		std::floor(frame_context.globals.camera_position.z / patch_cell_depth) * patch_cell_depth;

	const XMMATRIX water_world =
		XMMatrixScaling(patch_width, patch_depth, 1.0f) *
		XMMatrixRotationX(XM_PIDIV2) *
		XMMatrixTranslation(snapped_x, water_desc.height, snapped_z);
	XMFLOAT4 base_color{ 1.0f, 1.0f, 1.0f, std::clamp(water_desc.base_color.w, 0.34f, 0.70f) };

	Sprite3D_DrawWaterSRV(
		terrain_height_srv,
		water_velocity_srv,
		water_sediment_srv,
		terrain_normal_srv,
		scene_depth_srv,
		water_world,
		base_color,
		frame_context.globals.camera_position,
		0.0f);

	(void)terrain_height_srv;
	(void)water_velocity_srv;
	(void)water_sediment_srv;
	(void)terrain_normal_srv;
	(void)water_world;
	(void)base_color;
	(void)scene_depth_srv;
}
