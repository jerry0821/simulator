#include "compute_grass_instances.h"

#include <cmath>
#include <fstream>
#include <vector>

#include "direct3d.h"
#include "debug_ostream.h"
#include "sampler.h"
#include "terrain_data_model.h"

namespace
{
template <typename T>
void SafeRelease(T*& resource)
{
	if (resource != nullptr)
	{
		resource->Release();
		resource = nullptr;
	}
}

constexpr float kGrassLodFullDistance = 78.0f;
constexpr float kGrassLodMaxDistance = 168.0f;
constexpr float kGrassFarKeepProbability = 0.18f;
}

bool ComputeGrassInstances::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	Finalize();
	m_last_error = "Initializing";

	m_device = device;
	m_context = context;
	if (m_device == nullptr || m_context == nullptr)
	{
		m_last_error = "Missing device/context";
		return false;
	}

	std::ifstream coverage_shader_stream("resource/shader/shader_compute_grass_coverage.cso", std::ios::binary);
	if (!coverage_shader_stream)
	{
		hal::dout << "ComputeGrassInstances::Initialize(): failed to open shader_compute_grass_coverage.cso" << std::endl;
		m_last_error = "Missing coverage shader";
		return false;
	}

	const std::vector<char> coverage_shader_bytes((std::istreambuf_iterator<char>(coverage_shader_stream)),
												  std::istreambuf_iterator<char>());
	if (coverage_shader_bytes.empty())
	{
		m_last_error = "Coverage shader is empty";
		return false;
	}

	if (FAILED(m_device->CreateComputeShader(
			coverage_shader_bytes.data(),
			coverage_shader_bytes.size(),
			nullptr,
			&m_coverage_compute_shader)))
	{
		hal::dout << "ComputeGrassInstances::Initialize(): CreateComputeShader(coverage) failed" << std::endl;
		m_last_error = "CreateComputeShader(coverage) failed";
		Finalize();
		return false;
	}

	std::ifstream cull_shader_stream("resource/shader/shader_compute_grass_instances.cso", std::ios::binary);
	if (!cull_shader_stream)
	{
		hal::dout << "ComputeGrassInstances::Initialize(): failed to open shader_compute_grass_instances.cso" << std::endl;
		m_last_error = "Missing grass cull shader";
		Finalize();
		return false;
	}

	const std::vector<char> cull_shader_bytes((std::istreambuf_iterator<char>(cull_shader_stream)),
											  std::istreambuf_iterator<char>());
	if (cull_shader_bytes.empty())
	{
		m_last_error = "Cull shader is empty";
		Finalize();
		return false;
	}

	if (FAILED(m_device->CreateComputeShader(cull_shader_bytes.data(), cull_shader_bytes.size(), nullptr, &m_cull_compute_shader)))
	{
		hal::dout << "ComputeGrassInstances::Initialize(): CreateComputeShader(cull) failed" << std::endl;
		m_last_error = "CreateComputeShader(cull) failed";
		Finalize();
		return false;
	}

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.ByteWidth = sizeof(GrassInstanceConstants);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(m_device->CreateBuffer(&buffer_desc, nullptr, &m_constant_buffer)))
	{
		hal::dout << "ComputeGrassInstances::Initialize(): CreateBuffer(constant) failed" << std::endl;
		m_last_error = "CreateBuffer(constant) failed";
		Finalize();
		return false;
	}

	m_last_error = "Ready";
	return true;
}

