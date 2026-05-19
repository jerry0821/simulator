#ifndef COMPUTE_FLOATING_LIGHT_POINTS_H
#define COMPUTE_FLOATING_LIGHT_POINTS_H

#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>

#include "compute_task.h"
#include "render_shadow_map_resource.h"

class ComputeFloatingLightPoints : public ComputeTask
{
public:
	struct Seed
	{
		DirectX::XMFLOAT4 base_position_size{};
		DirectX::XMFLOAT4 params{};
	};

	~ComputeFloatingLightPoints() override = default;

	const char* DebugName() const override
	{
		return "ComputeFloatingLightPoints";
	}

	ComputeTaskDispatchMode DispatchMode() const override
	{
		return ComputeTaskDispatchMode::EveryFrame;
	}

	ResourceSpan ReadResources() const override
	{
		static constexpr ComputeSharedResourceId kReadResources[] = {
			ComputeSharedResourceId::TerrainHeight,
			ComputeSharedResourceId::MeteorographField
		};
		return kReadResources;
	}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
	void Finalize() override;

	bool SetSeeds(const std::vector<Seed>& seeds);
	void Update(
		float time_seconds,
		ID3D11ShaderResourceView* terrain_height_srv,
		ID3D11ShaderResourceView* meteorograph_srv,
		const DirectX::XMFLOAT3& camera_position,
		const DirectX::XMFLOAT3& camera_forward,
		const DirectX::XMFLOAT3& camera_right,
		const DirectX::XMFLOAT3& camera_up) const;

	bool IsValid() const override;
	bool HasSeeds() const;

	ID3D11Buffer* InstanceBuffer() const;
	unsigned int InstanceCount() const;

private:
	struct FloatingLightConstants
	{
		float time_seconds = 0.0f;
		float world_min_x = -640.0f;
		float world_max_x = 640.0f;
		float world_min_z = -640.0f;
		float world_max_z = 640.0f;
		unsigned int seed_count = 0;
		float padding0 = 0.0f;
		float padding1 = 0.0f;
		float padding2 = 0.0f;
		DirectX::XMFLOAT4 camera_position = { 0.0f, 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 camera_forward = { 0.0f, 0.0f, 1.0f, 0.0f };
		DirectX::XMFLOAT4 camera_right = { 1.0f, 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 camera_up = { 0.0f, 1.0f, 0.0f, 0.0f };
	};

	static constexpr unsigned int kThreadGroupSize = 64;

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;
	ID3D11ComputeShader* m_compute_shader = nullptr;
	ID3D11Buffer* m_seed_buffer = nullptr;
	ID3D11ShaderResourceView* m_seed_srv = nullptr;
	ID3D11Buffer* m_instance_compute_buffer = nullptr;
	ID3D11Buffer* m_instance_draw_buffer = nullptr;
	ID3D11UnorderedAccessView* m_instance_uav = nullptr;
	ID3D11Buffer* m_constant_buffer = nullptr;
	unsigned int m_seed_count = 0;
};

#endif // COMPUTE_FLOATING_LIGHT_POINTS_H
