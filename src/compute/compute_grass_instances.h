#ifndef COMPUTE_GRASS_INSTANCES_H
#define COMPUTE_GRASS_INSTANCES_H

#include <d3d11.h>
#include <DirectXMath.h>
#include <string>

#include "compute_task.h"
#include "meshfield.h"

class ComputeGrassInstances : public ComputeTask
{
public:
	~ComputeGrassInstances() override = default;

	const char* DebugName() const override
	{
		return "ComputeGrassInstances";
	}

	ComputeTaskDispatchMode DispatchMode() const override
	{
		return ComputeTaskDispatchMode::EveryFrame;
	}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
	void Finalize() override;

	bool ConfigureCoverage(
		ID3D11ShaderResourceView* terrain_height_srv,
		ID3D11ShaderResourceView* terrain_vegetation_suitability_srv,
		ID3D11ShaderResourceView* terrain_classification_srv,
		unsigned int grid_cols,
		unsigned int grid_rows,
		float world_min_x,
		float world_min_z,
		float spacing,
		const TerrainSettings& terrain_settings);
	void Update(
		float quad_scale_x,
		float quad_scale_y,
		const DirectX::XMFLOAT3& camera_position,
		const DirectX::XMMATRIX& view,
		const DirectX::XMMATRIX& proj,
		float current_time_seconds) const;

	bool IsValid() const override;
	bool HasSeeds() const;
	void MarkCoverageDirty();

	ID3D11Buffer* InstanceBuffer() const;
	ID3D11ShaderResourceView* InstanceSRV() const;
	ID3D11Buffer* ArgsBuffer() const;
	unsigned int InstanceStride() const;
	unsigned int InstanceCount() const;
	unsigned int GridCols() const;
	unsigned int GridRows() const;
	unsigned int SeedCount() const;
	unsigned int LastVisibleInstanceCount() const;
	bool DispatchDirty() const;
	const char* LastError() const;

private:
	struct GrassSeedData
	{
		DirectX::XMFLOAT4 data{};
	};

	struct GrassInstanceConstants
	{
		DirectX::XMFLOAT4X4 view_proj{};
		float quad_scale_x = 1.0f;
		float quad_scale_y = 1.0f;
		float world_min_x = -256.0f;
		float world_min_z = -256.0f;
		float spacing = 6.5f;
		float sample_offset = 2.2f;
		float water_height = 0.35f;
		float camera_x = 0.0f;
		float camera_z = 0.0f;
		float lod_full_distance = 42.0f;
		float lod_max_distance = 88.0f;
		float far_keep_probability = 0.24f;
		unsigned int grid_cols = 0;
		unsigned int grid_rows = 0;
		unsigned int quads_per_seed = 3;
		unsigned int padding0 = 0;
	};

	static constexpr unsigned int kQuadsPerSeed = 3;
	static constexpr unsigned int kThreadGroupSize = 64;

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;
	ID3D11ComputeShader* m_coverage_compute_shader = nullptr;
	ID3D11ComputeShader* m_cull_compute_shader = nullptr;
	ID3D11ShaderResourceView* m_terrain_height_srv = nullptr;
	ID3D11ShaderResourceView* m_terrain_vegetation_suitability_srv = nullptr;
	ID3D11ShaderResourceView* m_terrain_classification_srv = nullptr;
	ID3D11Buffer* m_seed_buffer = nullptr;
	ID3D11ShaderResourceView* m_seed_srv = nullptr;
	ID3D11UnorderedAccessView* m_seed_uav = nullptr;
	ID3D11Buffer* m_instance_buffer = nullptr;
	ID3D11ShaderResourceView* m_instance_srv = nullptr;
	ID3D11UnorderedAccessView* m_instance_uav = nullptr;
	ID3D11Buffer* m_args_buffer = nullptr;
	ID3D11Buffer* m_args_readback_buffer = nullptr;
	ID3D11Buffer* m_constant_buffer = nullptr;
	unsigned int m_grid_cols = 0;
	unsigned int m_grid_rows = 0;
	unsigned int m_instance_count = 0;
	float m_world_min_x = -256.0f;
	float m_world_min_z = -256.0f;
	float m_spacing = 6.5f;
	TerrainSettings m_terrain_settings{};
	mutable bool m_dispatch_dirty = true;
	mutable unsigned int m_last_visible_instance_count = 0;
	mutable float m_last_readback_time_seconds = -1000.0f;
	std::string m_last_error = "Not configured";
};

#endif // COMPUTE_GRASS_INSTANCES_H
