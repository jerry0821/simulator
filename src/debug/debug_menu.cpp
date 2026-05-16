// ----------------------------------------------------
// debug gui [debug_gui.h]
// ====================================================
// Created by: Jerry
// Date: 2025-12-11
// ----------------------------------------------------
#include "debug_menu.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "camera.h"
#include "compute_noise_texture.h"
#include "compute_shared_resource_registry.h"
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
static bool g_EnableWaterSurfaceDeformation = true;
static ComputeNoiseSettings g_ComputeNoiseSettings{};
static TerrainSettings g_TerrainSettings = MeshFieldRenderer::GetTerrainSettings();
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
constexpr float kWindPreviewWorldMinX = -640.0f;
constexpr float kWindPreviewWorldMaxX = 640.0f;
constexpr float kWindPreviewWorldMinZ = -640.0f;
constexpr float kWindPreviewWorldMaxZ = 640.0f;
constexpr int kWindPreviewCols = 18;
constexpr int kWindPreviewRows = 18;

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
    case ComputeSharedResourceId::SurfaceWater:
        return "SurfaceWater";
    case ComputeSharedResourceId::WaterSurfaceHeight:
        return "WaterSurfaceHeight";
    case ComputeSharedResourceId::SurfaceWaterFlow:
        return "SurfaceWaterFlow";
    case ComputeSharedResourceId::SurfaceWaterFlowPreview:
        return "SurfaceWaterFlowPreview";
    case ComputeSharedResourceId::WaterVelocity:
        return "WaterVelocity";
    case ComputeSharedResourceId::WaterSediment:
        return "WaterSediment";
    case ComputeSharedResourceId::VisibleWater:
        return "VisibleWater";
    case ComputeSharedResourceId::WaterMask:
        return "WaterMask";
    case ComputeSharedResourceId::WaterInteractionData:
        return "WaterInteractionData";
    case ComputeSharedResourceId::SoilMoisture:
        return "SoilMoisture";
    case ComputeSharedResourceId::ErosionDelta:
        return "ErosionDelta";
    case ComputeSharedResourceId::ComputeNoise:
        return "ComputeNoise";
    case ComputeSharedResourceId::WindField:
        return "WindField";
    case ComputeSharedResourceId::ClimateField:
        return "ClimateField";
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
        float dir_x = pixel_ptr[0] * 2.0f - 1.0f;
        float dir_y = pixel_ptr[1] * 2.0f - 1.0f;
        const float strength = pixel_ptr[2];
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
    float time_seconds)
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
    const ComputeNoiseSettings& settings)
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
        time_seconds);

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

