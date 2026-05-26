// ----------------------------------------------------
// debug gui [debug_gui.h]
// ====================================================
// Created by: Jerry
// Date: 2025-12-11
// ----------------------------------------------------
#include "debug_menu.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "camera.h"
#include "compute_noise_texture.h"
#include "compute_shared_resource_registry.h"
#include "compute_texture_dimensions.h"
#include "direct3d.h"
#include "frustum_culling_debug.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include "instancing_debug.h"
#include "light.h"
#include "meshfield.h"
#include "render_frame_context.h"
#include "render_frame_plan.h"
#include "render_resource_usage.h"
#include "render_water_surface.h"
#include "scene_stress_debug.h"
#include "shader_field.h"
#include "terrain_surface_settings.h"
#include "wind_field_cpu.h"
#include <DirectXMath.h>

using namespace DirectX;

static RenderFramePlan g_FramePlan;
static InstancingStats g_InstancingStats;
static FrustumCullingStats g_FrustumCullingStats;
static GrassComputeStats g_GrassComputeStats;
static float g_DebugFps = 0.0f;
static float g_DebugFrameTimeMs = 0.0f;
static bool g_ShaderReloadRequested = false;
static bool g_ShaderReloadSucceeded = true;
static std::string g_ShaderReloadMessage = "Ready";
static bool g_SurfaceWaterInjectionRequested = false;
static bool g_SurfaceWaterResetRequested = false;
static bool g_ShowComputeResourcePreviews = false;
static bool g_EnableTerrainSurfacePresentation = true;
static bool g_EnableGrassGpu = true;
struct DebugWindowVisibility
{
    bool portfolio = true;
    bool compute_noise = false;
    bool wind = true;
    bool atmosphere = true;
    bool water = true;
    bool post_fx = false;
    bool terrain = true;
    bool camera = false;
    bool toon = false;
    bool compute_registry = false;
    bool instancing = true;
    bool frustum_culling = false;
    bool grass_compute = true;
    bool render_flow = false;
};
static DebugWindowVisibility g_DebugWindows{};
static ComputeNoiseSettings g_ComputeNoiseSettings{};
static TerrainSettings g_TerrainSettings = MeshFieldRenderer::GetTerrainSettings();
struct TerrainAuthoringControls
{
    float plains_coverage = 0.45f;
    float peak_density = 0.45f;
    float peak_height = 0.55f;
    float lake_amount = 0.0f;
    float lake_size = 0.35f;
};
static TerrainAuthoringControls g_TerrainAuthoringControls{};
static bool g_TerrainAuthoringControlsInitialized = false;
static TerrainMaterialSettings g_TerrainMaterialSettings{};
static WaterSurfaceDesc g_WaterSurfaceSettings{};
static SurfaceWaterSimulationSettings g_SurfaceWaterSimulationSettings{};
static PostProcessSettings g_PostProcessSettings{};
static ID3D11Texture2D* g_WindFieldPreviewStagingTexture = nullptr;
static bool g_UseAccurateWindPreviewArrows = false;
static float g_WindFieldPreviewLastReadbackTime = -1000.0f;
static bool g_WindFieldPreviewSamplesValid = false;
static XMFLOAT3 g_WindFieldPreviewSamples[18 * 18]{};

