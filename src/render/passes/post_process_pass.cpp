#include "post_process_pass.h"

#include <array>
#include <fstream>
#include <vector>

#include <DirectXMath.h>

#include "debug_menu.h"
#include "direct3d.h"
#include "render_backend_dx11.h"
#include "render_frame_context.h"
#include "render_resource_usage.h"
#include "render_scene.h"
#include "render_water_surface.h"
#include "sampler.h"
#include "shader_glitch.h"
#include "shader_post.h"

namespace
{
struct BloomConstantBuffer
{
	DirectX::XMFLOAT2 texel_size{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 blur_direction{ 0.0f, 0.0f };
	float threshold = 0.74f;
	float soft_knee = 0.16f;
	float blur_scale = 1.65f;
	float padding0 = 0.0f;
};

bool CreateBloomTarget(
	unsigned int width,
	unsigned int height,
	ID3D11Texture2D** texture,
	ID3D11RenderTargetView** rtv,
	ID3D11ShaderResourceView** srv)
{
	if (texture == nullptr || rtv == nullptr || srv == nullptr)
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(Direct3D_GetDevice()->CreateTexture2D(&desc, nullptr, texture)))
	{
		return false;
	}

	if (FAILED(Direct3D_GetDevice()->CreateRenderTargetView(*texture, nullptr, rtv)))
	{
		return false;
	}

	if (FAILED(Direct3D_GetDevice()->CreateShaderResourceView(*texture, nullptr, srv)))
	{
		return false;
	}

	return true;
}

bool LoadPixelShader(const char* path, ID3D11PixelShader** shader)
{
	if (shader == nullptr)
	{
		return false;
	}

	std::ifstream input(path, std::ios::binary);
	if (!input)
	{
		return false;
	}

	input.seekg(0, std::ios::end);
	const std::streamsize size = input.tellg();
	input.seekg(0, std::ios::beg);

	std::vector<char> shader_data(static_cast<size_t>(size));
	input.read(shader_data.data(), size);

	return SUCCEEDED(Direct3D_GetDevice()->CreatePixelShader(
		shader_data.data(),
		shader_data.size(),
		nullptr,
		shader));
}
}

PostProcessPass::PostProcessPass(RenderBackendDX11& backend)
	: backend_(backend)
{
}

PostProcessPass::~PostProcessPass()
{
	ReleaseBloomResources();
	SAFE_RELEASE(bloom_constant_buffer_);
	SAFE_RELEASE(bloom_prefilter_ps_);
	SAFE_RELEASE(bloom_blur_ps_);
}

std::string_view PostProcessPass::name() const
{
	return "PostProcessPass";
}

std::span<const RenderResourceUsage> PostProcessPass::resources() const
{
	static constexpr std::array usages = {
		RenderResourceUsage{ RenderResourceId::SceneColor, RenderResourceAccess::Read },
		RenderResourceUsage{ RenderResourceId::SceneDepth, RenderResourceAccess::Read },
		RenderResourceUsage{ RenderResourceId::BackBuffer, RenderResourceAccess::Write }
	};
	return usages;
}

bool PostProcessPass::EnsureBloomShaders()
{
	if (bloom_prefilter_ps_ != nullptr &&
		bloom_blur_ps_ != nullptr &&
		bloom_constant_buffer_ != nullptr)
	{
		return true;
	}

	if (bloom_prefilter_ps_ == nullptr &&
		!LoadPixelShader("resource/shader/shader_pixel_bloom_prefilter.cso", &bloom_prefilter_ps_))
	{
		return false;
	}

	if (bloom_blur_ps_ == nullptr &&
		!LoadPixelShader("resource/shader/shader_pixel_bloom_blur.cso", &bloom_blur_ps_))
	{
		return false;
	}

	if (bloom_constant_buffer_ == nullptr)
	{
		D3D11_BUFFER_DESC buffer_desc{};
		buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		buffer_desc.ByteWidth = sizeof(BloomConstantBuffer);
		buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		if (FAILED(Direct3D_GetDevice()->CreateBuffer(&buffer_desc, nullptr, &bloom_constant_buffer_)))
		{
			return false;
		}
	}

	return true;
}

void PostProcessPass::ReleaseBloomResources()
{
	SAFE_RELEASE(bloom_srv_a_);
	SAFE_RELEASE(bloom_rtv_a_);
	SAFE_RELEASE(bloom_tex_a_);
	SAFE_RELEASE(bloom_srv_b_);
	SAFE_RELEASE(bloom_rtv_b_);
	SAFE_RELEASE(bloom_tex_b_);
	bloom_width_ = 0;
	bloom_height_ = 0;
}

bool PostProcessPass::EnsureBloomTargets(unsigned int width, unsigned int height)
{
	if (bloom_srv_a_ != nullptr &&
		bloom_srv_b_ != nullptr &&
		bloom_width_ == width &&
		bloom_height_ == height)
	{
		return true;
	}

	ReleaseBloomResources();

	if (!CreateBloomTarget(width, height, &bloom_tex_a_, &bloom_rtv_a_, &bloom_srv_a_))
	{
		ReleaseBloomResources();
		return false;
	}

	if (!CreateBloomTarget(width, height, &bloom_tex_b_, &bloom_rtv_b_, &bloom_srv_b_))
	{
		ReleaseBloomResources();
		return false;
	}

	bloom_width_ = width;
	bloom_height_ = height;
	return true;
}

