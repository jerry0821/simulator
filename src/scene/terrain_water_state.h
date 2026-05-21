#ifndef TERRAIN_WATER_STATE_H
#define TERRAIN_WATER_STATE_H

#include "compute_shared_resource_registry.h"
#include "render_water_surface.h"

struct TerrainWaterFrameState
{
	float water_surface_level = 0.0f;
	WaterSurfaceDesc water_surface_desc{};
	Backend::RenderShaderResource terrain_height{};
	Backend::RenderShaderResource terrain_normal{};
	Backend::RenderShaderResource terrain_surface_data{};
	Backend::RenderShaderResource water_surface_height_texture{};
	Backend::RenderShaderResource surface_water_flow{};
	Backend::RenderShaderResource water_velocity{};
	Backend::RenderShaderResource water_sediment{};
	Backend::RenderShaderResource water_interaction_data{};
	Backend::RenderShaderResource soil_moisture{};
	Backend::RenderShaderResource erosion_delta{};

	bool HasHydrologyData() const
	{
		return water_surface_height_texture.isValid() ||
			water_velocity.isValid() ||
			water_sediment.isValid() ||
			water_interaction_data.isValid();
	}

	bool HasWaterSurfaceHeight() const
	{
		return water_surface_level > 0.0f;
	}

	bool HasWaterSurfaceDesc() const
	{
		return water_surface_desc.enabled && water_surface_desc.size_x > 0.0f && water_surface_desc.size_z > 0.0f;
	}

	bool HasSurfaceMaterialData() const
	{
		return terrain_surface_data.isValid() ||
			water_interaction_data.isValid() ||
			soil_moisture.isValid() ||
			erosion_delta.isValid();
	}

};

class TerrainWaterState
{
public:
	void Clear()
	{
		frame_state_ = TerrainWaterFrameState{};
	}

	void Update(
		float water_surface_height,
		const WaterSurfaceDesc& water_surface_desc,
		Backend::RenderShaderResource terrain_height,
		Backend::RenderShaderResource terrain_normal,
		Backend::RenderShaderResource terrain_surface_data,
		Backend::RenderShaderResource water_surface_height_texture,
		Backend::RenderShaderResource surface_water_flow,
		Backend::RenderShaderResource water_velocity,
		Backend::RenderShaderResource water_sediment,
		Backend::RenderShaderResource water_interaction_data,
		Backend::RenderShaderResource soil_moisture,
		Backend::RenderShaderResource erosion_delta)
	{
		frame_state_.water_surface_level = water_surface_height;
		frame_state_.water_surface_desc = water_surface_desc;
		frame_state_.terrain_height = terrain_height;
		frame_state_.terrain_normal = terrain_normal;
		frame_state_.terrain_surface_data = terrain_surface_data;
		frame_state_.water_surface_height_texture = water_surface_height_texture;
		frame_state_.surface_water_flow = surface_water_flow;
		frame_state_.water_velocity = water_velocity;
		frame_state_.water_sediment = water_sediment;
		frame_state_.water_interaction_data = water_interaction_data;
		frame_state_.soil_moisture = soil_moisture;
		frame_state_.erosion_delta = erosion_delta;
	}

	const TerrainWaterFrameState& FrameState() const
	{
		return frame_state_;
	}

	void PublishTo(ComputeSharedResourceRegistry& registry) const
	{
		registry.PublishShaderResource(
			ComputeSharedResourceId::TerrainHeight,
			"TerrainHeight",
			frame_state_.terrain_height);
		registry.PublishShaderResource(
			ComputeSharedResourceId::TerrainNormal,
			"TerrainNormal",
			frame_state_.terrain_normal);
		registry.PublishShaderResource(
			ComputeSharedResourceId::TerrainSurfaceData,
			"TerrainSurfaceData",
			frame_state_.terrain_surface_data);
		registry.PublishShaderResource(
			ComputeSharedResourceId::WaterSurfaceHeight,
			"WaterSurfaceHeight",
			frame_state_.water_surface_height_texture);
		registry.PublishShaderResource(
			ComputeSharedResourceId::SurfaceWaterFlow,
			"SurfaceWaterFlow",
			frame_state_.surface_water_flow);
		registry.PublishShaderResource(
			ComputeSharedResourceId::WaterVelocity,
			"WaterVelocity",
			frame_state_.water_velocity);
		registry.PublishShaderResource(
			ComputeSharedResourceId::WaterSediment,
			"WaterSediment",
			frame_state_.water_sediment);
		registry.PublishShaderResource(
			ComputeSharedResourceId::WaterInteractionData,
			"WaterInteractionData",
			frame_state_.water_interaction_data);
		registry.PublishShaderResource(
			ComputeSharedResourceId::SoilMoisture,
			"SoilMoisture",
			frame_state_.soil_moisture);
		registry.PublishShaderResource(
			ComputeSharedResourceId::ErosionDelta,
			"ErosionDelta",
			frame_state_.erosion_delta);
	}

private:
	TerrainWaterFrameState frame_state_{};
};

#endif // TERRAIN_WATER_STATE_H
