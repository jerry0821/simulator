#include "shader_glitch.h"

#include <algorithm>
#include <d3d11.h>
#include <fstream>
#include <vector>

#include "debug_ostream.h"
#include "direct3d.h"
#include "sampler.h"
#include "shader_post.h"

namespace
{
struct GlitchConstantBuffer
{
	float time = 0.0f;
	float amount = 0.0f;
	DirectX::XMFLOAT2 padding0{};
	DirectX::XMFLOAT3 camera_position{ 0.0f, 0.0f, 0.0f };
	float underwater_enabled = 0.0f;
	DirectX::XMFLOAT4 water_surface_rect{ 0.0f, 0.0f, 1.0f, 1.0f };
	float water_height = 0.0f;
	float underwater_depth_range = 0.85f;
	DirectX::XMFLOAT2 padding1{};
	DirectX::XMFLOAT4 underwater_tint{ 0.22f, 0.44f, 0.56f, 1.0f };
	DirectX::XMFLOAT4 post_fx_primary{ 0.038f, 1.15f, 1.0f, 0.18f };
	DirectX::XMFLOAT4 post_fx_secondary{ 0.0025f, 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 post_fx_fog{ 0.20f, 0.24f, 0.0f, 0.82f };
	DirectX::XMFLOAT4 post_fx_scatter{ 0.28f, 0.38f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 post_fx_tone{ 0.0f, 2.0f, 2.2f, 1.0f };
	DirectX::XMFLOAT4X4 inverse_view_projection{};
};

ID3D11PixelShader* g_glitch_pixel_shader = nullptr;
ID3D11Buffer* g_glitch_constant_buffer = nullptr;
}

bool ShaderGlitch_Initialize()
{
	std::ifstream input("resource/shader/shader_pixel_glitch.cso", std::ios::binary);
	if (!input)
	{
		hal::dout << "ShaderGlitch_Initialize(): failed to open shader_pixel_glitch.cso" << std::endl;
		return false;
	}

	input.seekg(0, std::ios::end);
	const std::streamsize size = input.tellg();
	input.seekg(0, std::ios::beg);

	std::vector<char> shader_data(static_cast<size_t>(size));
	input.read(shader_data.data(), size);

	if (FAILED(Direct3D_GetDevice()->CreatePixelShader(
		shader_data.data(),
		shader_data.size(),
		nullptr,
		&g_glitch_pixel_shader)))
	{
		return false;
	}

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.ByteWidth = sizeof(GlitchConstantBuffer);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	if (FAILED(Direct3D_GetDevice()->CreateBuffer(
		&buffer_desc,
		nullptr,
		&g_glitch_constant_buffer)))
	{
		return false;
	}

	return true;
}

void ShaderGlitch_Finalize()
{
	SAFE_RELEASE(g_glitch_constant_buffer);
	SAFE_RELEASE(g_glitch_pixel_shader);
}

void ShaderGlitch_Draw(ID3D11ShaderResourceView* scene_srv,
					   ID3D11ShaderResourceView* bloom_srv,
					   ID3D11ShaderResourceView* scene_depth_srv,
					   ID3D11ShaderResourceView* terrain_height_srv,
					   float time,
					   float amount,
					   const DirectX::XMFLOAT3& camera_position,
					   const DirectX::XMFLOAT4X4& inverse_view_projection,
					   const PostProcessSettings& post_process_settings,
					   const WaterSurfaceDesc* water_surface_desc)
{
	GlitchConstantBuffer constants{};
	constants.time = time;
	constants.amount = amount;
	constants.camera_position = camera_position;
	constants.inverse_view_projection = inverse_view_projection;
	constants.post_fx_primary = DirectX::XMFLOAT4(
		post_process_settings.film_grain,
		post_process_settings.film_grain_speed,
		post_process_settings.film_grain_debug_boost,
		post_process_settings.vignette_intensity);
	constants.post_fx_secondary = DirectX::XMFLOAT4(
		post_process_settings.chromatic_aberration,
		post_process_settings.bloom_intensity,
		post_process_settings.vignette_softness,
		post_process_settings.atmospheric_dust);
	constants.post_fx_fog = DirectX::XMFLOAT4(
		post_process_settings.fog_intensity,
		post_process_settings.fog_distance_fade,
		post_process_settings.fog_height_bias,
		post_process_settings.fog_env_mix);
	constants.post_fx_scatter = DirectX::XMFLOAT4(
		post_process_settings.fog_scatter_strength,
		post_process_settings.fog_scatter_focus,
		0.0f,
		0.0f);
	constants.post_fx_tone = DirectX::XMFLOAT4(
		post_process_settings.exposure_ev,
		post_process_settings.tone_map_mode,
		post_process_settings.output_gamma,
		post_process_settings.output_gain);

	if (water_surface_desc != nullptr && water_surface_desc->enabled)
	{
		constants.underwater_enabled = 1.0f;
		constants.water_surface_rect = DirectX::XMFLOAT4(
			water_surface_desc->center_x,
			water_surface_desc->center_z,
			(std::max)(water_surface_desc->size_x, 1.0f),
			(std::max)(water_surface_desc->size_z, 1.0f));
		constants.water_height = water_surface_desc->height;
	}

	Direct3D_GetContext()->UpdateSubresource(
		g_glitch_constant_buffer,
		0,
		nullptr,
		&constants,
		0,
		0);

	Direct3D_SetDepthEnable(false);
	Direct3D_SetBlendStateDisable();

	ShaderPost_BindVS();
	Direct3D_GetContext()->PSSetShader(g_glitch_pixel_shader, nullptr, 0);
	Direct3D_GetContext()->PSSetConstantBuffers(0, 1, &g_glitch_constant_buffer);
	ID3D11ShaderResourceView* srvs[4] = {
		scene_srv,
		bloom_srv,
		scene_depth_srv,
		terrain_height_srv
	};
	Direct3D_GetContext()->PSSetShaderResources(0, 4, srvs);
	Backend::DX11::Sampler::SetLinearFilter();

	ShaderPost_Draw();

	ID3D11ShaderResourceView* null_srvs[4] = { nullptr, nullptr, nullptr, nullptr };
	Direct3D_GetContext()->PSSetShaderResources(0, 4, null_srvs);
	Direct3D_GetContext()->PSSetShader(nullptr, nullptr, 0);
	Direct3D_SetDepthEnable(true);
}
