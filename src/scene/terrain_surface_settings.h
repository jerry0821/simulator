#ifndef TERRAIN_SURFACE_SETTINGS_H
#define TERRAIN_SURFACE_SETTINGS_H

struct TerrainMaterialSettings
{
	float grass_slope_min = 0.55f;
	float grass_slope_max = 0.88f;
	float grass_noise_strength = 0.18f;
	float grass_height_start = 7.5f;
	float grass_height_end = 16.0f;
	float rock_slope_start = 0.45f;
	float rock_slope_end = 0.82f;
	float rock_height_start = 9.0f;
	float rock_height_end = 18.0f;
	float stone_noise_scale = 0.035f;
	float shoreline_offset_start = 0.55f;
	float shoreline_offset_end = 5.5f;
	float lowland_height_start = 26.0f;
	float lowland_height_end = 58.0f;
	float grass_coverage_min = 0.28f;
	float water_height = 0.0f;

	bool operator==(const TerrainMaterialSettings& other) const
	{
		return grass_slope_min == other.grass_slope_min &&
			grass_slope_max == other.grass_slope_max &&
			grass_noise_strength == other.grass_noise_strength &&
			grass_height_start == other.grass_height_start &&
			grass_height_end == other.grass_height_end &&
			rock_slope_start == other.rock_slope_start &&
			rock_slope_end == other.rock_slope_end &&
			rock_height_start == other.rock_height_start &&
			rock_height_end == other.rock_height_end &&
			stone_noise_scale == other.stone_noise_scale &&
			shoreline_offset_start == other.shoreline_offset_start &&
			shoreline_offset_end == other.shoreline_offset_end &&
			lowland_height_start == other.lowland_height_start &&
			lowland_height_end == other.lowland_height_end &&
			grass_coverage_min == other.grass_coverage_min &&
			water_height == other.water_height;
	}

	bool operator!=(const TerrainMaterialSettings& other) const
	{
		return !(*this == other);
	}
};

#endif // TERRAIN_SURFACE_SETTINGS_H
