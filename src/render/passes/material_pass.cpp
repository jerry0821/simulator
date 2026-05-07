#include "material_pass.h"

#include "direct3d.h"
#include "shader3d.h"
#include "shader3d_unlit.h"

namespace
{
Direct3DCullMode toDirect3DCullMode(CullMode cull_mode)
{
	switch (cull_mode)
	{
	case CullMode::None:
		return Direct3DCullMode::None;

	case CullMode::Front:
		return Direct3DCullMode::Front;

	case CullMode::Back:
	default:
		return Direct3DCullMode::Back;
	}
}
}

MaterialPass::MaterialPass(MaterialType material_type, RenderState render_state)
	: material_type_(material_type),
	  render_state_(render_state)
{
}

void MaterialPass::begin(const DirectX::XMMATRIX& world_matrix,
                         const DirectX::XMFLOAT4& material_color) const
{
	switch (material_type_)
	{
	case MaterialType::Lit:
		Shader3D_Begin();
		Shader3D_SetWorldMatrix(world_matrix);
		Shader3D_SetMaterialColor(material_color);
		break;

	case MaterialType::Unlit:
		Shader3D_Unlit_Begin();
		Shader3D_Unlit_SetWorldMatrix(world_matrix);
		Shader3D_Unlit_SetMaterialColor(material_color);
		break;
	}

	switch (render_state_.depth_mode)
	{
	case DepthMode::Disabled:
		Direct3D_SetDepthEnable(false);
		break;

	case DepthMode::ReadWrite:
		Direct3D_SetDepthEnable(true);
		Direct3D_SetDepthWrite(true);
		break;

	case DepthMode::ReadOnly:
		Direct3D_SetDepthEnable(true);
		Direct3D_SetDepthWrite(false);
		break;
	}

	switch (render_state_.blend_mode)
	{
	case BlendMode::Opaque:
		Direct3D_SetBlendStateDisable();
		break;

	case BlendMode::Alpha:
		Direct3D_SetAlphaBlendTransparent();
		break;

	case BlendMode::Additive:
		Direct3D_SetAlphaBlendAdd();
		break;
	}

	Direct3D_SetCullMode(toDirect3DCullMode(render_state_.cull_mode));
}

void MaterialPass::end() const
{
	Direct3D_SetBlendStateDisable();
	Direct3D_SetCullMode(Direct3DCullMode::Back);
	Direct3D_SetDepthEnable(false);
}