void ComputeGrassInstances::Finalize()
{
	m_grid_cols = 0;
	m_grid_rows = 0;
	m_instance_count = 0;
	m_last_visible_instance_count = 0;
	m_last_readback_time_seconds = -1000.0f;
	m_world_min_x = -256.0f;
	m_world_min_z = -256.0f;
	m_spacing = 6.5f;
	m_terrain_height_srv = nullptr;
	m_terrain_vegetation_suitability_srv = nullptr;
	m_terrain_classification_srv = nullptr;
	m_terrain_settings = TerrainSettings{};
	m_dispatch_dirty = true;

	SafeRelease(m_constant_buffer);
	SafeRelease(m_args_readback_buffer);
	SafeRelease(m_args_buffer);
	SafeRelease(m_instance_uav);
	SafeRelease(m_instance_srv);
	SafeRelease(m_instance_buffer);
	SafeRelease(m_seed_uav);
	SafeRelease(m_seed_srv);
	SafeRelease(m_seed_buffer);
	SafeRelease(m_cull_compute_shader);
	SafeRelease(m_coverage_compute_shader);

	m_device = nullptr;
	m_context = nullptr;
}

bool ComputeGrassInstances::ConfigureCoverage(
	ID3D11ShaderResourceView* terrain_height_srv,
	ID3D11ShaderResourceView* terrain_vegetation_suitability_srv,
	ID3D11ShaderResourceView* terrain_classification_srv,
	unsigned int grid_cols,
	unsigned int grid_rows,
	float world_min_x,
	float world_min_z,
	float spacing,
	const TerrainSettings& terrain_settings)
{
	SafeRelease(m_seed_uav);
	SafeRelease(m_seed_srv);
	SafeRelease(m_seed_buffer);
	SafeRelease(m_instance_uav);
	SafeRelease(m_instance_srv);
	SafeRelease(m_instance_buffer);
	SafeRelease(m_args_buffer);
	SafeRelease(m_args_readback_buffer);
	m_grid_cols = 0;
	m_grid_rows = 0;
	m_instance_count = 0;
	m_last_visible_instance_count = 0;
	m_last_readback_time_seconds = -1000.0f;
	m_world_min_x = world_min_x;
	m_world_min_z = world_min_z;
	m_spacing = spacing;
	m_terrain_height_srv = terrain_height_srv;
	m_terrain_vegetation_suitability_srv = terrain_vegetation_suitability_srv;
	m_terrain_classification_srv = terrain_classification_srv;
	m_terrain_settings = terrain_settings;
	m_dispatch_dirty = true;
	m_last_error = "Configuring coverage";

	if (!IsValid() ||
		m_terrain_height_srv == nullptr ||
		m_terrain_vegetation_suitability_srv == nullptr ||
		m_terrain_classification_srv == nullptr ||
		grid_cols == 0 ||
		grid_rows == 0)
	{
		if (!IsValid())
		{
			m_last_error = "Compute grass system is not initialized";
		}
		else if (m_terrain_height_srv == nullptr)
		{
			m_last_error = "Terrain height SRV is null";
		}
		else if (m_terrain_vegetation_suitability_srv == nullptr)
		{
			m_last_error = "Terrain vegetation suitability SRV is null";
		}
		else if (m_terrain_classification_srv == nullptr)
		{
			m_last_error = "Terrain classification SRV is null";
		}
		else
		{
			m_last_error = "Coverage grid size is zero";
		}
		return false;
	}

	m_grid_cols = grid_cols;
	m_grid_rows = grid_rows;
	m_instance_count = m_grid_cols * m_grid_rows * kQuadsPerSeed;

	D3D11_BUFFER_DESC seed_desc{};
	seed_desc.Usage = D3D11_USAGE_DEFAULT;
	seed_desc.ByteWidth = static_cast<UINT>(sizeof(GrassSeedData) * m_grid_cols * m_grid_rows);
	seed_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	seed_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	seed_desc.StructureByteStride = sizeof(GrassSeedData);

	if (FAILED(m_device->CreateBuffer(&seed_desc, nullptr, &m_seed_buffer)))
	{
		hal::dout << "ComputeGrassInstances::ConfigureCoverage(): CreateBuffer(seed) failed" << std::endl;
		m_last_error = "CreateBuffer(seed) failed";
		Finalize();
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC seed_srv_desc{};
	seed_srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	seed_srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	seed_srv_desc.Buffer.FirstElement = 0;
	seed_srv_desc.Buffer.NumElements = m_grid_cols * m_grid_rows;

	if (FAILED(m_device->CreateShaderResourceView(m_seed_buffer, &seed_srv_desc, &m_seed_srv)))
	{
		hal::dout << "ComputeGrassInstances::ConfigureCoverage(): CreateShaderResourceView(seed) failed" << std::endl;
		m_last_error = "CreateShaderResourceView(seed) failed";
		Finalize();
		return false;
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC seed_uav_desc{};
	seed_uav_desc.Format = DXGI_FORMAT_UNKNOWN;
	seed_uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	seed_uav_desc.Buffer.FirstElement = 0;
	seed_uav_desc.Buffer.NumElements = m_grid_cols * m_grid_rows;

	if (FAILED(m_device->CreateUnorderedAccessView(m_seed_buffer, &seed_uav_desc, &m_seed_uav)))
	{
		hal::dout << "ComputeGrassInstances::ConfigureCoverage(): CreateUnorderedAccessView(seed) failed" << std::endl;
		m_last_error = "CreateUnorderedAccessView(seed) failed";
		Finalize();
		return false;
	}

	struct InstanceData
	{
		DirectX::XMFLOAT4 world0;
		DirectX::XMFLOAT4 world1;
		DirectX::XMFLOAT4 world2;
		DirectX::XMFLOAT4 world3;
		DirectX::XMFLOAT4 lodColor;
	};

	D3D11_BUFFER_DESC instance_desc{};
	instance_desc.Usage = D3D11_USAGE_DEFAULT;
	instance_desc.ByteWidth = static_cast<UINT>(sizeof(InstanceData) * m_instance_count);
	instance_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	instance_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	instance_desc.StructureByteStride = sizeof(InstanceData);

	if (FAILED(m_device->CreateBuffer(&instance_desc, nullptr, &m_instance_buffer)))
	{
		hal::dout << "ComputeGrassInstances::ConfigureCoverage(): CreateBuffer(instance) failed" << std::endl;
		m_last_error = "CreateBuffer(instance) failed";
		Finalize();
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC instance_srv_desc{};
	instance_srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	instance_srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	instance_srv_desc.Buffer.FirstElement = 0;
	instance_srv_desc.Buffer.NumElements = m_instance_count;

	if (FAILED(m_device->CreateShaderResourceView(m_instance_buffer, &instance_srv_desc, &m_instance_srv)))
	{
		hal::dout << "ComputeGrassInstances::ConfigureCoverage(): CreateShaderResourceView(instance) failed" << std::endl;
		m_last_error = "CreateShaderResourceView(instance) failed";
		Finalize();
		return false;
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC instance_uav_desc{};
	instance_uav_desc.Format = DXGI_FORMAT_UNKNOWN;
	instance_uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	instance_uav_desc.Buffer.FirstElement = 0;
	instance_uav_desc.Buffer.NumElements = m_instance_count;
	instance_uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;

	if (FAILED(m_device->CreateUnorderedAccessView(m_instance_buffer, &instance_uav_desc, &m_instance_uav)))
	{
		hal::dout << "ComputeGrassInstances::ConfigureCoverage(): CreateUnorderedAccessView(instance) failed" << std::endl;
		m_last_error = "CreateUnorderedAccessView(instance) failed";
		Finalize();
		return false;
	}

	D3D11_BUFFER_DESC args_desc{};
	args_desc.Usage = D3D11_USAGE_DEFAULT;
	args_desc.ByteWidth = sizeof(D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS);
	args_desc.BindFlags = 0;
	args_desc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

	const D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS args_init = { 6, 0, 0, 0, 0 };
	D3D11_SUBRESOURCE_DATA args_data{};
	args_data.pSysMem = &args_init;

	if (FAILED(m_device->CreateBuffer(&args_desc, &args_data, &m_args_buffer)))
	{
		hal::dout << "ComputeGrassInstances::ConfigureCoverage(): CreateBuffer(args) failed" << std::endl;
		m_last_error = "CreateBuffer(args) failed";
		Finalize();
		return false;
	}

	D3D11_BUFFER_DESC args_readback_desc{};
	args_readback_desc.Usage = D3D11_USAGE_STAGING;
	args_readback_desc.ByteWidth = sizeof(D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS);
	args_readback_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	if (FAILED(m_device->CreateBuffer(&args_readback_desc, nullptr, &m_args_readback_buffer)))
	{
		hal::dout << "ComputeGrassInstances::ConfigureCoverage(): CreateBuffer(args readback) failed" << std::endl;
		m_last_error = "CreateBuffer(args readback) failed";
		Finalize();
		return false;
	}

	m_dispatch_dirty = true;
	m_last_error = "Coverage configured";
	return true;
}

void ComputeGrassInstances::Update(
	float quad_scale_x,
	float quad_scale_y,
	const DirectX::XMFLOAT3& camera_position,
	const DirectX::XMMATRIX& view,
	const DirectX::XMMATRIX& proj,
	float current_time_seconds) const
{
	if (!HasSeeds())
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE mapped_resource{};
	if (SUCCEEDED(m_context->Map(m_constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource)))
	{
		auto* constants = static_cast<GrassInstanceConstants*>(mapped_resource.pData);
		DirectX::XMFLOAT4X4 view_proj{};
		DirectX::XMStoreFloat4x4(&view_proj, DirectX::XMMatrixTranspose(view * proj));
		const float active_water_height = TerrainDataModel::ActiveWaterSurfaceHeight();
		*constants = GrassInstanceConstants{
			view_proj,
			quad_scale_x,
			quad_scale_y,
			m_world_min_x,
			m_world_min_z,
			m_spacing,
			2.2f,
			active_water_height,
			camera_position.x,
			camera_position.z,
			kGrassLodFullDistance,
			kGrassLodMaxDistance,
			kGrassFarKeepProbability,
			m_grid_cols,
			m_grid_rows,
			kQuadsPerSeed,
			0u };
		m_context->Unmap(m_constant_buffer, 0);
	}

	const unsigned int candidate_count = m_grid_cols * m_grid_rows;

	if (m_dispatch_dirty)
	{
		ID3D11ShaderResourceView* coverage_srvs[] = {
			m_terrain_height_srv,
			m_terrain_vegetation_suitability_srv,
			m_terrain_classification_srv
		};
		m_context->CSSetShader(m_coverage_compute_shader, nullptr, 0);
		m_context->CSSetConstantBuffers(0, 1, &m_constant_buffer);
		m_context->CSSetShaderResources(0, 3, coverage_srvs);
		ID3D11SamplerState* sampler_state = Backend::DX11::Sampler::GetState();
		m_context->CSSetSamplers(0, 1, &sampler_state);
		m_context->CSSetUnorderedAccessViews(0, 1, &m_seed_uav, nullptr);
		m_context->Dispatch((candidate_count + kThreadGroupSize - 1) / kThreadGroupSize, 1, 1);

		ID3D11ShaderResourceView* null_srvs[] = { nullptr, nullptr, nullptr };
		ID3D11UnorderedAccessView* null_uav = nullptr;
		ID3D11Buffer* null_constant_buffer = nullptr;
		ID3D11SamplerState* null_sampler = nullptr;
		m_context->CSSetShaderResources(0, 3, null_srvs);
		m_context->CSSetSamplers(0, 1, &null_sampler);
		m_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
		m_context->CSSetConstantBuffers(0, 1, &null_constant_buffer);
		m_context->CSSetShader(nullptr, nullptr, 0);
		m_dispatch_dirty = false;
	}

	const D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS args_reset = { 6, 0, 0, 0, 0 };
	m_context->UpdateSubresource(m_args_buffer, 0, nullptr, &args_reset, 0, 0);

	m_context->CSSetShader(m_cull_compute_shader, nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, &m_constant_buffer);
	m_context->CSSetShaderResources(0, 1, &m_seed_srv);
	UINT initial_count = 0;
	m_context->CSSetUnorderedAccessViews(0, 1, &m_instance_uav, &initial_count);
	m_context->Dispatch((candidate_count + kThreadGroupSize - 1) / kThreadGroupSize, 1, 1);
	m_context->CopyStructureCount(m_args_buffer, sizeof(UINT), m_instance_uav);

	ID3D11ShaderResourceView* null_srv = nullptr;
	ID3D11UnorderedAccessView* null_uav = nullptr;
	ID3D11Buffer* null_constant_buffer = nullptr;
	m_context->CSSetShaderResources(0, 1, &null_srv);
	m_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
	m_context->CSSetConstantBuffers(0, 1, &null_constant_buffer);
	m_context->CSSetShader(nullptr, nullptr, 0);

	if (m_args_readback_buffer != nullptr &&
		current_time_seconds - m_last_readback_time_seconds >= 0.25f)
	{
		m_context->CopyResource(m_args_readback_buffer, m_args_buffer);

		D3D11_MAPPED_SUBRESOURCE readback_mapped_resource{};
		if (SUCCEEDED(m_context->Map(m_args_readback_buffer, 0, D3D11_MAP_READ, 0, &readback_mapped_resource)))
		{
			const auto* args =
				static_cast<const D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS*>(readback_mapped_resource.pData);
			m_last_visible_instance_count = args->InstanceCount;
			m_last_readback_time_seconds = current_time_seconds;
			m_context->Unmap(m_args_readback_buffer, 0);
		}
	}
}

bool ComputeGrassInstances::IsValid() const
{
	return m_coverage_compute_shader != nullptr &&
		   m_cull_compute_shader != nullptr &&
		   m_constant_buffer != nullptr &&
		   m_device != nullptr &&
		   m_context != nullptr;
}

bool ComputeGrassInstances::HasSeeds() const
{
	return IsValid() &&
		   m_terrain_height_srv != nullptr &&
		   m_terrain_vegetation_suitability_srv != nullptr &&
		   m_terrain_classification_srv != nullptr &&
		   m_seed_buffer != nullptr &&
		   m_seed_srv != nullptr &&
		   m_seed_uav != nullptr &&
		   m_instance_buffer != nullptr &&
		   m_instance_srv != nullptr &&
		   m_instance_uav != nullptr &&
		   m_instance_count > 0;
}

void ComputeGrassInstances::MarkCoverageDirty()
{
	m_dispatch_dirty = true;
}

ID3D11Buffer* ComputeGrassInstances::InstanceBuffer() const
{
	return m_instance_buffer;
}

ID3D11ShaderResourceView* ComputeGrassInstances::InstanceSRV() const
{
	return m_instance_srv;
}

ID3D11Buffer* ComputeGrassInstances::ArgsBuffer() const
{
	return m_args_buffer;
}

unsigned int ComputeGrassInstances::InstanceStride() const
{
	return static_cast<unsigned int>(sizeof(DirectX::XMFLOAT4) * 5u);
}

unsigned int ComputeGrassInstances::InstanceCount() const
{
	return m_instance_count;
}

unsigned int ComputeGrassInstances::GridCols() const
{
	return m_grid_cols;
}

unsigned int ComputeGrassInstances::GridRows() const
{
	return m_grid_rows;
}

unsigned int ComputeGrassInstances::SeedCount() const
{
	return m_grid_cols * m_grid_rows;
}

unsigned int ComputeGrassInstances::LastVisibleInstanceCount() const
{
	return m_last_visible_instance_count;
}

bool ComputeGrassInstances::DispatchDirty() const
{
	return m_dispatch_dirty;
}

const char* ComputeGrassInstances::LastError() const
{
	return m_last_error.c_str();
}