ID3D11ShaderResourceView* PostProcessPass::RenderBloom(
	ID3D11ShaderResourceView* scene_srv,
	const PostProcessSettings& settings)
{
	if (scene_srv == nullptr || settings.bloom_intensity <= 0.0f)
	{
		return nullptr;
	}

	const unsigned int width = Direct3D_GetBackBufferWidth();
	const unsigned int height = Direct3D_GetBackBufferHeight();
	if (!EnsureBloomShaders() || !EnsureBloomTargets(width, height))
	{
		return nullptr;
	}

	BloomConstantBuffer constants{};
	constants.texel_size = { 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height) };
	constants.threshold = settings.bloom_threshold;
	constants.soft_knee = settings.bloom_soft_knee;
	constants.blur_scale = settings.bloom_blur_scale;

	ID3D11DeviceContext* context = Direct3D_GetContext();
	float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	ID3D11ShaderResourceView* null_srvs[3] = { nullptr, nullptr, nullptr };

	Direct3D_SetDepthEnable(false);
	Direct3D_SetBlendStateDisable();
	ShaderPost_BindVS();
	context->PSSetConstantBuffers(0, 1, &bloom_constant_buffer_);
	Backend::DX11::Sampler::SetLinearFilter();

	// Bright prefilter.
	Direct3D_SetCustomRenderTarget(bloom_rtv_a_, nullptr);
	context->ClearRenderTargetView(bloom_rtv_a_, clear_color);
	context->UpdateSubresource(bloom_constant_buffer_, 0, nullptr, &constants, 0, 0);
	context->PSSetShader(bloom_prefilter_ps_, nullptr, 0);
	context->PSSetShaderResources(0, 1, &scene_srv);
	ShaderPost_Draw();
	context->PSSetShaderResources(0, 1, null_srvs);

	// Horizontal blur.
	Direct3D_SetCustomRenderTarget(bloom_rtv_b_, nullptr);
	context->ClearRenderTargetView(bloom_rtv_b_, clear_color);
	constants.blur_direction = { 1.0f, 0.0f };
	context->UpdateSubresource(bloom_constant_buffer_, 0, nullptr, &constants, 0, 0);
	context->PSSetShader(bloom_blur_ps_, nullptr, 0);
	context->PSSetShaderResources(0, 1, &bloom_srv_a_);
	ShaderPost_Draw();
	context->PSSetShaderResources(0, 1, null_srvs);

	// Vertical blur.
	Direct3D_SetCustomRenderTarget(bloom_rtv_a_, nullptr);
	context->ClearRenderTargetView(bloom_rtv_a_, clear_color);
	constants.blur_direction = { 0.0f, 1.0f };
	context->UpdateSubresource(bloom_constant_buffer_, 0, nullptr, &constants, 0, 0);
	context->PSSetShader(bloom_blur_ps_, nullptr, 0);
	context->PSSetShaderResources(0, 1, &bloom_srv_b_);
	ShaderPost_Draw();
	context->PSSetShaderResources(0, 1, null_srvs);
	context->PSSetShader(nullptr, nullptr, 0);

	return bloom_srv_a_;
}

void PostProcessPass::execute(const RenderFrameContext& frame_context)
{
	const PostProcessSettings& post_process_settings = DebugMenu_GetPostProcessSettings();
	ID3D11ShaderResourceView* bloom_srv = RenderBloom(
		frame_context.resources.scene_color.shaderResourceView(),
		post_process_settings);

	backend_.beginBackbufferPass();

	const TerrainWaterFrameState& terrain_water = frame_context.resources.terrain_water;
	WaterSurfaceDesc water_desc{};
	const WaterSurfaceDesc* water_desc_ptr = nullptr;
	if (terrain_water.HasWaterSurfaceDesc())
	{
		water_desc = terrain_water.water_surface_desc;
		water_desc_ptr = &water_desc;
	}
	else if (frame_context.render_scene != nullptr &&
			 frame_context.render_scene->getWaterSurfaceDesc(water_desc) &&
			 water_desc.enabled)
	{
		water_desc_ptr = &water_desc;
	}
	ID3D11ShaderResourceView* water_presence_srv =
		terrain_water.water_interaction_data.isValid()
			? terrain_water.water_interaction_data.shaderResourceView()
			: (terrain_water.visible_water.isValid()
				? terrain_water.visible_water.shaderResourceView()
				: nullptr);
	const DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&frame_context.globals.view_matrix);
	const DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&frame_context.globals.projection_matrix);
	DirectX::XMFLOAT4X4 inverse_view_projection{};
	DirectX::XMStoreFloat4x4(
		&inverse_view_projection,
		DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, view * proj)));
	ShaderGlitch_Draw(
		frame_context.resources.scene_color.shaderResourceView(),
		water_presence_srv,
		bloom_srv,
		frame_context.resources.scene_depth.isValid()
			? frame_context.resources.scene_depth.shaderResourceView()
			: nullptr,
		static_cast<float>(frame_context.globals.time_seconds),
		frame_context.globals.glitch_amount,
		frame_context.globals.camera_position,
		inverse_view_projection,
		post_process_settings,
		water_desc_ptr);
}
