#include "ui_pass.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "compute_texture_dimensions.h"
#include "compute_noise_texture.h"
#include "debug_menu.h"
#include "direct3d.h"
#include "render_frame_context.h"
#include "render_resource_usage.h"
#include "render_scene.h"
#include "sprite.h"
#include "texture.h"

namespace
{
	constexpr bool kDrawAtmospherePreview = true;
	constexpr float kPreviewSize = 192.0f;
	constexpr float kPreviewMargin = 16.0f;
	constexpr int kWindOverlayCols = 20;
	constexpr int kWindOverlayRows = 20;
	constexpr float kClimateWorldMinX = -ComputeTextureDimensions::kWorldHalfExtent;
	constexpr float kClimateWorldMaxX = ComputeTextureDimensions::kWorldHalfExtent;
	constexpr float kClimateWorldMinZ = -ComputeTextureDimensions::kWorldHalfExtent;
	constexpr float kClimateWorldMaxZ = ComputeTextureDimensions::kWorldHalfExtent;
	ID3D11Texture2D* g_MeteorographPreviewStagingTexture = nullptr;
	float g_MeteorographPreviewLastReadbackTime = -1000.0f;
	bool g_MeteorographPreviewSamplesValid = false;
	DirectX::XMFLOAT3 g_MeteorographPreviewSamples[kWindOverlayCols * kWindOverlayRows]{};

	template <typename T>
	void SafeReleaseLocal(T*& resource)
	{
		if (resource != nullptr)
		{
			resource->Release();
			resource = nullptr;
		}
	}

	int getArrowTextureId()
	{
		static int arrow_texture_id = -1;
		if (arrow_texture_id < 0)
		{
			arrow_texture_id = Texture_Load(L"resource/texture/white.png");
		}
		return arrow_texture_id;
	}

	float frac(float value)
	{
		return value - std::floor(value);
	}

	float hash21(float x, float y)
	{
		float px = frac(x * 123.34f);
		float py = frac(y * 345.45f);
		float dot_term = px * (px + 34.345f) + py * (py + 34.345f);
		px += dot_term;
		py += dot_term;
		return frac(px * py);
	}

	float valueNoise(float x, float y)
	{
		const float cell_x = std::floor(x);
		const float cell_y = std::floor(y);
		const float local_x = frac(x);
		const float local_y = frac(y);
		const float smooth_x = local_x * local_x * (3.0f - 2.0f * local_x);
		const float smooth_y = local_y * local_y * (3.0f - 2.0f * local_y);

		const float v00 = hash21(cell_x + 0.0f, cell_y + 0.0f);
		const float v10 = hash21(cell_x + 1.0f, cell_y + 0.0f);
		const float v01 = hash21(cell_x + 0.0f, cell_y + 1.0f);
		const float v11 = hash21(cell_x + 1.0f, cell_y + 1.0f);

		const float lerp_x0 = v00 + (v10 - v00) * smooth_x;
		const float lerp_x1 = v01 + (v11 - v01) * smooth_x;
		return lerp_x0 + (lerp_x1 - lerp_x0) * smooth_y;
	}

	float fbm(float x, float y)
	{
		float value = 0.0f;
		float amplitude = 0.5f;
		float domain_x = x;
		float domain_y = y;

		for (int octave = 0; octave < 4; ++octave)
		{
			value += valueNoise(domain_x, domain_y) * amplitude;
			domain_x = domain_x * 2.03f + 17.0f;
			domain_y = domain_y * 2.03f + 9.0f;
			amplitude *= 0.5f;
		}

		return value;
	}

	DirectX::XMFLOAT2 safeNormalize2(float x, float y)
	{
		const float length_sq = x * x + y * y;
		if (length_sq < 1.0e-6f)
		{
			return { 1.0f, 0.0f };
		}
		const float inv_length = 1.0f / std::sqrt(length_sq);
		return { x * inv_length, y * inv_length };
	}

