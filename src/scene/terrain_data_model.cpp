#include "terrain_data_model.h"

#include <algorithm>
#include <cmath>

#include <DirectXMath.h>

#include "terrain_height_field.h"

namespace
{
using namespace DirectX;

TerrainMaterialSettings g_material_settings{};

XMFLOAT3 NormalizeOrUp(const XMFLOAT3& vector)
{
	const float length_sq = vector.x * vector.x + vector.y * vector.y + vector.z * vector.z;
	if (length_sq <= 1.0e-6f)
	{
		return { 0.0f, 1.0f, 0.0f };
	}

	const float inv_length = 1.0f / std::sqrt(length_sq);
	return { vector.x * inv_length, vector.y * inv_length, vector.z * inv_length };
}

float Frac01(float value)
{
	return value - std::floor(value);
}

float Hash21(float x, float y)
{
	return Frac01(std::sin(x * 127.1f + y * 311.7f) * 43758.5453f);
}

float Smoothstep01(float value)
{
	return value * value * (3.0f - 2.0f * value);
}

float RemapClamped(float value, float in_min, float in_max)
{
	if (in_max <= in_min)
	{
		return 0.0f;
	}

	return std::clamp((value - in_min) / (in_max - in_min), 0.0f, 1.0f);
}

float ValueNoise(float x, float y)
{
	const float floor_x = std::floor(x);
	const float floor_y = std::floor(y);
	const float frac_x = x - floor_x;
	const float frac_y = y - floor_y;

	const float n00 = Hash21(floor_x, floor_y);
	const float n10 = Hash21(floor_x + 1.0f, floor_y);
	const float n01 = Hash21(floor_x, floor_y + 1.0f);
	const float n11 = Hash21(floor_x + 1.0f, floor_y + 1.0f);

	const float sx = Smoothstep01(frac_x);
	const float sy = Smoothstep01(frac_y);
	const float nx0 = std::lerp(n00, n10, sx);
	const float nx1 = std::lerp(n01, n11, sx);
	return std::lerp(nx0, nx1, sy);
}

float Fbm(float x, float y)
{
	float value = 0.0f;
	float amplitude = 0.5f;
	float domain_x = x;
	float domain_y = y;

	for (int octave = 0; octave < 4; ++octave)
	{
		value += ValueNoise(domain_x, domain_y) * amplitude;
		domain_x = domain_x * 2.03f + 17.0f;
		domain_y = domain_y * 2.03f + 9.0f;
		amplitude *= 0.5f;
	}

	return value;
}
}

void TerrainDataModel::SetMaterialSettings(const TerrainMaterialSettings& material_settings)
{
	g_material_settings = material_settings;
	if (g_material_settings.water_height <= 0.0f)
	{
		g_material_settings.water_height = TerrainHeightField::GetSuggestedWaterHeight();
	}
}

Backend::RenderShaderResource TerrainDataModel::HeightResource()
{
	return TerrainHeightField::HeightResource();
}

ID3D11ShaderResourceView* TerrainDataModel::HeightSRV()
{
	return HeightResource().shaderResourceView();
}

float TerrainDataModel::SampleHeightWorld(float world_x, float world_z)
{
	return TerrainHeightField::GetHeight(world_x, world_z);
}

DirectX::XMFLOAT3 TerrainDataModel::SampleNormalWorld(float world_x, float world_z, float sample_offset)
{
	if (sample_offset <= 0.0f)
	{
		return { 0.0f, 1.0f, 0.0f };
	}

	const float left_height = SampleHeightWorld(world_x - sample_offset, world_z);
	const float right_height = SampleHeightWorld(world_x + sample_offset, world_z);
	const float up_height = SampleHeightWorld(world_x, world_z - sample_offset);
	const float down_height = SampleHeightWorld(world_x, world_z + sample_offset);

	const XMFLOAT3 tangent = { sample_offset * 2.0f, right_height - left_height, 0.0f };
	const XMFLOAT3 bitangent = { 0.0f, down_height - up_height, sample_offset * 2.0f };
	const XMVECTOR tangent_vector = XMLoadFloat3(&tangent);
	const XMVECTOR bitangent_vector = XMLoadFloat3(&bitangent);
	const XMVECTOR normal_vector = XMVector3Normalize(XMVector3Cross(bitangent_vector, tangent_vector));

	XMFLOAT3 normal{};
	XMStoreFloat3(&normal, normal_vector);
	return NormalizeOrUp(normal);
}

