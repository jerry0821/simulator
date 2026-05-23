#include "runtime_system.h"

#include "billboard.h"
#include "capsule.h"
#include "collision.h"
#include "cube.h"
#include "cylinder.h"
#include "direct3d.h"
#include "game_window.h"
#include "grass_patch.h"
#include "grid.h"
#include "key_logger.h"
#include "light.h"
#include "meshfield.h"
#include "mouse.h"
#include "pad_logger.h"
#include "sampler.h"
#include "shader.h"
#include "shader3d.h"
#include "shader3d_instanced.h"
#include "shader3d_unlit.h"
#include "shader_glitch.h"
#include "shader_grass_instanced.h"
#include "shader_post.h"
#include "shader_shadow.h"
#include "shader_toon.h"
#include "sphere.h"
#include "sprite.h"
#include "sprite3d.h"
#include "sprite_anim.h"
#include "system_timer.h"
#include "texture.h"

namespace RuntimeSystem
{
bool Initialize(HWND window_handle)
{
	SystemTimer_Initialize();
	KeyLogger_Initialize();
	PadLogger_Initialize();
	Mouse_Initialize(window_handle);

	if (!Direct3D_Initialize(window_handle))
	{
		return false;
	}

	Shader_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Shader3D_Initialize();
	Shader3DInstanced_Initialize();
	ShaderGrassInstanced_Initialize();
	Shader3D_Unlit_Initialize();
	ShaderToon_Initialize();
	ShaderPost_Initialize();
	ShaderShadow_Initialize();
	ShaderGlitch_Initialize();
	TextureManager::Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Backend::DX11::Sampler::Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Sprite_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Sprite3D_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	SpriteAnim_Initialize();
	Billboard_Initialize();
	Collision::Debug::Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Grid_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Cube_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	GrassPatch_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Sphere_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Cylinder_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Capsule_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	MeshFieldRenderer::Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	Light_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
	return true;
}

void Finalize()
{
	Light_Finalize();
	MeshFieldRenderer::Finalize();
	Grid_Finalize();
	GrassPatch_Finalize();
	Cylinder_Finalize();
	Capsule_Finalize();
	Sphere_Finalize();
	Cube_Finalize();
	Collision::Debug::Finalize();
	SpriteAnim_Finalize();
	Sprite3D_Finalize();
	Sprite_Finalize();
	Billboard_Finalize();
	Backend::DX11::Sampler::Finalize();
	TextureManager::Finalize();
	ShaderGlitch_Finalize();
	ShaderShadow_Finalize();
	ShaderPost_Finalize();
	ShaderToon_Finalize();
	Shader3D_Unlit_Finalize();
	ShaderGrassInstanced_Finalize();
	Shader3DInstanced_Finalize();
	Shader3D_Finalize();
	Shader_Finalize();
	Direct3D_Finalize();
	Mouse_Finalize();
}
}
