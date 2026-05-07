#ifndef COMPUTE_FINAL_TERRAIN_HEIGHT_TEXTURE_H
#define COMPUTE_FINAL_TERRAIN_HEIGHT_TEXTURE_H

#include <vector>

#include "compute_task.h"
#include "render_shadow_map_resource.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ComputeShader;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11Buffer;

class ComputeFinalTerrainHeightTexture : public ComputeTask
{
public:
	ComputeFinalTerrainHeightTexture() = default;
	~ComputeFinalTerrainHeightTexture() override = default;

	const char* DebugName() const override
	{
		return "ComputeFinalTerrainHeightTexture";
	}

	ComputeTaskDispatchMode DispatchMode() const override
	{
		return ComputeTaskDispatchMode::Manual;
	}

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
	void Finalize() override;
	void Update(
		ID3D11ShaderResourceView* base_terrain_height_srv,
		ID3D11ShaderResourceView* erosion_delta_srv);
	bool IsValid() const override;
	Backend::RenderShaderResource Resource() const;
	bool HasCpuHeightData() const;
	float SampleHeightWorld(float world_x, float world_z, float field_width, float field_depth) const;

private:
	struct FinalTerrainHeightConstants
	{
		float erosion_strength = 0.18f;
		float deposition_strength = 0.10f;
		float min_height = -32.0f;
		float max_height = 96.0f;
		float min_delta = -0.80f;
		float max_delta = 0.45f;
		unsigned int width = 0;
		unsigned int height = 0;
	};

	static constexpr unsigned int kTextureWidth = 257;
	static constexpr unsigned int kTextureHeight = 257;
	static constexpr unsigned int kThreadGroupSize = 8;

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;
	ID3D11ComputeShader* m_compute_shader = nullptr;
	ID3D11Texture2D* m_texture = nullptr;
	ID3D11Texture2D* m_readback_texture = nullptr;
	ID3D11ShaderResourceView* m_srv = nullptr;
	ID3D11UnorderedAccessView* m_uav = nullptr;
	ID3D11Buffer* m_constant_buffer = nullptr;
	std::vector<float> m_height_samples{};
	bool m_cpu_height_data_ready = false;
};

#endif // COMPUTE_FINAL_TERRAIN_HEIGHT_TEXTURE_H
