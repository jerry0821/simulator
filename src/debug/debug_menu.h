// ----------------------------------------------------
// debug_menu [debug_menu.h]
// ====================================================
// Created by: Jerry
// Date: 2025-12-11
// ----------------------------------------------------
#ifndef DEBUG_MENU_H
#define DEBUG_MENU_H
#include <d3d11.h>
#include <string>

struct RenderFramePlan;
struct RenderFrameContext;
struct InstancingStats;
struct FrustumCullingStats;
struct GrassComputeStats;
struct ComputeNoiseSettings;
struct TerrainSettings;
struct TerrainMaterialSettings;
struct WaterSurfaceDesc;
struct SurfaceWaterSimulationSettings
{
	float accumulation_rate = 0.075f;
	float evaporation_rate = 0.0035f;
	float seepage_rate = 0.0030f;
	float basin_fade = 6.0f;
	float downhill_flow_rate = 0.22f;
	float flow_damping = 0.90f;
	float max_outflow_fraction = 0.55f;
	float debug_injection_x = 46.0f;
	float debug_injection_z = 118.0f;
	float debug_injection_radius = 14.0f;
	float debug_injection_amount = 0.90f;
};

struct PostProcessSettings
{
	float chromatic_aberration = 0.0025f;
	float film_grain = 0.038f;
	float film_grain_speed = 1.15f;
	float film_grain_debug_boost = 1.0f;
	float atmospheric_dust = 0.0f;
	float vignette_intensity = 0.18f;
	float vignette_softness = 0.42f;
	float bloom_intensity = 0.22f;
	float bloom_threshold = 0.74f;
	float bloom_soft_knee = 0.16f;
	float bloom_blur_scale = 1.65f;
	float fog_intensity = 0.20f;
	float fog_distance_fade = 0.24f;
	float fog_height_bias = 0.0f;
	float fog_env_mix = 0.82f;
	float fog_scatter_strength = 0.28f;
	float fog_scatter_focus = 0.38f;
};

struct GrassComputeStats
{
	bool gpu_ready = false;
	bool gpu_path_used = false;
	bool fallback_used = false;
	bool coverage_dirty = false;
	unsigned int grid_cols = 0;
	unsigned int grid_rows = 0;
	unsigned int seed_count = 0;
	unsigned int max_instance_capacity = 0;
	unsigned int visible_instance_count = 0;
	std::string last_error;
};

void DebugMenu_Initialize(ID3D11Device* device, ID3D11DeviceContext* context, HWND hwnd);
void DebugMenu_Finalize();

void DebugMenu_SetFramePlan(const RenderFramePlan& frame_plan);
void DebugMenu_SetInstancingStats(const InstancingStats& instancing_stats);
void DebugMenu_SetFrustumCullingStats(const FrustumCullingStats& frustum_culling_stats);
void DebugMenu_SetGrassComputeStats(const GrassComputeStats& grass_compute_stats);
void DebugMenu_SetPerformanceStats(float fps, float frame_time_ms);
void DebugMenu_SetShaderReloadStatus(bool succeeded, const char* message);
bool DebugMenu_ConsumeShaderReloadRequest();
bool DebugMenu_ConsumeSurfaceWaterInjectionRequest();
bool DebugMenu_ConsumeSurfaceWaterResetRequest();
const ComputeNoiseSettings& DebugMenu_GetComputeNoiseSettings();
const TerrainSettings& DebugMenu_GetTerrainSettings();
const TerrainMaterialSettings& DebugMenu_GetTerrainMaterialSettings();
const WaterSurfaceDesc& DebugMenu_GetWaterSurfaceSettings();
const SurfaceWaterSimulationSettings& DebugMenu_GetSurfaceWaterSimulationSettings();
const PostProcessSettings& DebugMenu_GetPostProcessSettings();
bool DebugMenu_IsFinalTerrainHeightEnabled();
bool DebugMenu_IsTerrainClassificationEnabled();
bool DebugMenu_IsGrassGpuEnabled();
bool DebugMenu_IsWaterSurfaceDeformationEnabled();
void DebugMenu_Begin();
void DebugMenu_Draw(const RenderFrameContext* frame_context = nullptr);
void DebugMenu_End();

#endif // DEBUG_MENU_H