	DirectX::XMFLOAT3 sampleWindFieldCpu(
		float uv_x,
		float uv_y,
		float time_seconds,
		const ComputeNoiseSettings& settings)
	{
		float wind_x = settings.wind_direction_x;
		float wind_y = settings.wind_direction_y;
		const DirectX::XMFLOAT2 base_wind = safeNormalize2(wind_x, wind_y);
		wind_x = base_wind.x;
		wind_y = base_wind.y;

		const float cross_x = -wind_y;
		const float cross_y = wind_x;
		const float noise_scale = std::max(settings.noise_scale, 1.0f);
		const float domain_x = uv_x * noise_scale;
		const float domain_y = uv_y * noise_scale;

		const float drift_x = wind_x * static_cast<float>(time_seconds) * 0.008f
			+ cross_x * std::sin(static_cast<float>(time_seconds) * 0.004f) * 0.025f;
		const float drift_y = wind_y * static_cast<float>(time_seconds) * 0.008f
			+ cross_y * std::sin(static_cast<float>(time_seconds) * 0.004f) * 0.025f;

		const float jet_band_north = std::exp(-std::pow((uv_y - 0.22f) / 0.14f, 2.0f));
		const float jet_band_mid = std::exp(-std::pow((uv_y - 0.52f) / 0.20f, 2.0f));
		const float jet_band_south = std::exp(-std::pow((uv_y - 0.80f) / 0.16f, 2.0f));
		const float bias_flow_x = wind_x * (0.30f + jet_band_north * 0.36f + jet_band_mid * 0.12f - jet_band_south * 0.08f)
			+ cross_x * ((jet_band_north - jet_band_south) * 0.14f + (jet_band_mid - 0.35f) * 0.06f);
		const float bias_flow_y = wind_y * (0.30f + jet_band_north * 0.36f + jet_band_mid * 0.12f - jet_band_south * 0.08f)
			+ cross_y * ((jet_band_north - jet_band_south) * 0.14f + (jet_band_mid - 0.35f) * 0.06f);

		const float warp_x = fbm(domain_x * 0.14f + drift_x * 10.0f + 7.1f, domain_y * 0.14f + drift_y * 10.0f + 13.4f) - 0.5f;
		const float warp_y = fbm(domain_x * 0.14f - drift_x * 8.0f - 4.8f, domain_y * 0.14f - drift_y * 8.0f + 3.2f) - 0.5f;
		const float flow_uv_x = uv_x + drift_x + warp_x * (0.08f + settings.wind_cross_influence * 0.06f);
		const float flow_uv_y = uv_y + drift_y + warp_y * (0.08f + settings.wind_cross_influence * 0.06f);

		const float eps = 0.010f;
		const float potential_x1 =
			fbm((flow_uv_x + eps) * 2.0f + 11.0f, flow_uv_y * 2.0f + 5.0f) +
			fbm((flow_uv_x + eps) * 4.2f - 9.2f, flow_uv_y * 4.2f + 14.7f) * 0.28f;
		const float potential_x0 =
			fbm((flow_uv_x - eps) * 2.0f + 11.0f, flow_uv_y * 2.0f + 5.0f) +
			fbm((flow_uv_x - eps) * 4.2f - 9.2f, flow_uv_y * 4.2f + 14.7f) * 0.28f;
		const float potential_y1 =
			fbm(flow_uv_x * 2.0f + 11.0f, (flow_uv_y + eps) * 2.0f + 5.0f) +
			fbm(flow_uv_x * 4.2f - 9.2f, (flow_uv_y + eps) * 4.2f + 14.7f) * 0.28f;
		const float potential_y0 =
			fbm(flow_uv_x * 2.0f + 11.0f, (flow_uv_y - eps) * 2.0f + 5.0f) +
			fbm(flow_uv_x * 4.2f - 9.2f, (flow_uv_y - eps) * 4.2f + 14.7f) * 0.28f;
		const float dphi_dx = (potential_x1 - potential_x0) / (eps * 2.0f);
		const float dphi_dy = (potential_y1 - potential_y0) / (eps * 2.0f);
		const float curl_flow_x = dphi_dy;
		const float curl_flow_y = -dphi_dx;

		const float combined_flow_x = curl_flow_x * 0.92f + bias_flow_x;
		const float combined_flow_y = curl_flow_y * 0.92f + bias_flow_y;
		const DirectX::XMFLOAT2 local_dir = safeNormalize2(combined_flow_x, combined_flow_y);

		const float strength_noise = fbm(flow_uv_x * 1.4f + 9.6f, flow_uv_y * 1.4f - 6.4f);
		const float strength = std::clamp(
			0.08f + settings.wind_strength * 1.35f
				+ std::sqrt(curl_flow_x * curl_flow_x + curl_flow_y * curl_flow_y) * 0.16f
				+ std::max(std::max(jet_band_north, jet_band_mid), jet_band_south) * 0.10f
				+ (strength_noise - 0.5f) * 0.06f,
			0.0f,
			1.0f);
		return { local_dir.x, local_dir.y, strength };
	}

