#ifndef COMPUTE_SHARED_RESOURCE_REGISTRY_H
#define COMPUTE_SHARED_RESOURCE_REGISTRY_H

#include <array>
#include <cstddef>

#include "render_shadow_map_resource.h"

struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;

enum class ComputeSharedResourceId
{
	BaseTerrainHeight,
	TerrainHeight,
	TerrainNormal,
	TerrainSurfaceData,
	TerrainVegetationSuitability,
	GrassData,
	RainMap,
	SurfaceWater,
	WaterSurfaceHeight,
	SurfaceWaterFlow,
	WaterVelocity,
	WaterSediment,
	WaterInteractionData,
	SoilMoisture,
	ErosionDelta,
	ComputeNoise,
	WindField,
	ClimateField,
	MeteorographField,
	GrassInstances,
	GrassIndirectArgs,
	FloatingLightInstances,
	Count,
};

enum class ComputeSharedResourceKind
{
	None,
	ShaderTexture,
	StructuredBuffer,
	IndirectArgs,
};

struct ComputeSharedResourceEntry
{
	const char* debug_name = "";
	ComputeSharedResourceKind kind = ComputeSharedResourceKind::None;
	Backend::RenderShaderResource shader_resource{};
	ID3D11Buffer* buffer = nullptr;
	ID3D11ShaderResourceView* srv = nullptr;
	ID3D11UnorderedAccessView* uav = nullptr;
	unsigned int element_count = 0;
	unsigned int stride = 0;
};

class ComputeSharedResourceRegistry
{
public:
	void Clear()
	{
		for (ComputeSharedResourceEntry& entry : m_entries)
		{
			entry = ComputeSharedResourceEntry{};
		}
	}

	void Reset(ComputeSharedResourceId id)
	{
		m_entries[toIndex(id)] = ComputeSharedResourceEntry{};
	}

	void PublishShaderResource(
		ComputeSharedResourceId id,
		const char* debug_name,
		Backend::RenderShaderResource shader_resource)
	{
		ComputeSharedResourceEntry& entry = m_entries[toIndex(id)];
		entry = ComputeSharedResourceEntry{};
		entry.debug_name = debug_name != nullptr ? debug_name : "";
		entry.kind = ComputeSharedResourceKind::ShaderTexture;
		entry.shader_resource = shader_resource;
		entry.srv = shader_resource.shaderResourceView();
	}

	void PublishStructuredBuffer(
		ComputeSharedResourceId id,
		const char* debug_name,
		ID3D11Buffer* buffer,
		ID3D11ShaderResourceView* srv,
		ID3D11UnorderedAccessView* uav,
		unsigned int element_count = 0,
		unsigned int stride = 0)
	{
		ComputeSharedResourceEntry& entry = m_entries[toIndex(id)];
		entry = ComputeSharedResourceEntry{};
		entry.debug_name = debug_name != nullptr ? debug_name : "";
		entry.kind = ComputeSharedResourceKind::StructuredBuffer;
		entry.buffer = buffer;
		entry.srv = srv;
		entry.uav = uav;
		entry.element_count = element_count;
		entry.stride = stride;
	}

	void PublishIndirectArgs(
		ComputeSharedResourceId id,
		const char* debug_name,
		ID3D11Buffer* buffer)
	{
		ComputeSharedResourceEntry& entry = m_entries[toIndex(id)];
		entry = ComputeSharedResourceEntry{};
		entry.debug_name = debug_name != nullptr ? debug_name : "";
		entry.kind = ComputeSharedResourceKind::IndirectArgs;
		entry.buffer = buffer;
	}

	bool Has(ComputeSharedResourceId id) const
	{
		const ComputeSharedResourceEntry& entry = m_entries[toIndex(id)];
		return entry.kind != ComputeSharedResourceKind::None;
	}

	const ComputeSharedResourceEntry& Get(ComputeSharedResourceId id) const
	{
		return m_entries[toIndex(id)];
	}

	Backend::RenderShaderResource GetShaderResource(ComputeSharedResourceId id) const
	{
		return m_entries[toIndex(id)].shader_resource;
	}

	ID3D11Buffer* GetBuffer(ComputeSharedResourceId id) const
	{
		return m_entries[toIndex(id)].buffer;
	}

	ID3D11ShaderResourceView* GetShaderResourceView(ComputeSharedResourceId id) const
	{
		return m_entries[toIndex(id)].srv;
	}

	ID3D11UnorderedAccessView* GetUnorderedAccessView(ComputeSharedResourceId id) const
	{
		return m_entries[toIndex(id)].uav;
	}

private:
	static constexpr std::size_t toIndex(ComputeSharedResourceId id)
	{
		return static_cast<std::size_t>(id);
	}

	std::array<ComputeSharedResourceEntry, static_cast<std::size_t>(ComputeSharedResourceId::Count)> m_entries{};
};

#endif // COMPUTE_SHARED_RESOURCE_REGISTRY_H
