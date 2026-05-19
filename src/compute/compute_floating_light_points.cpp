#include "compute_floating_light_points.h"

#include <fstream>
#include <vector>

#include "debug_ostream.h"
#include "sampler.h"

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
}

bool ComputeFloatingLightPoints::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	Finalize();

	m_device = device;
	m_context = context;
	if (m_device == nullptr || m_context == nullptr)
	{
		return false;
	}

	std::ifstream shader_stream("resource/shader/shader_compute_floating_light_points.cso", std::ios::binary);
	if (!shader_stream)
	{
		hal::dout << "ComputeFloatingLightPoints::Initialize(): failed to open shader_compute_floating_light_points.cso" << std::endl;
		return false;
	}

	const std::vector<char> shader_bytes((std::istreambuf_iterator<char>(shader_stream)),
										 std::istreambuf_iterator<char>());
	if (shader_bytes.empty())
	{
		return false;
	}

	if (FAILED(m_device->CreateComputeShader(shader_bytes.data(), shader_bytes.size(), nullptr, &m_compute_shader)))
	{
		hal::dout << "ComputeFloatingLightPoints::Initialize(): CreateComputeShader failed" << std::endl;
		Finalize();
		return false;
	}

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.ByteWidth = sizeof(FloatingLightConstants);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(m_device->CreateBuffer(&buffer_desc, nullptr, &m_constant_buffer)))
	{
		hal::dout << "ComputeFloatingLightPoints::Initialize(): CreateBuffer(constant) failed" << std::endl;
		Finalize();
		return false;
	}

	return true;
}

void ComputeFloatingLightPoints::Finalize()
{
	m_seed_count = 0;

	SafeRelease(m_constant_buffer);
	SafeRelease(m_instance_uav);
	SafeRelease(m_instance_draw_buffer);
	SafeRelease(m_instance_compute_buffer);
	SafeRelease(m_seed_srv);
	SafeRelease(m_seed_buffer);
	SafeRelease(m_compute_shader);

	m_device = nullptr;
	m_context = nullptr;
}