	bool MapMeteorographPreviewTexture(
		ID3D11ShaderResourceView* meteorograph_srv,
		D3D11_TEXTURE2D_DESC& out_desc,
		D3D11_MAPPED_SUBRESOURCE& out_mapped)
	{
		if (meteorograph_srv == nullptr)
		{
			return false;
		}

		ID3D11Resource* resource = nullptr;
		meteorograph_srv->GetResource(&resource);
		if (resource == nullptr)
		{
			return false;
		}

		ID3D11Texture2D* source_texture = nullptr;
		const HRESULT query_result = resource->QueryInterface(
			__uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(&source_texture));
		resource->Release();
		if (FAILED(query_result) || source_texture == nullptr)
		{
			return false;
		}

		source_texture->GetDesc(&out_desc);

		bool needs_recreate = g_MeteorographPreviewStagingTexture == nullptr;
		if (!needs_recreate)
		{
			D3D11_TEXTURE2D_DESC staging_desc{};
			g_MeteorographPreviewStagingTexture->GetDesc(&staging_desc);
			needs_recreate =
				staging_desc.Width != out_desc.Width ||
				staging_desc.Height != out_desc.Height ||
				staging_desc.Format != out_desc.Format;
		}

		if (needs_recreate)
		{
			SafeReleaseLocal(g_MeteorographPreviewStagingTexture);

			D3D11_TEXTURE2D_DESC staging_desc = out_desc;
			staging_desc.BindFlags = 0;
			staging_desc.MiscFlags = 0;
			staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			staging_desc.Usage = D3D11_USAGE_STAGING;
			staging_desc.MipLevels = 1;
			staging_desc.ArraySize = 1;
			if (FAILED(Direct3D_GetDevice()->CreateTexture2D(
					&staging_desc,
					nullptr,
					&g_MeteorographPreviewStagingTexture)))
			{
				source_texture->Release();
				return false;
			}
		}

		Direct3D_GetContext()->CopyResource(g_MeteorographPreviewStagingTexture, source_texture);
		source_texture->Release();
		return SUCCEEDED(Direct3D_GetContext()->Map(
			g_MeteorographPreviewStagingTexture,
			0,
			D3D11_MAP_READ,
			0,
			&out_mapped));
	}

	void UnmapMeteorographPreviewTexture()
	{
		if (g_MeteorographPreviewStagingTexture != nullptr)
		{
			Direct3D_GetContext()->Unmap(g_MeteorographPreviewStagingTexture, 0);
		}
	}