float TerrainDataModel::SampleNormalYWorld(float world_x, float world_z, float sample_offset)
{
	return std::clamp(SampleNormalWorld(world_x, world_z, sample_offset).y, 0.0f, 1.0f);
}

TerrainMaterialClassification TerrainDataModel::ClassifyMaterialWorld(
	float world_x,
	float world_z,
	float height,
	float normal_y)
{
	TerrainMaterialClassification classification{};
	const float water_height = g_material_settings.water_height > 0.0f
		? g_material_settings.water_height
		: SuggestedWaterHeight();

	classification.grass_flatness =
		RemapClamped(normal_y, g_material_settings.grass_slope_min, g_material_settings.grass_slope_max);
	classification.lowland = 1.0f - Smoothstep01(RemapClamped(
		height,
		g_material_settings.lowland_height_end,
		g_material_settings.lowland_height_end + 30.0f));
	classification.shoreline =
		Smoothstep01(RemapClamped(
			height,
			water_height + g_material_settings.shoreline_offset_start,
			water_height + g_material_settings.shoreline_offset_end));
	const float above_water =
		Smoothstep01(RemapClamped(
			height,
			water_height + std::max(g_material_settings.shoreline_offset_start * 0.18f, 0.08f),
			water_height + std::max(g_material_settings.shoreline_offset_start + 0.95f, 1.15f)));

	const float macro_noise = Fbm(world_x * 0.0125f + 37.0f, world_z * 0.0125f - 21.0f);
	const float patch_noise = ValueNoise(world_x * 0.050f - 11.0f, world_z * 0.050f + 17.0f);
	const float detail_noise = ValueNoise(world_x * 0.110f + 23.0f, world_z * 0.110f - 31.0f);
	const float macro_mask = Smoothstep01(RemapClamped(macro_noise, 0.40f, 0.72f));
	const float patch_mask =
		Smoothstep01(RemapClamped(
			patch_noise * 0.7f + detail_noise * 0.3f + (macro_noise - 0.5f) * g_material_settings.grass_noise_strength,
			0.44f,
			0.72f));
	const float vegetation_noise =
		Smoothstep01(RemapClamped(macro_noise * 0.65f + patch_noise * 0.35f, 0.22f, 0.70f));
	const float surface_grass_coverage =
		classification.grass_flatness *
		classification.shoreline *
		std::lerp(0.72f, 1.0f, classification.lowland) *
		std::lerp(0.42f, 1.0f, macro_mask) *
		patch_mask;
	const float vegetation_suitability =
		classification.grass_flatness *
		above_water *
		std::lerp(0.68f, 1.0f, vegetation_noise);
	classification.grass_coverage = surface_grass_coverage;
	classification.vegetation_suitability = vegetation_suitability;
	classification.grass_habitat =
		height > water_height + std::max(g_material_settings.shoreline_offset_start * 0.18f, 0.08f) &&
		classification.grass_flatness > 0.08f &&
		classification.vegetation_suitability > std::max(0.24f, g_material_settings.grass_coverage_min * 0.85f);

	const float cliff_slope_mask =
		1.0f - Smoothstep01(RemapClamped(normal_y, g_material_settings.rock_slope_start, g_material_settings.rock_slope_end));
	const float cliff_height_mask =
		Smoothstep01(RemapClamped(height, g_material_settings.rock_height_start, g_material_settings.rock_height_end));
	classification.rock = std::clamp(cliff_slope_mask * 1.15f + cliff_height_mask * 0.30f, 0.0f, 1.0f);

	return classification;
}

TerrainSurfaceSample TerrainDataModel::SampleSurfaceWorld(float world_x, float world_z, float normal_sample_offset)
{
	TerrainSurfaceSample sample{};
	sample.height = SampleHeightWorld(world_x, world_z);
	sample.normal = SampleNormalWorld(world_x, world_z, normal_sample_offset);
	sample.normal_y = std::clamp(sample.normal.y, 0.0f, 1.0f);
	sample.material = ClassifyMaterialWorld(world_x, world_z, sample.height, sample.normal_y);
	return sample;
}

bool TerrainDataModel::IsGrassHabitatWorld(float world_x, float world_z, float height, float normal_y)
{
	return ClassifyMaterialWorld(world_x, world_z, height, normal_y).grass_habitat;
}

float TerrainDataModel::ActiveWaterSurfaceHeight()
{
	return g_material_settings.water_height > 0.0f
		? g_material_settings.water_height
		: SuggestedWaterHeight();
}

float TerrainDataModel::SuggestedWaterHeight()
{
	return TerrainHeightField::GetSuggestedWaterHeight();
}