namespace
{
constexpr float kWindPreviewWorldMinX = -ComputeTextureDimensions::kWorldHalfExtent;
constexpr float kWindPreviewWorldMaxX = ComputeTextureDimensions::kWorldHalfExtent;
constexpr float kWindPreviewWorldMinZ = -ComputeTextureDimensions::kWorldHalfExtent;
constexpr float kWindPreviewWorldMaxZ = ComputeTextureDimensions::kWorldHalfExtent;
constexpr int kWindPreviewCols = 18;
constexpr int kWindPreviewRows = 18;

float Saturate(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

float InverseLerpClamped(float a, float b, float value)
{
    return Saturate((value - a) / std::max(b - a, 1.0e-5f));
}

void SyncTerrainAuthoringControlsFromSettings(
    const TerrainSettings& settings,
    TerrainAuthoringControls& controls)
{
    const float peak_density = (
        InverseLerpClamped(0.0015f, 0.0200f, settings.base_frequency) +
        InverseLerpClamped(1.0f, 8.0f, settings.detail_frequency) +
        InverseLerpClamped(0.5f, 4.0f, settings.ridge_frequency)) / 3.0f;
    const float peak_height = (
        InverseLerpClamped(4.0f, 36.0f, settings.base_height) +
        InverseLerpClamped(0.0f, 12.0f, settings.detail_height) +
        InverseLerpClamped(0.0f, 28.0f, settings.ridge_height) +
        InverseLerpClamped(-2.0f, 10.0f, settings.continent_height)) / 4.0f;

    controls.plains_coverage = Saturate(1.0f - (peak_density * 0.45f + peak_height * 0.55f));
    controls.peak_density = peak_density;
    controls.peak_height = peak_height;
    controls.lake_amount = InverseLerpClamped(0.0f, 20.0f, settings.lake_depth);
    controls.lake_size = (
        InverseLerpClamped(8.0f, 80.0f, settings.lake_radius_x) +
        InverseLerpClamped(8.0f, 80.0f, settings.lake_radius_z)) * 0.5f;
}

void ApplyTerrainAuthoringControlsToSettings(
    const TerrainAuthoringControls& controls,
    TerrainSettings& settings)
{
    const float plains = Saturate(controls.plains_coverage);
    const float peak_density = Saturate(controls.peak_density);
    const float peak_height = Saturate(controls.peak_height);
    const float lake_amount = Saturate(controls.lake_amount);
    const float lake_size = Saturate(controls.lake_size);

    settings.base_frequency = Lerp(0.0021f, 0.0075f, peak_density);
    settings.detail_frequency = Lerp(1.2f, 4.6f, peak_density);
    settings.ridge_frequency = Lerp(0.55f, 1.85f, peak_density);

    settings.base_height = Lerp(10.0f, 30.0f, peak_height);
    settings.detail_height = Lerp(0.8f, 8.0f, peak_height);
    settings.ridge_height = Lerp(3.0f, 24.0f, peak_height);
    settings.continent_height = Lerp(0.0f, 8.0f, peak_height);

    settings.base_frequency *= Lerp(1.0f, 0.72f, plains);
    settings.detail_frequency *= Lerp(1.0f, 0.78f, plains);
    settings.ridge_frequency *= Lerp(1.0f, 0.80f, plains);
    settings.base_height = Lerp(settings.base_height, 9.0f, plains * 0.55f);
    settings.detail_height *= Lerp(1.0f, 0.35f, plains);
    settings.ridge_height *= Lerp(1.0f, 0.24f, plains);
    settings.continent_height *= Lerp(1.0f, 0.55f, plains);

    settings.lake_depth = Lerp(0.0f, 18.0f, lake_amount);
    const float lake_radius = Lerp(16.0f, 72.0f, lake_size);
    settings.lake_radius_x = lake_radius;
    settings.lake_radius_z = lake_radius;
}

void ApplyAfterglowMeteorographPreset()
{
    g_ComputeNoiseSettings.noise_scale = 12.0f;
    g_ComputeNoiseSettings.wind_direction_x = 1.0f;
    g_ComputeNoiseSettings.wind_direction_y = 0.0f;
    g_ComputeNoiseSettings.wind_strength = 0.08f;
    g_ComputeNoiseSettings.wind_cross_influence = 0.12f;
    g_ComputeNoiseSettings.flow_speed_x0 = 0.02f;
    g_ComputeNoiseSettings.flow_speed_y0 = 0.00f;
    g_ComputeNoiseSettings.flow_speed_x1 = -0.01f;
    g_ComputeNoiseSettings.flow_speed_y1 = 0.01f;
    g_ComputeNoiseSettings.band_strength = 0.04f;
    g_ComputeNoiseSettings.contrast = 1.0f;
}

ImVec4 ToColor(RenderResourceId resource_id)
{
    switch (resource_id)
    {
    case RenderResourceId::SceneColor:
        return ImVec4(0.95f, 0.65f, 0.20f, 1.0f);
    case RenderResourceId::SceneDepth:
        return ImVec4(0.45f, 0.70f, 1.00f, 1.0f);
    case RenderResourceId::ShadowMap:
        return ImVec4(0.75f, 0.65f, 0.95f, 1.0f);
    case RenderResourceId::BackBuffer:
        return ImVec4(0.60f, 0.95f, 0.65f, 1.0f);
    default:
        return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

const char* ToString(RenderResourceId resource_id)
{
    switch (resource_id)
    {
    case RenderResourceId::SceneColor:
        return "SceneColor";
    case RenderResourceId::SceneDepth:
        return "SceneDepth";
    case RenderResourceId::ShadowMap:
        return "ShadowMap";
    case RenderResourceId::BackBuffer:
        return "BackBuffer";
    default:
        return "Unknown";
    }
}

const char* ToString(RenderResourceAccess access)
{
    switch (access)
    {
    case RenderResourceAccess::Read:
        return "Read";
    case RenderResourceAccess::Write:
        return "Write";
    case RenderResourceAccess::ReadWrite:
        return "ReadWrite";
    default:
        return "Unknown";
    }
}

const char* ToString(ComputeSharedResourceKind kind)
{
    switch (kind)
    {
    case ComputeSharedResourceKind::ShaderTexture:
        return "ShaderTexture";
    case ComputeSharedResourceKind::StructuredBuffer:
        return "StructuredBuffer";
    case ComputeSharedResourceKind::IndirectArgs:
        return "IndirectArgs";
    case ComputeSharedResourceKind::None:
    default:
        return "None";
    }
}

const char* ToString(ComputeSharedResourceId id)
{
    switch (id)
    {
    case ComputeSharedResourceId::BaseTerrainHeight:
        return "BaseTerrainHeight";
    case ComputeSharedResourceId::TerrainHeight:
        return "TerrainHeight";
    case ComputeSharedResourceId::TerrainNormal:
        return "TerrainNormal";
    case ComputeSharedResourceId::TerrainSurfaceData:
        return "TerrainSurfaceData";
    case ComputeSharedResourceId::TerrainVegetationSuitability:
        return "TerrainVegetationSuitability";
    case ComputeSharedResourceId::GrassData:
        return "GrassData";
    case ComputeSharedResourceId::RainMap:
        return "RainMap";
    case ComputeSharedResourceId::AtmospherePreview:
        return "AtmospherePreview";
    case ComputeSharedResourceId::WaterSurfaceHeight:
        return "WaterSurfaceHeight";
    case ComputeSharedResourceId::SurfaceWaterFlow:
        return "SurfaceWaterFlow";
    case ComputeSharedResourceId::WaterVelocity:
        return "WaterVelocity";
    case ComputeSharedResourceId::WaterSediment:
        return "WaterSediment";
    case ComputeSharedResourceId::ComputeNoise:
        return "ComputeNoise";
    case ComputeSharedResourceId::MeteorographField:
        return "MeteorographField";
    case ComputeSharedResourceId::GrassInstances:
        return "GrassInstances";
    case ComputeSharedResourceId::GrassIndirectArgs:
        return "GrassIndirectArgs";
    case ComputeSharedResourceId::FloatingLightInstances:
        return "FloatingLightInstances";
    default:
        return "Unknown";
    }
}

void DrawResourceUsageChip(const RenderResourceUsage& usage)
{
    const ImVec4 color = ToColor(usage.resource_id);

    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::Text("%s", ToString(usage.resource_id));
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::TextDisabled("-> %s", ToString(usage.access));
}

bool MapWindFieldPreviewTexture(
    ID3D11ShaderResourceView* wind_field_srv,
    D3D11_TEXTURE2D_DESC& out_desc,
    D3D11_MAPPED_SUBRESOURCE& out_mapped)
{
    if (wind_field_srv == nullptr)
    {
        return false;
    }

    ID3D11Resource* resource = nullptr;
    wind_field_srv->GetResource(&resource);
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

    bool needs_recreate = g_WindFieldPreviewStagingTexture == nullptr;
    if (!needs_recreate)
    {
        D3D11_TEXTURE2D_DESC staging_desc{};
        g_WindFieldPreviewStagingTexture->GetDesc(&staging_desc);
        needs_recreate =
            staging_desc.Width != out_desc.Width ||
            staging_desc.Height != out_desc.Height ||
            staging_desc.Format != out_desc.Format;
    }

    if (needs_recreate)
    {
        SAFE_RELEASE(g_WindFieldPreviewStagingTexture);

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
                &g_WindFieldPreviewStagingTexture)))
        {
            source_texture->Release();
            return false;
        }
    }

    Direct3D_GetContext()->CopyResource(g_WindFieldPreviewStagingTexture, source_texture);
    source_texture->Release();
    return SUCCEEDED(Direct3D_GetContext()->Map(
        g_WindFieldPreviewStagingTexture,
        0,
        D3D11_MAP_READ,
        0,
        &out_mapped));
}

void UnmapWindFieldPreviewTexture()
{
    if (g_WindFieldPreviewStagingTexture != nullptr)
    {
        Direct3D_GetContext()->Unmap(g_WindFieldPreviewStagingTexture, 0);
    }
}

bool DecodeWindFieldPixel(
    const D3D11_TEXTURE2D_DESC& texture_desc,
    const D3D11_MAPPED_SUBRESOURCE& mapped_resource,
    unsigned int x,
    unsigned int y,
    bool raw_vector_xy,
    XMFLOAT3& out_wind)
{
    out_wind = { 1.0f, 0.0f, 0.0f };
    x = std::min(x, texture_desc.Width - 1u);
    y = std::min(y, texture_desc.Height - 1u);

    const unsigned char* row_ptr =
        static_cast<const unsigned char*>(mapped_resource.pData) + mapped_resource.RowPitch * y;
    if (texture_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM)
    {
        const unsigned char* pixel_ptr = row_ptr + x * 4u;
        float dir_x = (static_cast<float>(pixel_ptr[0]) / 255.0f) * 2.0f - 1.0f;
        float dir_y = (static_cast<float>(pixel_ptr[1]) / 255.0f) * 2.0f - 1.0f;
        const float strength = static_cast<float>(pixel_ptr[2]) / 255.0f;
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
        float strength = pixel_ptr[2];
        if (raw_vector_xy)
        {
            strength = std::clamp(std::sqrt(dir_x * dir_x + dir_y * dir_y), 0.0f, 1.0f);
        }
        else
        {
            dir_x = dir_x * 2.0f - 1.0f;
            dir_y = dir_y * 2.0f - 1.0f;
        }
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

    if (texture_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
    {
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

        const unsigned short* pixel_ptr = reinterpret_cast<const unsigned short*>(row_ptr) + x * 4u;
        float dir_x = half_to_float(pixel_ptr[0]);
        float dir_y = half_to_float(pixel_ptr[1]);
        float strength = 0.0f;
        if (raw_vector_xy)
        {
            strength = std::clamp(std::sqrt(dir_x * dir_x + dir_y * dir_y), 0.0f, 1.0f);
        }
        else
        {
            dir_x = dir_x * 2.0f - 1.0f;
            dir_y = dir_y * 2.0f - 1.0f;
            strength = std::clamp(half_to_float(pixel_ptr[2]), 0.0f, 1.0f);
        }
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

bool RefreshWindFieldPreviewSamples(
    ID3D11ShaderResourceView* wind_field_srv,
    float time_seconds,
    bool raw_vector_xy)
{
    if (!g_UseAccurateWindPreviewArrows)
    {
        return false;
    }

    if (g_WindFieldPreviewSamplesValid &&
        (time_seconds - g_WindFieldPreviewLastReadbackTime) < 0.18f)
    {
        return true;
    }

    D3D11_TEXTURE2D_DESC preview_desc{};
    D3D11_MAPPED_SUBRESOURCE preview_mapped{};
    if (!MapWindFieldPreviewTexture(wind_field_srv, preview_desc, preview_mapped))
    {
        g_WindFieldPreviewSamplesValid = false;
        return false;
    }

    for (int row = 0; row < kWindPreviewRows; ++row)
    {
        for (int col = 0; col < kWindPreviewCols; ++col)
        {
            const float u = (static_cast<float>(col) + 0.5f) / static_cast<float>(kWindPreviewCols);
            const float v = (static_cast<float>(row) + 0.5f) / static_cast<float>(kWindPreviewRows);
            const unsigned int sample_x = static_cast<unsigned int>(
                std::clamp(u * static_cast<float>(preview_desc.Width), 0.0f, static_cast<float>(preview_desc.Width - 1u)));
            const unsigned int sample_y = static_cast<unsigned int>(
                std::clamp(v * static_cast<float>(preview_desc.Height), 0.0f, static_cast<float>(preview_desc.Height - 1u)));
            XMFLOAT3 wind{};
            DecodeWindFieldPixel(
                preview_desc,
                preview_mapped,
                sample_x,
                sample_y,
                raw_vector_xy,
                wind);
            g_WindFieldPreviewSamples[row * kWindPreviewCols + col] = wind;
        }
    }

    UnmapWindFieldPreviewTexture();
    g_WindFieldPreviewLastReadbackTime = time_seconds;
    g_WindFieldPreviewSamplesValid = true;
    return true;
}

void DrawWindFieldOverlay(
    const ImVec2& top_left,
    const ImVec2& size,
    ID3D11ShaderResourceView* wind_field_srv,
    float time_seconds,
    const ComputeNoiseSettings& settings,
    bool raw_vector_xy)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (draw_list == nullptr)
    {
        return;
    }

    const float cell_width = size.x / static_cast<float>(kWindPreviewCols);
    const float cell_height = size.y / static_cast<float>(kWindPreviewRows);
    const float world_range_x = kWindPreviewWorldMaxX - kWindPreviewWorldMinX;
    const float world_range_z = kWindPreviewWorldMaxZ - kWindPreviewWorldMinZ;
    const bool using_gpu_preview = RefreshWindFieldPreviewSamples(
        wind_field_srv,
        time_seconds,
        raw_vector_xy);

    for (int row = 0; row < kWindPreviewRows; ++row)
    {
        for (int col = 0; col < kWindPreviewCols; ++col)
        {
            const float u = (static_cast<float>(col) + 0.5f) / static_cast<float>(kWindPreviewCols);
            const float v = (static_cast<float>(row) + 0.5f) / static_cast<float>(kWindPreviewRows);
            XMFLOAT3 wind{};
            if (using_gpu_preview)
            {
                wind = g_WindFieldPreviewSamples[row * kWindPreviewCols + col];
            }
            else
            {
                const float world_x = kWindPreviewWorldMinX + u * world_range_x;
                const float world_z = kWindPreviewWorldMinZ + v * world_range_z;
                wind = WindFieldCpu_SampleWorld(
                    world_x,
                    world_z,
                    time_seconds,
                    settings,
                    kWindPreviewWorldMinX,
                    kWindPreviewWorldMaxX,
                    kWindPreviewWorldMinZ,
                    kWindPreviewWorldMaxZ);
            }

            const float center_x = top_left.x + u * size.x;
            const float center_y = top_left.y + v * size.y;
            const float angle = std::atan2(wind.y, wind.x);
            const float strength = std::clamp(wind.z, 0.0f, 1.0f);
            const float line_length = 4.0f + strength * std::min(cell_width, cell_height) * 0.70f;
            const float line_half = line_length * 0.5f;
            const float dx = std::cos(angle) * line_half;
            const float dy = std::sin(angle) * line_half;
            const ImVec2 start(center_x - dx, center_y - dy);
            const ImVec2 tip(center_x + dx, center_y + dy);
            const float head_size = 2.5f + strength * 4.0f;
            const ImVec2 head_left(
                tip.x - std::cos(angle - 0.55f) * head_size,
                tip.y - std::sin(angle - 0.55f) * head_size);
            const ImVec2 head_right(
                tip.x - std::cos(angle + 0.55f) * head_size,
                tip.y - std::sin(angle + 0.55f) * head_size);

            const ImU32 arrow_color = ImGui::GetColorU32(
                ImVec4(0.95f, 0.98f, 1.00f, 0.28f + strength * 0.62f));
            draw_list->AddLine(start, tip, arrow_color, 1.6f);
            draw_list->AddTriangleFilled(tip, head_left, head_right, arrow_color);
        }
    }
}
} // namespace

void DebugMenu_Initialize(ID3D11Device* device, ID3D11DeviceContext* context, HWND hwnd)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(device, context);

    ImGui::StyleColorsDark();
}

void DebugMenu_Finalize()
{
    SAFE_RELEASE(g_WindFieldPreviewStagingTexture);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void DebugMenu_SetFramePlan(const RenderFramePlan& frame_plan)
{
    g_FramePlan = frame_plan;
}

void DebugMenu_SetInstancingStats(const InstancingStats& instancing_stats)
{
    g_InstancingStats = instancing_stats;
}

void DebugMenu_SetFrustumCullingStats(const FrustumCullingStats& frustum_culling_stats)
{
    g_FrustumCullingStats = frustum_culling_stats;
}

void DebugMenu_SetGrassComputeStats(const GrassComputeStats& grass_compute_stats)
{
    g_GrassComputeStats = grass_compute_stats;
}

void DebugMenu_SetPerformanceStats(float fps, float frame_time_ms)
{
    g_DebugFps = fps;
    g_DebugFrameTimeMs = frame_time_ms;
}

void DebugMenu_SetShaderReloadStatus(bool succeeded, const char* message)
{
    g_ShaderReloadSucceeded = succeeded;
    g_ShaderReloadMessage = message != nullptr ? message : "";
}

bool DebugMenu_ConsumeShaderReloadRequest()
{
    const bool was_requested = g_ShaderReloadRequested;
    g_ShaderReloadRequested = false;
    return was_requested;
}

bool DebugMenu_ConsumeSurfaceWaterInjectionRequest()
{
    const bool was_requested = g_SurfaceWaterInjectionRequested;
    g_SurfaceWaterInjectionRequested = false;
    return was_requested;
}

bool DebugMenu_ConsumeSurfaceWaterResetRequest()
{
    const bool was_requested = g_SurfaceWaterResetRequested;
    g_SurfaceWaterResetRequested = false;
    return was_requested;
}

const ComputeNoiseSettings& DebugMenu_GetComputeNoiseSettings()
{
    return g_ComputeNoiseSettings;
}

const TerrainSettings& DebugMenu_GetTerrainSettings()
{
    return g_TerrainSettings;
}

const TerrainMaterialSettings& DebugMenu_GetTerrainMaterialSettings()
{
    return g_TerrainMaterialSettings;
}

const WaterSurfaceDesc& DebugMenu_GetWaterSurfaceSettings()
{
    return g_WaterSurfaceSettings;
}

const SurfaceWaterSimulationSettings& DebugMenu_GetSurfaceWaterSimulationSettings()
{
    return g_SurfaceWaterSimulationSettings;
}

const PostProcessSettings& DebugMenu_GetPostProcessSettings()
{
    return g_PostProcessSettings;
}

bool DebugMenu_IsTerrainSurfacePresentationEnabled()
{
    return g_EnableTerrainSurfacePresentation;
}

bool DebugMenu_IsGrassGpuEnabled()
{
    return g_EnableGrassGpu;
}

void DebugMenu_Begin()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void DebugMenu_Draw(const RenderFrameContext* frame_context)
{
    ImGui::Begin("Debug Menu");
    bool instancing_enabled = InstancingDebug_IsEnabled();
    if (ImGui::Checkbox("Instancing", &instancing_enabled))
    {
        InstancingDebug_SetEnabled(instancing_enabled);
    }
    bool frustum_culling_enabled = FrustumCullingDebug_IsEnabled();
    if (ImGui::Checkbox("Frustum Culling", &frustum_culling_enabled))
    {
        FrustumCullingDebug_SetEnabled(frustum_culling_enabled);
    }
    bool vsync_enabled = Direct3D_IsVSyncEnabled();
    if (ImGui::Checkbox("VSync", &vsync_enabled))
    {
        Direct3D_SetVSyncEnabled(vsync_enabled);
    }
    bool msaa_enabled = Direct3D_IsSceneMSAAEnabled();
    if (ImGui::Checkbox("MSAA (F8)", &msaa_enabled))
    {
        Direct3D_SetSceneMSAAEnabled(msaa_enabled);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", Direct3D_IsSceneMSAAEnabled() ? "4x" : "off");
    ImGui::Separator();
    ImGui::Text("FPS: %.1f", g_DebugFps);
    ImGui::Text("Frame Time: %.2f ms", g_DebugFrameTimeMs);
    if (ImGui::Button("Reload Shaders (F5)"))
    {
        g_ShaderReloadRequested = true;
    }
    const ImVec4 shader_status_color = g_ShaderReloadSucceeded
        ? ImVec4(0.45f, 0.95f, 0.55f, 1.0f)
        : ImVec4(1.0f, 0.45f, 0.45f, 1.0f);
    ImGui::TextColored(shader_status_color, "Shader Reload: %s", g_ShaderReloadMessage.c_str());
    ImGui::TextDisabled("Map editor and object placement controls are disabled in simulator mode.");
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Panels", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Portfolio", &g_DebugWindows.portfolio);
        ImGui::Checkbox("Compute Noise", &g_DebugWindows.compute_noise);
        ImGui::Checkbox("Wind", &g_DebugWindows.wind);
        ImGui::Checkbox("Atmosphere", &g_DebugWindows.atmosphere);
        ImGui::Checkbox("Water", &g_DebugWindows.water);
        ImGui::Checkbox("Terrain", &g_DebugWindows.terrain);
        ImGui::Checkbox("Post FX", &g_DebugWindows.post_fx);
        ImGui::Checkbox("Camera", &g_DebugWindows.camera);
        ImGui::Checkbox("Toon", &g_DebugWindows.toon);
        ImGui::Checkbox("Grass Compute", &g_DebugWindows.grass_compute);
        ImGui::Checkbox("Instancing", &g_DebugWindows.instancing);
        ImGui::Checkbox("Frustum Culling", &g_DebugWindows.frustum_culling);
        ImGui::Checkbox("Compute Registry", &g_DebugWindows.compute_registry);
        ImGui::Checkbox("Render Flow", &g_DebugWindows.render_flow);
    }
    ImGui::End();

    if (g_DebugWindows.portfolio)
    {
        const bool portfolio_open = ImGui::Begin("Portfolio", &g_DebugWindows.portfolio);
        if (portfolio_open)
        {
            ImGui::TextDisabled("Before / After showcase toggles");
            ImGui::Checkbox("Terrain Surface Shading", &g_EnableTerrainSurfacePresentation);
            ImGui::Checkbox("Grass GPU Instances", &g_EnableGrassGpu);
            ImGui::Separator();
            ImGui::TextWrapped("Use these to record clean portfolio comparisons without changing the underlying pipeline.");
        }
        ImGui::End();
    }

    if (g_DebugWindows.compute_noise)
    {
        const bool compute_noise_open = ImGui::Begin("Compute Noise", &g_DebugWindows.compute_noise);
        if (compute_noise_open)
        {
            if (ImGui::Button("Apply Afterglow Meteo Preset"))
            {
                ApplyAfterglowMeteorographPreset();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Bias the active weather settings toward Afterglow-style broad, stable flow.");
            ImGui::SliderFloat("Scale", &g_ComputeNoiseSettings.noise_scale, 4.0f, 96.0f);
            ImGui::Separator();
            ImGui::TextDisabled("Wind");
            ImGui::SliderFloat("Wind Dir X", &g_ComputeNoiseSettings.wind_direction_x, -1.0f, 1.0f);
            ImGui::SliderFloat("Wind Dir Y", &g_ComputeNoiseSettings.wind_direction_y, -1.0f, 1.0f);
            ImGui::SliderFloat("Wind Strength", &g_ComputeNoiseSettings.wind_strength, 0.0f, 0.20f);
            ImGui::SliderFloat("Cross Flow", &g_ComputeNoiseSettings.wind_cross_influence, 0.0f, 1.5f);
            ImGui::Separator();
            ImGui::TextDisabled("Detail Offsets");
            ImGui::SliderFloat("Flow X0", &g_ComputeNoiseSettings.flow_speed_x0, -0.20f, 0.20f);
            ImGui::SliderFloat("Flow Y0", &g_ComputeNoiseSettings.flow_speed_y0, -0.20f, 0.20f);
            ImGui::SliderFloat("Flow X1", &g_ComputeNoiseSettings.flow_speed_x1, -0.20f, 0.20f);
            ImGui::SliderFloat("Flow Y1", &g_ComputeNoiseSettings.flow_speed_y1, -0.20f, 0.20f);
            ImGui::SliderFloat("Band Strength", &g_ComputeNoiseSettings.band_strength, 0.0f, 0.35f);
            ImGui::SliderFloat("Contrast", &g_ComputeNoiseSettings.contrast, 0.35f, 2.25f);
            ImGui::Separator();
            ImGui::TextDisabled("Rain");
            ImGui::TextWrapped("Rainfall now follows Afterglow-style humidity overflow inside Meteorograph. Higher local humidity and converging airflow create brighter rain zones.");
        }
        ImGui::End();
    }

    if (g_DebugWindows.wind)
    {
        const bool wind_open = ImGui::Begin("Wind", &g_DebugWindows.wind);
        if (wind_open)
        {
            ImGui::TextDisabled("Global wind controls and Meteorograph preview");
            ImGui::SliderFloat("World Wind Strength", &g_ComputeNoiseSettings.wind_strength, 0.0f, 0.20f);
            ImGui::Text("Current: %.3f", g_ComputeNoiseSettings.wind_strength);
            ImGui::Checkbox("Accurate GPU Arrows", &g_UseAccurateWindPreviewArrows);
            ImGui::TextDisabled("Off = approximate CPU preview. On = read back the authoritative Meteorograph state at a limited rate.");
            if (frame_context == nullptr || !frame_context->resources.meteorograph_field.isValid())
            {
                ImGui::TextDisabled("Meteorograph Wind Preview: Missing");
            }
            else
            {
                ImGui::Separator();
                ImGui::TextDisabled("Meteorograph Wind Preview");
                ImGui::TextWrapped("Background = Afterglow-style climate composite: warm temperature base, humidity tint, and white rainfall highlights. Arrows = wind vectors decoded from Meteorograph.xy.");
                const ImVec2 preview_size(224.0f, 224.0f);
                const ImVec2 preview_top_left = ImGui::GetCursorScreenPos();
                ImGui::Image(
                    ImTextureRef(reinterpret_cast<ImTextureID>(
                        frame_context->resources.atmosphere_preview.isValid()
                            ? frame_context->resources.atmosphere_preview.shaderResourceView()
                            : frame_context->resources.meteorograph_field.shaderResourceView())),
                    preview_size);
                DrawWindFieldOverlay(
                    preview_top_left,
                    preview_size,
                    frame_context->resources.meteorograph_field.shaderResourceView(),
                    static_cast<float>(frame_context->globals.time_seconds),
                    g_ComputeNoiseSettings,
                    true);
            }
        }
        ImGui::End();
    }

    if (g_DebugWindows.atmosphere)
    {
        const bool atmosphere_open = ImGui::Begin("Atmosphere", &g_DebugWindows.atmosphere);
        if (atmosphere_open)
        {
            if (frame_context == nullptr)
            {
                ImGui::TextDisabled("No frame context");
            }
            else
            {
                const ImVec2 preview_size(224.0f, 224.0f);

                ImGui::TextDisabled("Atmosphere Preview");
                ImGui::TextWrapped("Preview follows an Afterglow-style climate composite: warm temperature base, humidity tint, and white rainfall highlights. Wind arrows are shown in the Wind panel.");
                ImGui::TextDisabled("Afterglow-like tuning now biases toward weaker random source, wrap-around transport, humidity capacity 1.0, and slower broad weather motion.");
                if (!frame_context->resources.atmosphere_preview.isValid())
                {
                    ImGui::TextDisabled("Atmosphere Preview: Missing");
                }
                else
                {
                    ImGui::Image(
                        ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.atmosphere_preview.shaderResourceView())),
                        preview_size);
                }
            }
        }
        ImGui::End();
    }

    if (g_DebugWindows.water)
    {
        const bool water_open = ImGui::Begin("Water", &g_DebugWindows.water);
        if (!water_open)
        {
            ImGui::End();
        }
        else
        {
    ImGui::SliderFloat("Water Height Override", &g_WaterSurfaceSettings.height, -1.0f, 32.0f);
    ImGui::Separator();
    ImGui::ColorEdit4("Base Color", reinterpret_cast<float*>(&g_WaterSurfaceSettings.base_color));
    ImGui::SliderFloat("Ripple Strength", &g_WaterSurfaceSettings.ripple_strength, 0.0f, 3.0f);
    ImGui::SliderFloat("Edge Emphasis", &g_WaterSurfaceSettings.edge_emphasis, 0.0f, 3.0f);
    ImGui::Separator();
    ImGui::TextDisabled("Surface Water Simulation");
    ImGui::SliderFloat("Rain Accumulation", &g_SurfaceWaterSimulationSettings.accumulation_rate, 0.01f, 0.18f);
    ImGui::SliderFloat("Evaporation", &g_SurfaceWaterSimulationSettings.evaporation_rate, 0.0f, 0.02f);
    ImGui::SliderFloat("Seepage", &g_SurfaceWaterSimulationSettings.seepage_rate, 0.0f, 0.02f);
    ImGui::SliderFloat("Basin Fade", &g_SurfaceWaterSimulationSettings.basin_fade, 2.0f, 12.0f);
    ImGui::SliderFloat("Downhill Flow", &g_SurfaceWaterSimulationSettings.downhill_flow_rate, 0.05f, 1.60f);
    ImGui::SliderFloat("Max Outflow", &g_SurfaceWaterSimulationSettings.max_outflow_fraction, 0.10f, 0.95f);
    ImGui::Separator();
    ImGui::TextDisabled("One-shot Water Injection Test");
    ImGui::SliderFloat(
        "Drop X",
        &g_SurfaceWaterSimulationSettings.debug_injection_x,
        -ComputeTextureDimensions::kWorldHalfExtent,
        ComputeTextureDimensions::kWorldHalfExtent);
    ImGui::SliderFloat(
        "Drop Z",
        &g_SurfaceWaterSimulationSettings.debug_injection_z,
        -ComputeTextureDimensions::kWorldHalfExtent,
        ComputeTextureDimensions::kWorldHalfExtent);
    ImGui::SliderFloat("Drop Radius", &g_SurfaceWaterSimulationSettings.debug_injection_radius, 2.0f, 36.0f);
    ImGui::SliderFloat("Drop Amount", &g_SurfaceWaterSimulationSettings.debug_injection_amount, 0.05f, 2.50f);
    if (ImGui::Button("Use Hero Peak"))
    {
        g_SurfaceWaterSimulationSettings.debug_injection_x = 46.0f;
        g_SurfaceWaterSimulationSettings.debug_injection_z = 118.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Inject Water Pulse"))
    {
        g_SurfaceWaterInjectionRequested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Surface Water"))
    {
        g_SurfaceWaterResetRequested = true;
    }
    if (ImGui::Button("Isolated Peak Test"))
    {
        g_SurfaceWaterResetRequested = true;
        g_SurfaceWaterInjectionRequested = true;
        g_SurfaceWaterSimulationSettings.debug_injection_x = 46.0f;
        g_SurfaceWaterSimulationSettings.debug_injection_z = 118.0f;
    }
    ImGui::TextDisabled("Use this to prove downhill flow: drop water on a high ridge and watch WaterSurfaceHeight plus the downstream water fields react.");
    ImGui::Separator();
    ImGui::TextDisabled("Height <= 0 uses climate/rain-driven automatic basin water.");
    ImGui::TextDisabled("Shared Compute Inputs");
    if (frame_context == nullptr)
    {
        ImGui::TextDisabled("No frame context");
    }
    else
    {
        ImGui::Text(
            "BaseTerrainHeight: %s",
            frame_context->resources.base_terrain_height.isValid() ? "Active" : "Missing");
        ImGui::Text("TerrainHeight: %s", frame_context->resources.terrain_height.isValid() ? "Active" : "Missing");
        ImGui::Text("TerrainNormal: %s", frame_context->resources.terrain_normal.isValid() ? "Active" : "Missing");
        ImGui::Text("TerrainSurfaceData: %s", frame_context->resources.terrain_surface_data.isValid() ? "Active" : "Missing");
        ImGui::Text(
            "TerrainVegetationSuitability: %s",
            frame_context->resources.terrain_vegetation_suitability.isValid() ? "Active" : "Missing");
        ImGui::Text("GrassData: %s", frame_context->resources.grass_data.isValid() ? "Active" : "Missing");
        ImGui::Text("WaterSurfaceHeight: %s", frame_context->resources.water_surface_height.isValid() ? "Active" : "Missing");
        if (frame_context->resources.has_terrain_heightfield_range)
        {
            ImGui::Text(
                "  TerrainHeight.xy.R Range: %.2f .. %.2f",
                frame_context->resources.terrain_heightfield_min_height,
                frame_context->resources.terrain_heightfield_max_height);
            if (frame_context->resources.terrain_heightfield_range_is_fallback)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(fallback)");
            }
        }
        else
        {
            ImGui::TextDisabled("  TerrainHeight.xy.R Range: unavailable");
        }
        if (frame_context->resources.has_water_heightfield_range)
        {
            ImGui::Text(
                "  TerrainHeight.xy.G Range: %.2f .. %.2f",
                frame_context->resources.water_heightfield_min_height,
                frame_context->resources.water_heightfield_max_height);
            if (frame_context->resources.water_heightfield_range_is_fallback)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(fallback)");
            }
        }
        else
        {
            ImGui::TextDisabled("  TerrainHeight.xy.G Range: unavailable");
        }
        ImGui::Checkbox("Show Resource Previews", &g_ShowComputeResourcePreviews);

        if (g_ShowComputeResourcePreviews && frame_context->resources.base_terrain_height.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("BaseTerrainHeight Preview");
            ImGui::TextWrapped("Greyscale = procedural terrain height before it is packed into the shared TerrainHeight field.");
            if (frame_context->resources.has_base_terrain_range)
            {
                ImGui::Text(
                    "Height Range: %.2f .. %.2f",
                    frame_context->resources.base_terrain_min_height,
                    frame_context->resources.base_terrain_max_height);
            }
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.base_terrain_height.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }

        if (g_ShowComputeResourcePreviews && frame_context->resources.terrain_height.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("Active TerrainHeight Preview");
            ImGui::TextWrapped("Greyscale = terrain height currently used by terrain consumers. In shared mode this should follow TerrainHeight.x directly.");
            if (frame_context->resources.has_active_terrain_range)
            {
                ImGui::Text(
                    "Height Range: %.2f .. %.2f",
                    frame_context->resources.active_terrain_min_height,
                    frame_context->resources.active_terrain_max_height);
            }
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.terrain_height.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }

        if (g_ShowComputeResourcePreviews && frame_context->resources.terrain_normal.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("TerrainNormal Preview");
            ImGui::TextWrapped("RGB = world normal, A = normal.y slope helper. This is the shared terrain-normal field used by terrain and grass.");
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.terrain_normal.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }

        if (g_ShowComputeResourcePreviews && frame_context->resources.water_surface_height.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("WaterSurfaceHeight Preview");
            ImGui::TextWrapped("R = terrain height, G = water height. Terrain render ultimately consumes R, while the water vertex path only reads G, matching the shared TerrainHeight.xy layout.");
            if (frame_context->resources.has_terrain_heightfield_range)
            {
                ImGui::Text(
                    "Terrain Range: %.2f .. %.2f",
                    frame_context->resources.terrain_heightfield_min_height,
                    frame_context->resources.terrain_heightfield_max_height);
                if (frame_context->resources.terrain_heightfield_range_is_fallback)
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(fallback)");
                }
            }
            if (frame_context->resources.has_water_heightfield_range)
            {
                ImGui::Text(
                    "Water Range: %.2f .. %.2f",
                    frame_context->resources.water_heightfield_min_height,
                    frame_context->resources.water_heightfield_max_height);
                if (frame_context->resources.water_heightfield_range_is_fallback)
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(fallback)");
                }
            }
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.water_surface_height.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }

        if (g_ShowComputeResourcePreviews && frame_context->resources.terrain_surface_data.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("TerrainSurfaceData Preview");
            ImGui::TextWrapped("R = slope, G = beach / shoreline, B = humidity, A = roughness. This is the core terrain surface field.");
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.terrain_surface_data.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }

        if (g_ShowComputeResourcePreviews && frame_context->resources.terrain_vegetation_suitability.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("VegetationSuitability Preview");
            ImGui::TextWrapped("White = grass-friendly terrain. This is the dedicated vegetation signal used by grass seeding.");
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.terrain_vegetation_suitability.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }

        if (g_ShowComputeResourcePreviews && frame_context->resources.grass_data.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("GrassData Preview");
            ImGui::TextWrapped("R = grass possibility, G = stable distribution hash, B = scale hint. This is the consumer-facing grass field.");
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.grass_data.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }
    }
        ImGui::End();
        }
    }

    if (g_DebugWindows.post_fx)
    {
        const bool post_fx_open = ImGui::Begin("Post FX", &g_DebugWindows.post_fx);
        if (post_fx_open)
        {
    ImGui::TextDisabled("Current pass: Chromatic Aberration / Film Grain / Vignette / Bloom / Height Fog");
    ImGui::SliderFloat("Chromatic Aberration", &g_PostProcessSettings.chromatic_aberration, 0.0f, 0.016f);
    ImGui::SliderFloat("Film Grain", &g_PostProcessSettings.film_grain, 0.0f, 0.120f);
    ImGui::SliderFloat("Film Grain Speed", &g_PostProcessSettings.film_grain_speed, 0.0f, 4.0f);
    ImGui::SliderFloat("Film Grain Debug Boost", &g_PostProcessSettings.film_grain_debug_boost, 1.0f, 8.0f);
    ImGui::SliderFloat("Atmospheric Dust", &g_PostProcessSettings.atmospheric_dust, 0.0f, 0.120f);
    ImGui::SliderFloat("Vignette Intensity", &g_PostProcessSettings.vignette_intensity, 0.0f, 0.60f);
    ImGui::SliderFloat("Vignette Softness", &g_PostProcessSettings.vignette_softness, 0.10f, 0.90f);
    ImGui::Separator();
    ImGui::TextDisabled("Bloom");
    ImGui::SliderFloat("Bloom Intensity", &g_PostProcessSettings.bloom_intensity, 0.0f, 1.20f);
    ImGui::SliderFloat("Bloom Threshold", &g_PostProcessSettings.bloom_threshold, 0.40f, 1.00f);
    ImGui::SliderFloat("Bloom Soft Knee", &g_PostProcessSettings.bloom_soft_knee, 0.02f, 0.45f);
    ImGui::SliderFloat("Bloom Blur Scale", &g_PostProcessSettings.bloom_blur_scale, 0.50f, 3.50f);
    ImGui::Separator();
    ImGui::TextDisabled("Tone Mapping");
    ImGui::SliderFloat("Exposure EV", &g_PostProcessSettings.exposure_ev, -4.0f, 4.0f);
    static const char* toneMapModes[] = {
        "Off",
        "Reinhard",
        "ACES",
        "Filmic"
    };
    int tone_map_mode = static_cast<int>(g_PostProcessSettings.tone_map_mode + 0.5f);
    tone_map_mode = std::clamp(tone_map_mode, 0, static_cast<int>(std::size(toneMapModes)) - 1);
    if (ImGui::Combo("Tone Map", &tone_map_mode, toneMapModes, static_cast<int>(std::size(toneMapModes))))
    {
        g_PostProcessSettings.tone_map_mode = static_cast<float>(tone_map_mode);
    }
    ImGui::SliderFloat("Output Gamma", &g_PostProcessSettings.output_gamma, 1.0f, 2.6f);
    ImGui::SliderFloat("Output Gain", &g_PostProcessSettings.output_gain, 0.25f, 2.0f);
    ImGui::Separator();
    ImGui::TextDisabled("Height Fog");
    ImGui::SliderFloat("Fog Intensity", &g_PostProcessSettings.fog_intensity, 0.0f, 0.90f);
    ImGui::SliderFloat("Fog Distance Fade", &g_PostProcessSettings.fog_distance_fade, 0.0f, 0.80f);
    ImGui::SliderFloat("Fog Height Bias", &g_PostProcessSettings.fog_height_bias, -24.0f, 24.0f);
    ImGui::SliderFloat("Fog Env Mix", &g_PostProcessSettings.fog_env_mix, 0.0f, 1.0f);
    ImGui::Separator();
    ImGui::TextDisabled("Image-Based Scattering");
    ImGui::SliderFloat("Fog Scatter Strength", &g_PostProcessSettings.fog_scatter_strength, 0.0f, 1.0f);
    ImGui::SliderFloat("Fog Scatter Focus", &g_PostProcessSettings.fog_scatter_focus, 0.0f, 1.0f);
    ImGui::TextDisabled("Scattering v1 uses sky tint + forward light direction to color the fog, not full volumetrics.");
        }
        ImGui::End();
    }

    if (g_DebugWindows.terrain)
    {
        const bool terrain_open = ImGui::Begin("Terrain", &g_DebugWindows.terrain);
        if (terrain_open)
        {
    if (!g_TerrainAuthoringControlsInitialized)
    {
        SyncTerrainAuthoringControlsFromSettings(g_TerrainSettings, g_TerrainAuthoringControls);
        g_TerrainAuthoringControlsInitialized = true;
    }

    ImGui::TextDisabled("Shape");
    bool terrain_shape_changed = false;
    terrain_shape_changed |= ImGui::SliderFloat("Plains Coverage", &g_TerrainAuthoringControls.plains_coverage, 0.0f, 1.0f);
    terrain_shape_changed |= ImGui::SliderFloat("Peak Density", &g_TerrainAuthoringControls.peak_density, 0.0f, 1.0f);
    terrain_shape_changed |= ImGui::SliderFloat("Peak Height", &g_TerrainAuthoringControls.peak_height, 0.0f, 1.0f);
    terrain_shape_changed |= ImGui::SliderFloat("Lake Amount", &g_TerrainAuthoringControls.lake_amount, 0.0f, 1.0f);
    terrain_shape_changed |= ImGui::SliderFloat("Lake Size", &g_TerrainAuthoringControls.lake_size, 0.0f, 1.0f);
    if (terrain_shape_changed)
    {
        const float preserved_lake_center_z = g_TerrainSettings.lake_center_z;
        ApplyTerrainAuthoringControlsToSettings(g_TerrainAuthoringControls, g_TerrainSettings);
        g_TerrainSettings.lake_center_z = preserved_lake_center_z;
    }

    ImGui::Separator();
    if (ImGui::TreeNode("Advanced Terrain Settings"))
    {
        bool terrain_advanced_changed = false;
        terrain_advanced_changed |= ImGui::SliderFloat("Base Frequency", &g_TerrainSettings.base_frequency, 0.0015f, 0.0200f, "%.4f");
        terrain_advanced_changed |= ImGui::SliderFloat("Base Height", &g_TerrainSettings.base_height, 4.0f, 36.0f);
        terrain_advanced_changed |= ImGui::SliderFloat("Detail Frequency", &g_TerrainSettings.detail_frequency, 1.0f, 8.0f);
        terrain_advanced_changed |= ImGui::SliderFloat("Detail Height", &g_TerrainSettings.detail_height, 0.0f, 12.0f);
        terrain_advanced_changed |= ImGui::SliderFloat("Ridge Frequency", &g_TerrainSettings.ridge_frequency, 0.5f, 4.0f);
        terrain_advanced_changed |= ImGui::SliderFloat("Ridge Height", &g_TerrainSettings.ridge_height, 0.0f, 28.0f);
        terrain_advanced_changed |= ImGui::SliderFloat("Continent Height", &g_TerrainSettings.continent_height, -2.0f, 10.0f);
        ImGui::Separator();
        terrain_advanced_changed |= ImGui::SliderFloat("Lake Center Z", &g_TerrainSettings.lake_center_z, -40.0f, 60.0f);
        terrain_advanced_changed |= ImGui::SliderFloat("Lake Radius X", &g_TerrainSettings.lake_radius_x, 8.0f, 80.0f);
        terrain_advanced_changed |= ImGui::SliderFloat("Lake Radius Z", &g_TerrainSettings.lake_radius_z, 8.0f, 80.0f);
        terrain_advanced_changed |= ImGui::SliderFloat("Lake Depth", &g_TerrainSettings.lake_depth, 0.0f, 20.0f);
        if (terrain_advanced_changed)
        {
            SyncTerrainAuthoringControlsFromSettings(g_TerrainSettings, g_TerrainAuthoringControls);
        }
        ImGui::TreePop();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Material Blend");
    ImGui::SliderFloat("Grass Slope Min", &g_TerrainMaterialSettings.grass_slope_min, 0.10f, 0.95f);
    ImGui::SliderFloat("Grass Slope Max", &g_TerrainMaterialSettings.grass_slope_max, 0.10f, 1.00f);
    ImGui::SliderFloat("Grass Noise", &g_TerrainMaterialSettings.grass_noise_strength, 0.0f, 0.40f);
    ImGui::SliderFloat("Grass Height Start", &g_TerrainMaterialSettings.grass_height_start, -4.0f, 24.0f);
    ImGui::SliderFloat("Grass Height End", &g_TerrainMaterialSettings.grass_height_end, -2.0f, 32.0f);
    ImGui::SliderFloat("Grass Coverage Min", &g_TerrainMaterialSettings.grass_coverage_min, 0.05f, 0.95f);
    ImGui::SliderFloat("Stone Slope Start", &g_TerrainMaterialSettings.rock_slope_start, 0.0f, 1.0f);
    ImGui::SliderFloat("Stone Slope End", &g_TerrainMaterialSettings.rock_slope_end, 0.0f, 1.0f);
    ImGui::SliderFloat("Snow Height Start", &g_TerrainMaterialSettings.rock_height_start, -4.0f, 28.0f);
    ImGui::SliderFloat("Snow Height End", &g_TerrainMaterialSettings.rock_height_end, -2.0f, 36.0f);
    ImGui::SliderFloat("Stone Noise Scale", &g_TerrainMaterialSettings.stone_noise_scale, 0.005f, 0.150f, "%.3f");
    ImGui::SliderFloat("Shore Offset Start", &g_TerrainMaterialSettings.shoreline_offset_start, 0.0f, 4.0f);
    ImGui::SliderFloat("Shore Offset End", &g_TerrainMaterialSettings.shoreline_offset_end, 1.0f, 10.0f);
    ImGui::SliderFloat("Lowland Start", &g_TerrainMaterialSettings.lowland_height_start, -8.0f, 48.0f);
    ImGui::SliderFloat("Lowland End", &g_TerrainMaterialSettings.lowland_height_end, 0.0f, 80.0f);
    ImGui::Separator();
    ImGui::TextDisabled("PBR Preview");
    ImGui::SliderFloat("PBR Roughness Bias", &g_TerrainMaterialSettings.pbr_roughness_bias, -0.45f, 0.45f);
    ImGui::SliderFloat("PBR Specular Scale", &g_TerrainMaterialSettings.pbr_specular_scale, 0.0f, 4.0f);
    ImGui::SliderFloat("PBR Detail Normal", &g_TerrainMaterialSettings.pbr_detail_normal_strength, 0.0f, 1.5f);
    ImGui::SliderFloat("PBR Light Intensity", &g_TerrainMaterialSettings.pbr_light_intensity, 0.25f, 3.0f);
    ImGui::SliderFloat("PBR Metallic", &g_TerrainMaterialSettings.pbr_metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("PBR AO Strength", &g_TerrainMaterialSettings.pbr_ao_strength, 0.0f, 1.0f);
    static const char* pbrDebugModes[] = {
        "Shaded",
        "Albedo",
        "Metallic",
        "Roughness",
        "Normal",
        "AO",
        "Fresnel"
    };
    int pbr_debug_mode = static_cast<int>(g_TerrainMaterialSettings.pbr_debug_mode + 0.5f);
    pbr_debug_mode = std::clamp(pbr_debug_mode, 0, static_cast<int>(std::size(pbrDebugModes)) - 1);
    if (ImGui::Combo("PBR Debug View", &pbr_debug_mode, pbrDebugModes, static_cast<int>(std::size(pbrDebugModes))))
    {
        g_TerrainMaterialSettings.pbr_debug_mode = static_cast<float>(pbr_debug_mode);
    }
        }
        ImGui::End();
    }

    if (g_DebugWindows.camera)
    {
        const bool camera_open = ImGui::Begin("Camera", &g_DebugWindows.camera);
        if (camera_open)
        {
            if (ImGui::CollapsingHeader("Camera"))
            {
                extern float g_CameraMoveSpeed;
                ImGui::SliderFloat("Camera Speed", &g_CameraMoveSpeed, 1.0f, 20.0f);
            }
        }
        ImGui::End();
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2(200, 300), ImVec2(500, 800));
    if (g_DebugWindows.toon)
    {
        const bool toon_open = ImGui::Begin("Toon", &g_DebugWindows.toon);
        if (toon_open)
        {

    if (ImGui::CollapsingHeader("Toon Lighting Debug"))
    {
        static float azimuth = 45.0f;
        static float elevation = 45.0f;
        static float dirColor[3] = { 1.0f, 1.0f, 1.0f };
        static float dirIntensity = 1.0f;
        static float ambColor[3] = { 0.3f, 0.3f, 0.3f };
        static float specColor[3] = { 1.0f, 1.0f, 1.0f };
        static float specPower = 20.0f;

        bool changed = false;

        ImGui::Text("Directional Light (b2)");
        changed |= ImGui::SliderFloat("Azimuth", &azimuth, 0.0f, 360.0f);
        changed |= ImGui::SliderFloat("Elevation", &elevation, -90.0f, 90.0f);
        changed |= ImGui::ColorEdit3("Light Color", dirColor);
        changed |= ImGui::SliderFloat("Light Intensity", &dirIntensity, 0.0f, 5.0f);

        ImGui::Separator();
        ImGui::Text("Ambient Light (b1)");
        if (ImGui::ColorEdit3("Ambient Color", ambColor))
        {
            Light_SetAmbient(XMFLOAT3(ambColor[0], ambColor[1], ambColor[2]));
        }

        ImGui::Separator();
        ImGui::Text("Specular Light (b3)");
        changed |= ImGui::ColorEdit3("Spec Color", specColor);
        changed |= ImGui::SliderFloat("Spec Power", &specPower, 1.0f, 100.0f);

        if (changed)
        {
            const float phi = XMConvertToRadians(azimuth);
            const float theta = XMConvertToRadians(elevation);
            const XMVECTOR to_sun =
                XMVectorSet(cosf(theta) * sinf(phi), sinf(theta), cosf(theta) * cosf(phi), 0.0f);

            XMFLOAT4 final_dir;
            XMStoreFloat4(&final_dir, -to_sun);
            const XMFLOAT4 final_dir_col = {
                dirColor[0] * dirIntensity,
                dirColor[1] * dirIntensity,
                dirColor[2] * dirIntensity,
                1.0f };
            Light_SetDirectionalWorld(final_dir, final_dir_col);

            const XMFLOAT3 cam_pos = Camera_GetPosition();
            Light_SetSpecularWorld(
                cam_pos,
                specPower,
                XMFLOAT4(specColor[0], specColor[1], specColor[2], 1.0f));
        }
    }

    if (ImGui::CollapsingHeader("Toon Shader"))
    {
        extern int g_ToonStepCount;
        ImGui::SliderInt("Step Count", &g_ToonStepCount, 1, 30);
    }
        }
        ImGui::End();
    }

    if (g_DebugWindows.compute_registry)
    {
        const bool compute_registry_open = ImGui::Begin("Compute Registry", &g_DebugWindows.compute_registry);
        if (compute_registry_open)
        {
            if (frame_context == nullptr || frame_context->resources.compute_shared_registry == nullptr)
            {
                ImGui::TextDisabled("No registry bound");
            }
            else
            {
                const ComputeSharedResourceRegistry& registry = *frame_context->resources.compute_shared_registry;
                for (unsigned int i = 0; i < static_cast<unsigned int>(ComputeSharedResourceId::Count); ++i)
                {
                    const auto id = static_cast<ComputeSharedResourceId>(i);
                    const ComputeSharedResourceEntry& entry = registry.Get(id);
                    if (entry.kind == ComputeSharedResourceKind::None)
                    {
                        continue;
                    }

                    if (ImGui::TreeNode(ToString(id)))
                    {
                        ImGui::Text("Debug Name: %s", entry.debug_name != nullptr ? entry.debug_name : "");
                        ImGui::Text("Kind: %s", ToString(entry.kind));
                        ImGui::Text("SRV: %s", entry.srv != nullptr ? "Yes" : "No");
                        ImGui::Text("UAV: %s", entry.uav != nullptr ? "Yes" : "No");
                        ImGui::Text("Buffer: %s", entry.buffer != nullptr ? "Yes" : "No");
                        if (entry.element_count > 0)
                        {
                            ImGui::Text("Elements: %u", entry.element_count);
                        }
                        if (entry.stride > 0)
                        {
                            ImGui::Text("Stride: %u", entry.stride);
                        }
                        ImGui::TreePop();
                    }
                }
            }
        }
        ImGui::End();
    }

    if (g_DebugWindows.instancing)
    {
        const bool instancing_open = ImGui::Begin("Instancing", &g_DebugWindows.instancing);
        if (instancing_open)
        {
            ImGui::Text("Enabled: %s", InstancingDebug_IsEnabled() ? "Yes" : "No");
            ImGui::Text("Batches: %d", g_InstancingStats.instanced_batch_count);
            ImGui::Text("Instances: %d", g_InstancingStats.instanced_instance_count);
            ImGui::Text("Saved Draws: %d", g_InstancingStats.estimated_draw_calls_saved);
        }
        ImGui::End();
    }

    if (g_DebugWindows.frustum_culling)
    {
        const bool frustum_culling_open = ImGui::Begin("Frustum Culling", &g_DebugWindows.frustum_culling);
        if (frustum_culling_open)
        {
            ImGui::Text("Enabled: %s", FrustumCullingDebug_IsEnabled() ? "Yes" : "No");
            ImGui::Text("Tested: %d", g_FrustumCullingStats.tested_objects);
            ImGui::Text("Visible: %d", g_FrustumCullingStats.visible_objects);
            ImGui::Text("Culled: %d", g_FrustumCullingStats.culled_objects);
        }
        ImGui::End();
    }

    if (g_DebugWindows.grass_compute)
    {
        const bool grass_compute_open = ImGui::Begin("Grass Compute", &g_DebugWindows.grass_compute);
        if (grass_compute_open)
        {
            ImGui::Text("GPU Ready: %s", g_GrassComputeStats.gpu_ready ? "Yes" : "No");
            ImGui::Text("GPU Path Used: %s", g_GrassComputeStats.gpu_path_used ? "Yes" : "No");
            ImGui::Text("Fallback Used: %s", g_GrassComputeStats.fallback_used ? "Yes" : "No");
            ImGui::Text("Coverage Dirty: %s", g_GrassComputeStats.coverage_dirty ? "Yes" : "No");
            ImGui::Separator();
            ImGui::Text("Coverage Grid: %u x %u", g_GrassComputeStats.grid_cols, g_GrassComputeStats.grid_rows);
            ImGui::Text("Candidate Seeds: %u", g_GrassComputeStats.seed_count);
            ImGui::Text("Max Quads: %u", g_GrassComputeStats.max_instance_capacity);
            ImGui::Text("Visible Quads: %u", g_GrassComputeStats.visible_instance_count);
            if (!g_GrassComputeStats.last_error.empty())
            {
                ImGui::Separator();
                ImGui::TextWrapped("Status: %s", g_GrassComputeStats.last_error.c_str());
            }
        }
        ImGui::End();
    }

    if (g_DebugWindows.render_flow)
    {
        const bool render_flow_open = ImGui::Begin("Render Flow", &g_DebugWindows.render_flow);
        if (render_flow_open)
        {
            ImGui::TextDisabled("Pass order and resource flow");
            ImGui::Separator();

            for (size_t pass_index = 0; pass_index < g_FramePlan.passes.size(); ++pass_index)
            {
                const auto& pass_plan = g_FramePlan.passes[pass_index];
                if (ImGui::TreeNode(pass_plan.pass_name.data(), "%zu. %s", pass_index + 1, pass_plan.pass_name.data()))
                {
                    if (pass_plan.resources.empty())
                    {
                        ImGui::TextDisabled("No explicit resources");
                    }

                    for (const auto& usage : pass_plan.resources)
                    {
                        ImGui::Bullet();
                        ImGui::SameLine();
                        DrawResourceUsageChip(usage);
                    }
                    ImGui::TreePop();
                }

                if (pass_index + 1 < g_FramePlan.passes.size())
                {
                    ImGui::Indent(12.0f);
                    ImGui::TextDisabled("|");
                    ImGui::TextDisabled("v");
                    ImGui::Unindent(12.0f);
                }
            }
        }
        ImGui::End();
    }
}

void DebugMenu_End()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