	bool DecodeMeteorographWindPixel(
		const D3D11_TEXTURE2D_DESC& texture_desc,
		const D3D11_MAPPED_SUBRESOURCE& mapped_resource,
		unsigned int x,
		unsigned int y,
		DirectX::XMFLOAT3& out_wind)
	{
		out_wind = { 1.0f, 0.0f, 0.0f };
		x = std::min(x, texture_desc.Width - 1u);
		y = std::min(y, texture_desc.Height - 1u);

		const unsigned char* row_ptr =
			static_cast<const unsigned char*>(mapped_resource.pData) + mapped_resource.RowPitch * y;

		auto half_to_float = [](unsigned short value) -> float
		{
			const uint32_t sign = (static_cast<uint32_t>(value & 0x8000u)) << 16;
			uint32_t exponent = (value & 0x7C00u) >> 10;
			uint32_t mantissa = value & 0x03FFu;
			uint32_t bits = 0u;

			if (exponent == 0u)
			{
				if (mantissa == 0u)
				{
					bits = sign;
				}
				else
				{
					exponent = 1u;
					while ((mantissa & 0x0400u) == 0u)
					{
						mantissa <<= 1u;
						--exponent;
					}
					mantissa &= 0x03FFu;
					bits = sign | ((exponent + (127u - 15u)) << 23) | (mantissa << 13);
				}
			}
			else if (exponent == 0x1Fu)
			{
				bits = sign | 0x7F800000u | (mantissa << 13);
			}
			else
			{
				bits = sign | ((exponent + (127u - 15u)) << 23) | (mantissa << 13);
			}

			float result = 0.0f;
			std::memcpy(&result, &bits, sizeof(result));
			return result;
		};

		if (texture_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
		{
			const unsigned short* pixel_ptr = reinterpret_cast<const unsigned short*>(row_ptr) + x * 4u;
			float dir_x = half_to_float(pixel_ptr[0]);
			float dir_y = half_to_float(pixel_ptr[1]);
			float strength = std::clamp(std::sqrt(dir_x * dir_x + dir_y * dir_y), 0.0f, 1.0f);
			const float dir_length = std::sqrt(dir_x * dir_x + dir_y * dir_y);
			if (dir_length > 1.0e-6f)
			{
				dir_x /= dir_length;
				dir_y /= dir_length;
			}
			else
			{
				dir_x = 1.0f;
				dir_y = 0.0f;
			}
			out_wind = { dir_x, dir_y, strength };
			return true;
		}

		if (texture_desc.Format == DXGI_FORMAT_R32G32B32A32_FLOAT)
		{
			const float* pixel_ptr = reinterpret_cast<const float*>(row_ptr) + x * 4u;
			float dir_x = pixel_ptr[0];
			float dir_y = pixel_ptr[1];
			float strength = std::clamp(std::sqrt(dir_x * dir_x + dir_y * dir_y), 0.0f, 1.0f);
			const float dir_length = std::sqrt(dir_x * dir_x + dir_y * dir_y);
			if (dir_length > 1.0e-6f)
			{
				dir_x /= dir_length;
				dir_y /= dir_length;
			}
			else
			{
				dir_x = 1.0f;
				dir_y = 0.0f;
			}
			out_wind = { dir_x, dir_y, strength };
			return true;
		}

		return false;
	}

	bool RefreshMeteorographPreviewSamples(
		ID3D11ShaderResourceView* meteorograph_srv,
		float time_seconds)
	{
		if (g_MeteorographPreviewSamplesValid &&
			(time_seconds - g_MeteorographPreviewLastReadbackTime) < 0.18f)
		{
			return true;
		}

		D3D11_TEXTURE2D_DESC preview_desc{};
		D3D11_MAPPED_SUBRESOURCE preview_mapped{};
		if (!MapMeteorographPreviewTexture(meteorograph_srv, preview_desc, preview_mapped))
		{
			g_MeteorographPreviewSamplesValid = false;
			return false;
		}

		for (int row = 0; row < kWindOverlayRows; ++row)
		{
			for (int col = 0; col < kWindOverlayCols; ++col)
			{
				const float u = (static_cast<float>(col) + 0.5f) / static_cast<float>(kWindOverlayCols);
				const float v = (static_cast<float>(row) + 0.5f) / static_cast<float>(kWindOverlayRows);
				const unsigned int sample_x = static_cast<unsigned int>(
					std::clamp(u * static_cast<float>(preview_desc.Width), 0.0f, static_cast<float>(preview_desc.Width - 1u)));
				const unsigned int sample_y = static_cast<unsigned int>(
					std::clamp(v * static_cast<float>(preview_desc.Height), 0.0f, static_cast<float>(preview_desc.Height - 1u)));
				DirectX::XMFLOAT3 wind{};
				DecodeMeteorographWindPixel(preview_desc, preview_mapped, sample_x, sample_y, wind);
				g_MeteorographPreviewSamples[row * kWindOverlayCols + col] = wind;
			}
		}

		UnmapMeteorographPreviewTexture();
		g_MeteorographPreviewLastReadbackTime = time_seconds;
		g_MeteorographPreviewSamplesValid = true;
		return true;
	}

	void drawWindOverlay(
		float origin_x,
		float origin_y,
		float width,
		float height,
		float time_seconds,
		ID3D11ShaderResourceView* meteorograph_srv,
		const ComputeNoiseSettings& settings)
	{
		const int arrow_texture_id = getArrowTextureId();
		if (arrow_texture_id < 0)
		{
			return;
		}

		const bool using_gpu_preview =
			meteorograph_srv != nullptr && RefreshMeteorographPreviewSamples(meteorograph_srv, time_seconds);

		for (int row = 0; row < kWindOverlayRows; ++row)
		{
			for (int col = 0; col < kWindOverlayCols; ++col)
			{
				const float uv_x = (static_cast<float>(col) + 0.5f) / static_cast<float>(kWindOverlayCols);
				const float uv_y = (static_cast<float>(row) + 0.5f) / static_cast<float>(kWindOverlayRows);
				const DirectX::XMFLOAT3 wind =
					using_gpu_preview
						? g_MeteorographPreviewSamples[row * kWindOverlayCols + col]
						: sampleWindFieldCpu(uv_x, uv_y, time_seconds, settings);

				const float center_x = origin_x + uv_x * width;
				const float center_y = origin_y + uv_y * height;
				const float angle = std::atan2(wind.y, wind.x);
				const float shaft_length = 2.8f + wind.z * 8.2f;
				const float shaft_thickness = 1.2f;
				const float head_length = 1.8f + wind.z * 2.6f;
				const float head_thickness = 1.05f;
				const DirectX::XMFLOAT4 shaft_color = { 1.0f, 1.0f, 1.0f, 0.62f };
				const DirectX::XMFLOAT4 head_color = { 1.0f, 1.0f, 1.0f, 0.78f };

				Sprite_Draw(
					arrow_texture_id,
					center_x,
					center_y,
					shaft_length,
					shaft_thickness,
					0,
					0,
					1,
					1,
					angle,
					shaft_color);

				const float tip_offset_x = std::cos(angle) * (shaft_length * 0.35f);
				const float tip_offset_y = std::sin(angle) * (shaft_length * 0.35f);

				Sprite_Draw(
					arrow_texture_id,
					center_x + tip_offset_x,
					center_y + tip_offset_y,
					head_length,
					head_thickness,
					0,
					0,
					1,
					1,
					angle + 0.55f,
					head_color);

				Sprite_Draw(
					arrow_texture_id,
					center_x + tip_offset_x,
					center_y + tip_offset_y,
					head_length,
					head_thickness,
					0,
					0,
					1,
					1,
					angle - 0.55f,
					head_color);
			}
		}
	}

	bool worldToClimateUv(float world_x, float world_z, float& out_uv_x, float& out_uv_y)
	{
		const float denom_x = kClimateWorldMaxX - kClimateWorldMinX;
		const float denom_z = kClimateWorldMaxZ - kClimateWorldMinZ;
		out_uv_x = (world_x - kClimateWorldMinX) / denom_x;
		out_uv_y = (world_z - kClimateWorldMinZ) / denom_z;
		return out_uv_x >= 0.0f && out_uv_x <= 1.0f && out_uv_y >= 0.0f && out_uv_y <= 1.0f;
	}

	void drawCurrentPositionMarker(float origin_x, float origin_y, float width, float height, float world_x, float world_z)
	{
		float uv_x = 0.0f;
		float uv_y = 0.0f;
		if (!worldToClimateUv(world_x, world_z, uv_x, uv_y))
		{
			return;
		}

		const int marker_texture_id = getArrowTextureId();
		if (marker_texture_id < 0)
		{
			return;
		}

		const float center_x = origin_x + uv_x * width;
		const float center_y = origin_y + uv_y * height;
		const float radius = 6.0f;
		const float thickness = 1.6f;
		const DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 0.95f };

		for (int segment = 0; segment < 12; ++segment)
		{
			const float angle = (static_cast<float>(segment) / 12.0f) * 6.2831853f;
			const float px = center_x + std::cos(angle) * radius;
			const float py = center_y + std::sin(angle) * radius;
			Sprite_Draw(
				marker_texture_id,
				px,
				py,
				3.2f,
				thickness,
				0,
				0,
				1,
				1,
				angle + 1.5707963f,
				color);
		}
	}
}

