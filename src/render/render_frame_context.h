#ifndef RENDER_FRAME_CONTEXT_H
#define RENDER_FRAME_CONTEXT_H

#include <DirectXMath.h>

#include "render_shadow_map_resource.h"
#include "terrain_water_state.h"

class RenderScene;
class ComputeSharedResourceRegistry;
struct ID3D11Buffer;

struct RenderFrameGlobals
{
	DirectX::XMFLOAT4X4 view_matrix{};
	DirectX::XMFLOAT4X4 projection_matrix{};
	DirectX::XMFLOAT3 camera_position{};
	float padding0 = 0.0f;
	unsigned int backbuffer_width = 0;
	unsigned int backbuffer_height = 0;
	double time_seconds = 0.0;
	double elapsed_time = 0.0;
	float glitch_amount = 0.0f;
};

struct RenderFrameResources
{
	Backend::RenderSceneColorResource scene_color{};
	Backend::RenderDepthResource scene_depth{};
	Backend::RenderShadowMapResource shadow_map{};
	TerrainWaterFrameState terrain_water{};
	Backend::RenderShaderResource base_terrain_height{};
	float base_terrain_min_height = 0.0f;
	float base_terrain_max_height = 0.0f;
	bool has_base_terrain_range = false;
	Backend::RenderShaderResource terrain_height{};
	float active_terrain_min_height = 0.0f;
	float active_terrain_max_height = 0.0f;
	bool has_active_terrain_range = false;
	Backend::RenderShaderResource terrain_normal{};
	Backend::RenderShaderResource terrain_surface_data{};
	Backend::RenderShaderResource terrain_vegetation_suitability{};
	Backend::RenderShaderResource grass_data{};
	Backend::RenderShaderResource rain_map{};
	Backend::RenderShaderResource atmosphere_preview{};
	Backend::RenderShaderResource water_surface_height{};
	float terrain_heightfield_min_height = 0.0f;
	float terrain_heightfield_max_height = 0.0f;
	bool has_terrain_heightfield_range = false;
	bool terrain_heightfield_range_is_fallback = false;
	float water_heightfield_min_height = 0.0f;
	float water_heightfield_max_height = 0.0f;
	bool has_water_heightfield_range = false;
	bool water_heightfield_range_is_fallback = false;
	Backend::RenderShaderResource surface_water_flow{};
	Backend::RenderShaderResource water_velocity{};
	Backend::RenderShaderResource water_sediment{};
	Backend::RenderShaderResource water_interaction_data{};
	Backend::RenderShaderResource soil_moisture{};
	Backend::RenderShaderResource erosion_delta{};
	Backend::RenderShaderResource compute_noise{};
	Backend::RenderShaderResource meteorograph_field{};
	ID3D11Buffer* floating_light_instance_buffer = nullptr;
	unsigned int floating_light_instance_count = 0;
	ComputeSharedResourceRegistry* compute_shared_registry = nullptr;
};

struct RenderFrameContext
{
	RenderScene* render_scene = nullptr;
	RenderFrameGlobals globals{};
	RenderFrameResources resources{};
};

#endif // RENDER_FRAME_CONTEXT_H
