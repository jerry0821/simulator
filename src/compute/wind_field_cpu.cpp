#include "wind_field_cpu.h"

#include <algorithm>
#include <cmath>

#include "compute_noise_texture.h"

using namespace DirectX;

namespace
{
float hash2d(float x, float y)
{
    const float value = std::sin(x * 127.1f + y * 311.7f) * 43758.5453f;
    return value - std::floor(value);
}

float lerpScalar(float a, float b, float t)
{
    return a + (b - a) * t;
}

float smoothValueNoise(float x, float y)
{
    const float x0 = std::floor(x);
    const float y0 = std::floor(y);
    const float x1 = x0 + 1.0f;
    const float y1 = y0 + 1.0f;
    const float tx = x - x0;
    const float ty = y - y0;
    const float sx = tx * tx * (3.0f - 2.0f * tx);
    const float sy = ty * ty * (3.0f - 2.0f * ty);

    const float n00 = hash2d(x0, y0);
    const float n10 = hash2d(x1, y0);
    const float n01 = hash2d(x0, y1);
    const float n11 = hash2d(x1, y1);

    const float nx0 = lerpScalar(n00, n10, sx);
    const float nx1 = lerpScalar(n01, n11, sx);
    return lerpScalar(nx0, nx1, sy);
}

float fbm(float x, float y)
{
    float amplitude = 0.5f;
    float frequency = 1.0f;
    float value = 0.0f;
    float normalization = 0.0f;
    for (int octave = 0; octave < 4; ++octave)
    {
        value += smoothValueNoise(x * frequency, y * frequency) * amplitude;
        normalization += amplitude;
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }
    return normalization > 0.0f ? value / normalization : 0.0f;
}

XMFLOAT2 safeNormalize2(float x, float y)
{
    const float length_sq = x * x + y * y;
    if (length_sq <= 1.0e-6f)
    {
        return {1.0f, 0.0f};
    }
    const float inv_length = 1.0f / std::sqrt(length_sq);
    return {x * inv_length, y * inv_length};
}

XMFLOAT2 vortexContribution(
    float uv_x,
    float uv_y,
    float center_x,
    float center_y,
    float radius,
    float swirl_sign)
{
    const float offset_x = uv_x - center_x;
    const float offset_y = uv_y - center_y;
    const float dist_sq = offset_x * offset_x + offset_y * offset_y;
    const float radius_sq = radius * radius;
    if (dist_sq >= radius_sq || dist_sq <= 1.0e-6f)
    {
        return {0.0f, 0.0f};
    }

    const float dist = std::sqrt(dist_sq);
    const float nx = offset_x / dist;
    const float ny = offset_y / dist;
    const float tangent_x = -ny;
    const float tangent_y = nx;
    const float falloff = (1.0f - dist / radius);
    return {tangent_x * swirl_sign * falloff, tangent_y * swirl_sign * falloff};
}

XMFLOAT3 sampleWindFieldCpu(
    float uv_x,
    float uv_y,
    float time_seconds,
    const ComputeNoiseSettings& settings,
    float world_min_x,
    float world_max_x,
    float world_min_z,
    float world_max_z)
{
    (void)world_min_x;
    (void)world_max_x;
    (void)world_min_z;
    (void)world_max_z;

    float wind_x = settings.wind_direction_x;
    float wind_y = settings.wind_direction_y;
    const XMFLOAT2 base_wind = safeNormalize2(wind_x, wind_y);
    wind_x = base_wind.x;
    wind_y = base_wind.y;

    const float cross_x = -wind_y;
    const float cross_y = wind_x;
    const float noise_scale = std::max(settings.noise_scale, 1.0f);
    const float domain_x = uv_x * noise_scale;
    const float domain_y = uv_y * noise_scale;

    const float time_a = time_seconds * 0.010f;
    const float time_b = time_seconds * 0.007f;
    const float drift_x = wind_x * time_seconds * 0.007f + cross_x * std::sin(time_seconds * 0.005f) * 0.035f;
    const float drift_y = wind_y * time_seconds * 0.007f + cross_y * std::sin(time_seconds * 0.005f) * 0.035f;
    const float vortex_a_center_x = 0.24f + std::sin(time_a) * 0.025f;
    const float vortex_a_center_y = 0.32f + std::cos(time_a * 0.7f) * 0.025f;
    const float vortex_b_center_x = 0.76f + std::cos(time_b * 0.8f) * 0.032f;
    const float vortex_b_center_y = 0.64f + std::sin(time_b) * 0.032f;

    const XMFLOAT2 vortex_a = vortexContribution(uv_x, uv_y, vortex_a_center_x, vortex_a_center_y, 0.30f, +1.0f);
    const XMFLOAT2 vortex_b = vortexContribution(uv_x, uv_y, vortex_b_center_x, vortex_b_center_y, 0.34f, -1.0f);
    const float vortex_flow_x = vortex_a.x + vortex_b.x;
    const float vortex_flow_y = vortex_a.y + vortex_b.y;

    const float jet_band_north = std::exp(-std::pow((uv_y - 0.24f) / 0.12f, 2.0f));
    const float jet_band_mid = std::exp(-std::pow((uv_y - 0.52f) / 0.18f, 2.0f));
    const float jet_band_south = std::exp(-std::pow((uv_y - 0.78f) / 0.14f, 2.0f));
    const float jet_flow_x = wind_x * (0.62f + jet_band_north * 0.95f + jet_band_mid * 0.28f - jet_band_south * 0.22f)
                           + cross_x * ((jet_band_north - jet_band_south) * 0.34f + (jet_band_mid - 0.35f) * 0.18f);
    const float jet_flow_y = wind_y * (0.62f + jet_band_north * 0.95f + jet_band_mid * 0.28f - jet_band_south * 0.22f)
                           + cross_y * ((jet_band_north - jet_band_south) * 0.34f + (jet_band_mid - 0.35f) * 0.18f);

    const float warp_x = fbm(domain_x * 0.14f + drift_x * 14.0f + 7.1f, domain_y * 0.14f + drift_y * 14.0f + 13.4f) - 0.5f;
    const float warp_y = fbm(domain_x * 0.14f - drift_x * 12.0f - 4.8f, domain_y * 0.14f - drift_y * 12.0f + 3.2f) - 0.5f;
    const float flow_uv_x = uv_x + drift_x + warp_x * 0.11f;
    const float flow_uv_y = uv_y + drift_y + warp_y * 0.11f;

    const float eps = 0.012f;
    const float potential_x1 = fbm((flow_uv_x + eps) * 2.2f + 18.7f, flow_uv_y * 2.2f - 11.3f);
    const float potential_x0 = fbm((flow_uv_x - eps) * 2.2f + 18.7f, flow_uv_y * 2.2f - 11.3f);
    const float potential_y1 = fbm(flow_uv_x * 2.2f + 18.7f, (flow_uv_y + eps) * 2.2f - 11.3f);
    const float potential_y0 = fbm(flow_uv_x * 2.2f + 18.7f, (flow_uv_y - eps) * 2.2f - 11.3f);
    const float dphi_dx = (potential_x1 - potential_x0) / (2.0f * eps);
    const float dphi_dy = (potential_y1 - potential_y0) / (2.0f * eps);
    const float curl_flow_x = dphi_dy;
    const float curl_flow_y = -dphi_dx;

    const float micro_angle = (fbm(flow_uv_x * 3.2f + 17.2f, flow_uv_y * 3.2f + 4.8f) - 0.5f) * 0.22f * settings.wind_cross_influence;
    const float sin_a = std::sin(micro_angle);
    const float cos_a = std::cos(micro_angle);
    const float micro_flow_x = (wind_x * cos_a - wind_y * sin_a) * 0.10f;
    const float micro_flow_y = (wind_x * sin_a + wind_y * cos_a) * 0.10f;

    const float combined_flow_x = jet_flow_x + vortex_flow_x * 0.92f + curl_flow_x * 0.34f + micro_flow_x;
    const float combined_flow_y = jet_flow_y + vortex_flow_y * 0.92f + curl_flow_y * 0.34f + micro_flow_y;
    const XMFLOAT2 local_dir = safeNormalize2(combined_flow_x, combined_flow_y);

    const float strength_noise = fbm(flow_uv_x * 1.7f + 9.6f, flow_uv_y * 1.7f - 6.4f);
    const float vortex_strength = std::clamp(
        std::sqrt(vortex_flow_x * vortex_flow_x + vortex_flow_y * vortex_flow_y) * 0.50f,
        0.0f,
        1.0f);
    const float strength = std::clamp(
        0.08f + settings.wind_strength * 1.45f + vortex_strength * 0.24f
            + std::sqrt(curl_flow_x * curl_flow_x + curl_flow_y * curl_flow_y) * 0.08f
            + std::max(std::max(jet_band_north, jet_band_mid), jet_band_south) * 0.08f
            + (strength_noise - 0.5f) * 0.06f,
        0.0f,
        1.0f);
    return {local_dir.x, local_dir.y, strength};
}
}

XMFLOAT3 WindFieldCpu_SampleWorld(
    float world_x,
    float world_z,
    float time_seconds,
    const ComputeNoiseSettings& settings,
    float world_min_x,
    float world_max_x,
    float world_min_z,
    float world_max_z)
{
    const float range_x = std::max(world_max_x - world_min_x, 1.0e-4f);
    const float range_z = std::max(world_max_z - world_min_z, 1.0e-4f);
    const float uv_x = std::clamp((world_x - world_min_x) / range_x, 0.0f, 1.0f);
    const float uv_y = std::clamp((world_z - world_min_z) / range_z, 0.0f, 1.0f);
    return sampleWindFieldCpu(
        uv_x,
        uv_y,
        time_seconds,
        settings,
        world_min_x,
        world_max_x,
        world_min_z,
        world_max_z);
}