std::string_view UIPass::name() const
{
	return "UIPass";
}

std::span<const RenderResourceUsage> UIPass::resources() const
{
	static constexpr std::array usages = {
		RenderResourceUsage{ RenderResourceId::BackBuffer, RenderResourceAccess::ReadWrite }
	};
	return usages;
}

void UIPass::execute(const RenderFrameContext& frame_context)
{
	const ComputeNoiseSettings& compute_settings = DebugMenu_GetComputeNoiseSettings();

	if (frame_context.render_scene != nullptr)
	{
		frame_context.render_scene->drawUI(frame_context);
	}

	if (!kDrawAtmospherePreview)
	{
		return;
	}

	if (!frame_context.resources.meteorograph_field.isValid())
	{
		return;
	}

	Sprite_Draw(
		kPreviewMargin - 4.0f,
		kPreviewMargin - 4.0f,
		kPreviewSize + 8.0f,
		kPreviewSize + 8.0f,
		DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.55f));

	Sprite_DrawSRV(
		frame_context.resources.atmosphere_preview.isValid()
			? frame_context.resources.atmosphere_preview.shaderResourceView()
			: frame_context.resources.meteorograph_field.shaderResourceView(),
		kPreviewMargin,
		kPreviewMargin,
		kPreviewSize,
		kPreviewSize);
	drawWindOverlay(
		kPreviewMargin,
		kPreviewMargin,
		kPreviewSize,
		kPreviewSize,
		static_cast<float>(frame_context.globals.time_seconds),
		frame_context.resources.meteorograph_field.shaderResourceView(),
		compute_settings);

}
