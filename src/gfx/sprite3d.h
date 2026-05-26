// --------------------------------------------
// 3D image quad [sprite3d.h]
// ============================================
#ifndef SPRITE3D_H
#define SPRITE3D_H

#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>

void Sprite3D_Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
void Sprite3D_Finalize();

void Sprite3D_Draw(
	int texid,
	const DirectX::XMMATRIX& world_matrix,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

void Sprite3D_DrawTransparent(
	int texid,
	const DirectX::XMMATRIX& world_matrix,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

void Sprite3D_DrawTransparentSRV(
	ID3D11ShaderResourceView* texture_srv,
	const DirectX::XMMATRIX& world_matrix,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

void Sprite3D_DrawAdditiveSRV(
	ID3D11ShaderResourceView* texture_srv,
	const DirectX::XMMATRIX& world_matrix,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

void Sprite3D_DrawWaterSRV(
	ID3D11ShaderResourceView* terrain_height_srv,
	ID3D11ShaderResourceView* water_velocity_srv,
	ID3D11ShaderResourceView* water_sediment_srv,
	ID3D11ShaderResourceView* terrain_normal_srv,
	ID3D11ShaderResourceView* scene_depth_srv,
	const DirectX::XMMATRIX& world_matrix,
	const DirectX::XMFLOAT4& color,
	const DirectX::XMFLOAT3& camera_position,
	float highlight_strength = 0.0f);

void Sprite3D_DrawTransparentInstanced(
	int texid,
	const std::vector<DirectX::XMFLOAT4X4>& world_matrices,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

void Sprite3D_DrawTransparentInstancedColored(
	int texid,
	const std::vector<DirectX::XMFLOAT4X4>& world_matrices,
	const std::vector<DirectX::XMFLOAT4>& instance_colors,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

void Sprite3D_DrawAdditiveInstancedColored(
	int texid,
	const std::vector<DirectX::XMFLOAT4X4>& world_matrices,
	const std::vector<DirectX::XMFLOAT4>& instance_colors,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

void Sprite3D_DrawTransparentInstancedBuffer(
	int texid,
	ID3D11Buffer* instance_buffer,
	unsigned int instance_count,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

void Sprite3D_DrawAdditiveInstancedBuffer(
	int texid,
	ID3D11Buffer* instance_buffer,
	unsigned int instance_count,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

void Sprite3D_DrawAdditiveInstancedBufferOverlay(
	int texid,
	ID3D11Buffer* instance_buffer,
	unsigned int instance_count,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

void Sprite3D_DrawCutout(
	int texid,
	const DirectX::XMMATRIX& world_matrix,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

void Sprite3D_DrawCutoutInstanced(
	int texid,
	const std::vector<DirectX::XMFLOAT4X4>& world_matrices,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

void Sprite3D_DrawCutoutInstancedBuffer(
	int texid,
	ID3D11ShaderResourceView* instance_srv,
	unsigned int instance_count,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

void Sprite3D_DrawCutoutInstancedIndirectBuffer(
	int texid,
	ID3D11ShaderResourceView* instance_srv,
	ID3D11Buffer* args_buffer,
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

void Sprite3D_DrawCutoutShadowInstancedIndirectBuffer(
	int texid,
	ID3D11ShaderResourceView* instance_srv,
	ID3D11Buffer* args_buffer);

#endif // SPRITE3D_H
