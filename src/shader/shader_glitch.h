#ifndef SHADER_GLITCH_H
#define SHADER_GLITCH_H

#include <d3d11.h>
#include <DirectXMath.h>

#include "debug_menu.h"
#include "render_water_surface.h"

bool ShaderGlitch_Initialize();
void ShaderGlitch_Finalize();
void ShaderGlitch_Draw(ID3D11ShaderResourceView* scene_srv,
					   ID3D11ShaderResourceView* bloom_srv,
					   ID3D11ShaderResourceView* scene_depth_srv,
					   ID3D11ShaderResourceView* terrain_height_srv,
					   float time,
					   float amount,
					   const DirectX::XMFLOAT3& camera_position,
					   const DirectX::XMFLOAT4X4& inverse_view_projection,
					   const PostProcessSettings& post_process_settings,
					   const WaterSurfaceDesc* water_surface_desc);

#endif // SHADER_GLITCH_H