bool ComputeFloatingLightPoints::SetSeeds(const std::vector<Seed>& seeds)
{
	SafeRelease(m_instance_uav);
	SafeRelease(m_instance_draw_buffer);
	SafeRelease(m_instance_compute_buffer);
	SafeRelease(m_seed_srv);
	SafeRelease(m_seed_buffer);
	m_seed_count = 0;

	if (!IsValid() || seeds.empty())
	{
		return false;
	}

	m_seed_count = static_cast<unsigned int>(seeds.size());

	D3D11_BUFFER_DESC seed_desc{};
	seed_desc.Usage = D3D11_USAGE_IMMUTABLE;
	seed_desc.ByteWidth = static_cast<UINT>(sizeof(Seed) * seeds.size());
	seed_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	seed_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	seed_desc.StructureByteStride = sizeof(Seed);

	D3D11_SUBRESOURCE_DATA seed_data{};
	seed_data.pSysMem = seeds.data();

	if (FAILED(m_device->CreateBuffer(&seed_desc, &seed_data, &m_seed_buffer)))
	{
		hal::dout << "ComputeFloatingLightPoints::SetSeeds(): CreateBuffer(seed) failed" << std::endl;
		Finalize();
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC seed_srv_desc{};
	seed_srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	seed_srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	seed_srv_desc.Buffer.FirstElement = 0;
	seed_srv_desc.Buffer.NumElements = m_seed_count;

	if (FAILED(m_device->CreateShaderResourceView(m_seed_buffer, &seed_srv_desc, &m_seed_srv)))
	{
		hal::dout << "ComputeFloatingLightPoints::SetSeeds(): CreateShaderResourceView(seed) failed" << std::endl;
		Finalize();
		return false;
	}

	struct InstanceData
	{
		DirectX::XMFLOAT4 world0;
		DirectX::XMFLOAT4 world1;
		DirectX::XMFLOAT4 world2;
		DirectX::XMFLOAT4 world3;
		DirectX::XMFLOAT4 color;
	};

	D3D11_BUFFER_DESC instance_desc{};
	instance_desc.Usage = D3D11_USAGE_DEFAULT;
	instance_desc.ByteWidth = static_cast<UINT>(sizeof(InstanceData) * m_seed_count);
	instance_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	instance_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	instance_desc.StructureByteStride = sizeof(InstanceData);

	if (FAILED(m_device->CreateBuffer(&instance_desc, nullptr, &m_instance_compute_buffer)))
	{
		hal::dout << "ComputeFloatingLightPoints::SetSeeds(): CreateBuffer(instance compute) failed" << std::endl;
		Finalize();
		return false;
	}

	D3D11_BUFFER_DESC draw_desc{};
	draw_desc.Usage = D3D11_USAGE_DEFAULT;
	draw_desc.ByteWidth = static_cast<UINT>(sizeof(InstanceData) * m_seed_count);
	draw_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	if (FAILED(m_device->CreateBuffer(&draw_desc, nullptr, &m_instance_draw_buffer)))
	{
		hal::dout << "ComputeFloatingLightPoints::SetSeeds(): CreateBuffer(instance draw) failed" << std::endl;
		Finalize();
		return false;
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC instance_uav_desc{};
	instance_uav_desc.Format = DXGI_FORMAT_UNKNOWN;
	instance_uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	instance_uav_desc.Buffer.FirstElement = 0;
	instance_uav_desc.Buffer.NumElements = m_seed_count;

	if (FAILED(m_device->CreateUnorderedAccessView(m_instance_compute_buffer, &instance_uav_desc, &m_instance_uav)))
	{
		hal::dout << "ComputeFloatingLightPoints::SetSeeds(): CreateUnorderedAccessView(instance) failed" << std::endl;
		Finalize();
		return false;
	}

	return true;
}

void ComputeFloatingLightPoints::Update(
	float time_seconds,
	ID3D11ShaderResourceView* terrain_height_srv,
	ID3D11ShaderResourceView* meteorograph_srv,
	const DirectX::XMFLOAT3& camera_position,
	const DirectX::XMFLOAT3& camera_forward,
	const DirectX::XMFLOAT3& camera_right,
	const DirectX::XMFLOAT3& camera_up) const
{
	if (!HasSeeds())
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE mapped_resource{};
	if (SUCCEEDED(m_context->Map(m_constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource)))
	{
		auto* constants = static_cast<FloatingLightConstants*>(mapped_resource.pData);
		*constants = FloatingLightConstants{
			time_seconds,
			-640.0f,
			640.0f,
			-640.0f,
			640.0f,
			m_seed_count,
			0.0f,
			0.0f,
			0.0f,
			{ camera_position.x, camera_position.y, camera_position.z, 0.0f },
			{ camera_forward.x, camera_forward.y, camera_forward.z, 0.0f },
			{ camera_right.x, camera_right.y, camera_right.z, 0.0f },
			{ camera_up.x, camera_up.y, camera_up.z, 0.0f } };
		m_context->Unmap(m_constant_buffer, 0);
	}

	ID3D11ShaderResourceView* input_srvs[3] = {
		m_seed_srv,
		terrain_height_srv,
		meteorograph_srv
	};

	m_context->CSSetShader(m_compute_shader, nullptr, 0);
	m_context->CSSetConstantBuffers(0, 1, &m_constant_buffer);
	m_context->CSSetShaderResources(0, 3, input_srvs);
	m_context->CSSetUnorderedAccessViews(0, 1, &m_instance_uav, nullptr);
	m_context->Dispatch((m_seed_count + kThreadGroupSize - 1) / kThreadGroupSize, 1, 1);

	ID3D11ShaderResourceView* null_srvs[3] = { nullptr, nullptr, nullptr };
	ID3D11UnorderedAccessView* null_uav = nullptr;
	ID3D11Buffer* null_constant_buffer = nullptr;
	m_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
	m_context->CSSetShaderResources(0, 3, null_srvs);
	m_context->CSSetConstantBuffers(0, 1, &null_constant_buffer);
	m_context->CSSetShader(nullptr, nullptr, 0);

	if (m_instance_draw_buffer != nullptr && m_instance_compute_buffer != nullptr)
	{
		m_context->CopyResource(m_instance_draw_buffer, m_instance_compute_buffer);
	}
}

bool ComputeFloatingLightPoints::IsValid() const
{
	return m_compute_shader != nullptr &&
		   m_constant_buffer != nullptr &&
		   m_device != nullptr &&
		   m_context != nullptr;
}

bool ComputeFloatingLightPoints::HasSeeds() const
{
	return IsValid() &&
		   m_seed_buffer != nullptr &&
		   m_seed_srv != nullptr &&
		   m_instance_compute_buffer != nullptr &&
		   m_instance_draw_buffer != nullptr &&
		   m_instance_uav != nullptr &&
		   m_seed_count > 0;
}

ID3D11Buffer* ComputeFloatingLightPoints::InstanceBuffer() const
{
	return m_instance_draw_buffer;
}

unsigned int ComputeFloatingLightPoints::InstanceCount() const
{
	return m_seed_count;
}
