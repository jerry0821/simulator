#ifndef TERRAIN_WATER_STATE_H
#define TERRAIN_WATER_STATE_H

#include "compute_shared_resource_registry.h"
#include "render_water_surface.h"

struct TerrainWaterFrameState
{
	float water_surface_level = 0.0f;
	WaterSurfaceDesc water_surface_desc{};
	Backend::RenderShaderResource terrain_height{};
	Backend::RenderShaderResource terrain_classification{};
	Backend::RenderShaderResource terrain_vegetation_suitability{};
	Backend::RenderShaderResource surface_water{};
	Backend::RenderShaderResource water_surface_height_texture{};
	Backend::RenderShaderResource surface_water_flow{};
	Backend::RenderShaderResource surface_water_flow_preview{};
	Backend::RenderShaderResource visible_water{};
	Backend::RenderShaderResource water_mask{};
	Backend::RenderShaderResource soil_moisture{};
	Backend::RenderShaderResource erosion_delta{};

	bool HasHydrologyData() const
	{
		return surface_water.isValid() || water_mask.isValid() || visible_water.isValid();
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
		return terrain_classification.isValid() || soil_moisture.isValid() || erosion_delta.isValid();
	}

	bool HasVegetationData() const
	{
		return terrain_vegetation_suitability.isValid();
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
		Backend::RenderShaderResource terrain_classification,
		Backend::RenderShaderResource terrain_vegetation_suitability,
		Backend::RenderShaderResource surface_water,
		Backend::RenderShaderResource water_surface_height_texture,
		Backend::RenderShaderResource surface_water_flow,
		Backend::RenderShaderResource surface_water_flow_preview,
		Backend::RenderShaderResource visible_water,
		Backend::RenderShaderResource water_mask,
		Backend::RenderShaderResource soil_moisture,
		Backend::RenderShaderResource erosion_delta)
	{
		frame_state_.water_surface_level = water_surface_height;
		frame_state_.water_surface_desc = water_surface_desc;
		frame_state_.terrain_height = terrain_height;
		frame_state_.terrain_classification = terrain_classification;
		frame_state_.terrain_vegetation_suitability = terrain_vegetation_suitability;
		frame_state_.surface_water = surface_water;
		frame_state_.water_surface_height_texture = water_surface_height_texture;
		frame_state_.surface_water_flow = surface_water_flow;
		frame_state_.surface_water_flow_preview = surface_water_flow_preview;
		frame_state_.visible_water = visible_water;
		frame_state_.water_mask = water_mask;
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
			ComputeSharedResourceId::TerrainClassification,
			"TerrainClassification",
			frame_state_.terrain_classification);
		registry.PublishShaderResource(
			ComputeSharedResourceId::TerrainVegetationSuitability,
			"TerrainVegetationSuitability",
			frame_state_.terrain_vegetation_suitability);
		registry.PublishShaderResource(
			ComputeSharedResourceId::SurfaceWater,
			"SurfaceWater",
			frame_state_.surface_water);
		registry.PublishShaderResource(
			ComputeSharedResourceId::WaterSurfaceHeight,
			"WaterSurfaceHeight",
			frame_state_.water_surface_height_texture);
		registry.PublishShaderResource(
			ComputeSharedResourceId::SurfaceWaterFlow,
			"SurfaceWaterFlow",
			frame_state_.surface_water_flow);
		registry.PublishShaderResource(
			ComputeSharedResourceId::SurfaceWaterFlowPreview,
			"SurfaceWaterFlowPreview",
			frame_state_.surface_water_flow_preview);
		registry.PublishShaderResource(
			ComputeSharedResourceId::VisibleWater,
			"VisibleWater",
			frame_state_.visible_water);
		registry.PublishShaderResource(
			ComputeSharedResourceId::WaterMask,
			"WaterMask",
			frame_state_.water_mask);
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