bool DebugMenu_IsWaterSurfaceDeformationEnabled()
{
    return g_EnableWaterSurfaceDeformation;
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
    ImGui::End();

    ImGui::Begin("Portfolio");
    ImGui::TextDisabled("Before / After showcase toggles");
    ImGui::Checkbox("Terrain Surface Shading", &g_EnableTerrainSurfacePresentation);
    ImGui::Checkbox("Grass GPU Instances", &g_EnableGrassGpu);
    ImGui::Checkbox("Water Surface Deformation", &g_EnableWaterSurfaceDeformation);
    ImGui::Separator();
    ImGui::TextWrapped("Use these to record clean portfolio comparisons without changing the underlying pipeline.");
    ImGui::End();

    ImGui::Begin("Compute Noise");
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
    ImGui::TextDisabled("Rain Debug");
    ImGui::SliderFloat("Rain Multiplier", &g_ComputeNoiseSettings.rain_multiplier, 0.0f, 4.0f);
    ImGui::SliderFloat("Force Rain", &g_ComputeNoiseSettings.force_rain, 0.0f, 1.0f);
    ImGui::End();

    ImGui::Begin("Wind");
    ImGui::TextDisabled("Global wind controls");
    ImGui::SliderFloat("World Wind Strength", &g_ComputeNoiseSettings.wind_strength, 0.0f, 0.20f);
    ImGui::Text("Current: %.3f", g_ComputeNoiseSettings.wind_strength);
    ImGui::Checkbox("Accurate GPU Arrows", &g_UseAccurateWindPreviewArrows);
    ImGui::TextDisabled("Off = faster CPU preview. On = read back the GPU wind texture at a limited rate.");
    if (frame_context == nullptr || !frame_context->resources.wind_field.isValid())
    {
        ImGui::TextDisabled("WindField Preview: Missing");
    }
    else
    {
        ImGui::Separator();
        ImGui::TextDisabled("WindField Preview");
        ImGui::TextWrapped("Background = GPU wind field texture. Arrows = sampled flow direction and strength used by the scene.");
        const ImVec2 preview_size(224.0f, 224.0f);
        const ImVec2 preview_top_left = ImGui::GetCursorScreenPos();
        ImGui::Image(
            ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.wind_field.shaderResourceView())),
            preview_size);
        DrawWindFieldOverlay(
            preview_top_left,
            preview_size,
            frame_context->resources.wind_field.shaderResourceView(),
            static_cast<float>(frame_context->globals.time_seconds),
            g_ComputeNoiseSettings);
    }
    ImGui::End();

    ImGui::Begin("Water");
    ImGui::SliderFloat("Water Center X", &g_WaterSurfaceSettings.center_x, -80.0f, 80.0f);
    ImGui::SliderFloat("Water Center Z", &g_WaterSurfaceSettings.center_z, -80.0f, 80.0f);
    ImGui::SliderFloat("Water Height Override", &g_WaterSurfaceSettings.height, -1.0f, 32.0f);
    ImGui::SliderFloat("Water Size X", &g_WaterSurfaceSettings.size_x, 4.0f, 120.0f);
    ImGui::SliderFloat("Water Size Z", &g_WaterSurfaceSettings.size_z, 4.0f, 120.0f);
    ImGui::Separator();
    ImGui::ColorEdit4("Base Color", reinterpret_cast<float*>(&g_WaterSurfaceSettings.base_color));
    ImGui::ColorEdit4("Ripple Color", reinterpret_cast<float*>(&g_WaterSurfaceSettings.ripple_color));
    ImGui::ColorEdit4("Highlight Color", reinterpret_cast<float*>(&g_WaterSurfaceSettings.highlight_color));
    ImGui::SliderFloat("Ripple Strength", &g_WaterSurfaceSettings.ripple_strength, 0.0f, 3.0f);
    ImGui::SliderFloat("Wind Influence", &g_WaterSurfaceSettings.wind_influence, 0.0f, 3.0f);
    ImGui::SliderFloat("Edge Emphasis", &g_WaterSurfaceSettings.edge_emphasis, 0.0f, 3.0f);
    ImGui::Separator();
    ImGui::TextDisabled("Surface Water Simulation");
    ImGui::SliderFloat("Rain Accumulation", &g_SurfaceWaterSimulationSettings.accumulation_rate, 0.01f, 0.18f);
    ImGui::SliderFloat("Evaporation", &g_SurfaceWaterSimulationSettings.evaporation_rate, 0.0f, 0.02f);
    ImGui::SliderFloat("Seepage", &g_SurfaceWaterSimulationSettings.seepage_rate, 0.0f, 0.02f);
    ImGui::SliderFloat("Basin Fade", &g_SurfaceWaterSimulationSettings.basin_fade, 2.0f, 12.0f);
    ImGui::SliderFloat("Downhill Flow", &g_SurfaceWaterSimulationSettings.downhill_flow_rate, 0.05f, 0.60f);
    ImGui::SliderFloat("Flow Damping", &g_SurfaceWaterSimulationSettings.flow_damping, 0.25f, 1.00f);
    ImGui::SliderFloat("Max Outflow", &g_SurfaceWaterSimulationSettings.max_outflow_fraction, 0.10f, 0.95f);
    ImGui::Separator();
    ImGui::TextDisabled("One-shot Water Injection Test");
    ImGui::SliderFloat("Drop X", &g_SurfaceWaterSimulationSettings.debug_injection_x, -256.0f, 256.0f);
    ImGui::SliderFloat("Drop Z", &g_SurfaceWaterSimulationSettings.debug_injection_z, -256.0f, 256.0f);
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
    ImGui::TextDisabled("Use this to prove downhill flow: drop water on a high ridge and watch SurfaceWater / Flow / WaterMask.");
    ImGui::TextDisabled("Current world water render is still a flat stable plane, so peak injection is most obvious in the previews.");
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
        ImGui::Text("RainMap: %s", frame_context->resources.rain_map.isValid() ? "Active" : "Missing");
        ImGui::Text("SurfaceWater: %s", frame_context->resources.surface_water.isValid() ? "Active" : "Missing");
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
        ImGui::Text("SurfaceWaterFlow: %s", frame_context->resources.surface_water_flow.isValid() ? "Active" : "Missing");
        ImGui::Text("WaterVelocity: %s", frame_context->resources.water_velocity.isValid() ? "Active" : "Missing");
        ImGui::Text("WaterSediment: %s", frame_context->resources.water_sediment.isValid() ? "Active" : "Missing");
        ImGui::Text("VisibleWater: %s", frame_context->resources.visible_water.isValid() ? "Active" : "Missing");
        ImGui::Text("WaterMask: %s", frame_context->resources.water_mask.isValid() ? "Active" : "Missing");
        ImGui::Text("WaterInteractionData: %s", frame_context->resources.water_interaction_data.isValid() ? "Active" : "Missing");
        ImGui::Text("SoilMoisture: %s", frame_context->resources.soil_moisture.isValid() ? "Active" : "Missing");
        ImGui::Text("ErosionDelta: %s", frame_context->resources.erosion_delta.isValid() ? "Active" : "Missing");
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

        if (g_ShowComputeResourcePreviews && frame_context->resources.rain_map.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("RainMap Preview");
            ImGui::TextWrapped("Bright areas mean stronger rainfall. This drives surface water accumulation.");
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.rain_map.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }

        if (g_ShowComputeResourcePreviews && frame_context->resources.surface_water.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("SurfaceWater Preview");
            ImGui::TextWrapped("R = authoritative water depth. B = standing-water helper used by WaterMask. G/A remain preview helpers.");
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.surface_water.shaderResourceView())),
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

        if (g_ShowComputeResourcePreviews && frame_context->resources.surface_water_flow.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("SurfaceWater Flow Preview");
            ImGui::TextWrapped("RGBA = conserved outflow toward East / West / North / South.");
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.surface_water_flow.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }

        if (g_ShowComputeResourcePreviews && frame_context->resources.surface_water_flow_preview.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("SurfaceWater Flow Overlay");
            ImGui::TextWrapped("This is the lightweight visual overlay drawn on the safe water surface path.");
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.surface_water_flow_preview.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }

        if (g_ShowComputeResourcePreviews && frame_context->resources.water_velocity.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("WaterVelocity Preview");
            ImGui::TextWrapped("R/G = velocity XY, B = speed, A = transport energy. This is the raw hydrology velocity field.");
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.water_velocity.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }

        if (g_ShowComputeResourcePreviews && frame_context->resources.water_sediment.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("WaterSediment Preview");
            ImGui::TextWrapped("R = suspended sediment, G = deposition tendency, B = erosion tendency, A = sediment capacity.");
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.water_sediment.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }

        if (g_ShowComputeResourcePreviews && frame_context->resources.visible_water.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("VisibleWater Preview");
            ImGui::TextWrapped("R = visible standing water, G = thin runoff hint, B = pooled-water helper. This splits renderable water from raw simulation depth.");
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.visible_water.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }

        if (g_ShowComputeResourcePreviews && frame_context->resources.water_mask.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("WaterMask Preview");
            ImGui::TextWrapped("R = water contact, G = shoreline band, B = pooled-water support, A = render coverage.");
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.water_mask.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }

        if (g_ShowComputeResourcePreviews && frame_context->resources.water_interaction_data.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("WaterInteractionData Preview");
            ImGui::TextWrapped("R = surface interaction, G = shoreline influence, B = pooled-water interaction, A = retained wetness. This is the consumer-facing water field.");
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.water_interaction_data.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }

        if (g_ShowComputeResourcePreviews && frame_context->resources.soil_moisture.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("SoilMoisture Preview");
            ImGui::TextWrapped("R = retained soil moisture, G = shoreline seepage, B = recent rainfall memory.");
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.soil_moisture.shaderResourceView())),
                ImVec2(192.0f, 192.0f));
        }

        if (g_ShowComputeResourcePreviews && frame_context->resources.erosion_delta.isValid())
        {
            ImGui::Separator();
            ImGui::TextDisabled("ErosionDelta Preview");
            ImGui::TextWrapped("R = erosion accumulation, G = deposition accumulation, B = signed delta, A = hydraulic transport energy.");
            ImGui::Image(
                ImTextureRef(reinterpret_cast<ImTextureID>(frame_context->resources.erosion_delta.shaderResourceView())),
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

    ImGui::Begin("Post FX");
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
    ImGui::End();

    ImGui::Begin("Terrain");
    ImGui::SliderFloat("Base Frequency", &g_TerrainSettings.base_frequency, 0.0015f, 0.0200f, "%.4f");
    ImGui::SliderFloat("Base Height", &g_TerrainSettings.base_height, 4.0f, 36.0f);
    ImGui::SliderFloat("Detail Frequency", &g_TerrainSettings.detail_frequency, 1.0f, 8.0f);
    ImGui::SliderFloat("Detail Height", &g_TerrainSettings.detail_height, 0.0f, 12.0f);
    ImGui::SliderFloat("Ridge Frequency", &g_TerrainSettings.ridge_frequency, 0.5f, 4.0f);
    ImGui::SliderFloat("Ridge Height", &g_TerrainSettings.ridge_height, 0.0f, 28.0f);
    ImGui::SliderFloat("Continent Height", &g_TerrainSettings.continent_height, -2.0f, 10.0f);
    ImGui::Separator();
    ImGui::SliderFloat("Lake Center Z", &g_TerrainSettings.lake_center_z, -40.0f, 60.0f);
    ImGui::SliderFloat("Lake Radius X", &g_TerrainSettings.lake_radius_x, 8.0f, 80.0f);
    ImGui::SliderFloat("Lake Radius Z", &g_TerrainSettings.lake_radius_z, 8.0f, 80.0f);
    ImGui::SliderFloat("Lake Depth", &g_TerrainSettings.lake_depth, 0.0f, 20.0f);
    ImGui::Separator();
    ImGui::TextDisabled("Material Blend");
    ImGui::SliderFloat("Grass Slope Min", &g_TerrainMaterialSettings.grass_slope_min, 0.10f, 0.95f);
    ImGui::SliderFloat("Grass Slope Max", &g_TerrainMaterialSettings.grass_slope_max, 0.10f, 1.00f);
    ImGui::SliderFloat("Grass Noise", &g_TerrainMaterialSettings.grass_noise_strength, 0.0f, 0.40f);
    ImGui::SliderFloat("Grass Height Start", &g_TerrainMaterialSettings.grass_height_start, -4.0f, 24.0f);
    ImGui::SliderFloat("Grass Height End", &g_TerrainMaterialSettings.grass_height_end, -2.0f, 32.0f);
    ImGui::SliderFloat("Grass Coverage Min", &g_TerrainMaterialSettings.grass_coverage_min, 0.05f, 0.95f);
    ImGui::SliderFloat("Rock Slope Start", &g_TerrainMaterialSettings.rock_slope_start, 0.0f, 1.0f);
    ImGui::SliderFloat("Rock Slope End", &g_TerrainMaterialSettings.rock_slope_end, 0.0f, 1.0f);
    ImGui::SliderFloat("Rock Height Start", &g_TerrainMaterialSettings.rock_height_start, -4.0f, 28.0f);
    ImGui::SliderFloat("Rock Height End", &g_TerrainMaterialSettings.rock_height_end, -2.0f, 36.0f);
    ImGui::SliderFloat("Stone Noise Scale", &g_TerrainMaterialSettings.stone_noise_scale, 0.005f, 0.150f, "%.3f");
    ImGui::SliderFloat("Shore Offset Start", &g_TerrainMaterialSettings.shoreline_offset_start, 0.0f, 4.0f);
    ImGui::SliderFloat("Shore Offset End", &g_TerrainMaterialSettings.shoreline_offset_end, 1.0f, 10.0f);
    ImGui::SliderFloat("Lowland Start", &g_TerrainMaterialSettings.lowland_height_start, -8.0f, 48.0f);
    ImGui::SliderFloat("Lowland End", &g_TerrainMaterialSettings.lowland_height_end, 0.0f, 80.0f);
    ImGui::End();

    ImGui::Begin("Camera");
    if (ImGui::CollapsingHeader("Camera"))
    {
        extern float g_CameraMoveSpeed;
        ImGui::SliderFloat("Camera Speed", &g_CameraMoveSpeed, 1.0f, 20.0f);
    }
    ImGui::End();

    ImGui::SetNextWindowSizeConstraints(ImVec2(200, 300), ImVec2(500, 800));
    ImGui::Begin("Toon");

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
    ImGui::End();

    ImGui::Begin("Compute Registry");
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
    ImGui::End();

    ImGui::Begin("Instancing");
    ImGui::Text("Enabled: %s", InstancingDebug_IsEnabled() ? "Yes" : "No");
    ImGui::Text("Batches: %d", g_InstancingStats.instanced_batch_count);
    ImGui::Text("Instances: %d", g_InstancingStats.instanced_instance_count);
    ImGui::Text("Saved Draws: %d", g_InstancingStats.estimated_draw_calls_saved);
    ImGui::End();

    ImGui::Begin("Frustum Culling");
    ImGui::Text("Enabled: %s", FrustumCullingDebug_IsEnabled() ? "Yes" : "No");
    ImGui::Text("Tested: %d", g_FrustumCullingStats.tested_objects);
    ImGui::Text("Visible: %d", g_FrustumCullingStats.visible_objects);
    ImGui::Text("Culled: %d", g_FrustumCullingStats.culled_objects);
    ImGui::End();

    ImGui::Begin("Grass Compute");
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
    ImGui::End();

    ImGui::Begin("Render Flow");
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
    ImGui::End();
}

void DebugMenu_End()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
