#include "shader_reload.h"

#include "debug_ostream.h"
#include "direct3d.h"
#include "shader.h"
#include "shader3d.h"
#include "shader3d_instanced.h"
#include "shader_grass_instanced.h"
#include "shader3d_unlit.h"
#include "shader_billboard.h"
#include "shader_field.h"
#include "shader_glitch.h"
#include "shader_particle.h"
#include "shader_post.h"
#include "shader_shadow.h"
#include "shader_toon.h"

namespace
{
bool ReloadStandardShader()
{
	Shader_Finalize();
	return Shader_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
}

bool ReloadFieldShader()
{
	ShaderField_Finalize();
	return ShaderField_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
}
}

ShaderReloadStatus ShaderReload_ReloadAll()
{
	struct ReloadStep
	{
		const char* name = nullptr;
		void (*finalize)() = nullptr;
		bool (*initialize)() = nullptr;
	};

	const ReloadStep reload_steps[] = {
		{"Shader2D", Shader_Finalize, ReloadStandardShader},
		{"Shader3D", Shader3D_Finalize, Shader3D_Initialize},
		{"Shader3DInstanced", Shader3DInstanced_Finalize, Shader3DInstanced_Initialize},
		{"ShaderGrassInstanced", ShaderGrassInstanced_Finalize, ShaderGrassInstanced_Initialize},
		{"Shader3DUnlit", Shader3D_Unlit_Finalize, Shader3D_Unlit_Initialize},
		{"ShaderToon", ShaderToon_Finalize, ShaderToon_Initialize},
		{"ShaderParticle", ShaderParticle_Finalize, ShaderParticle_Initialize},
		{"ShaderPost", ShaderPost_Finalize, ShaderPost_Initialize},
		{"ShaderShadow", ShaderShadow_Finalize, ShaderShadow_Initialize},
		{"ShaderGlitch", ShaderGlitch_Finalize, ShaderGlitch_Initialize},
		{"ShaderField", ShaderField_Finalize, ReloadFieldShader},
		{"ShaderBillboard", ShaderBillBoard_Finalize, ShaderBillBoard_Initialize},
	};

	for (const ReloadStep& reload_step : reload_steps)
	{
		if (reload_step.finalize != nullptr)
		{
			reload_step.finalize();
		}

		if (!reload_step.initialize())
		{
			ShaderReloadStatus status{};
			status.succeeded = false;
			status.message = std::string("Reload failed: ") + reload_step.name;
			hal::dout << "ShaderReload_ReloadAll(): " << status.message << std::endl;
			return status;
		}
	}

	ShaderReloadStatus status{};
	status.succeeded = true;
	status.message = "Shaders reloaded";
	hal::dout << "ShaderReload_ReloadAll(): " << status.message << std::endl;
	return status;
}
