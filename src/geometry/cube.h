// ----------------------------------------------------
// 3D�L���[�u�̕\�� [cube.h]
// ====================================================
// Created by: Jerry
// Date: 2025-09-09
// Version: 1.0
// ----------------------------------------------------
#ifndef CUBE_H
#define CUBE_H

#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include "collision.h"
#include "material_type.h"
#include "render_state.h"

void Cube_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Cube_Finalize(void);
void Cube_Update(double elapsed_Time);
void Cube_Draw(int texId, const DirectX::XMMATRIX& mtxWorld);
void Cube_DrawMaterial(int texId,
					   const DirectX::XMMATRIX& mtxWorld,
					   const DirectX::XMFLOAT4& material_color,
					   RenderState render_state,
					   MaterialType material_type = MaterialType::Lit);
void Cube_DrawMaterialSRV(ID3D11ShaderResourceView* texture_srv,
						  const DirectX::XMMATRIX& mtxWorld,
						  const DirectX::XMFLOAT4& material_color,
						  RenderState render_state,
						  MaterialType material_type = MaterialType::Lit);
void Cube_DrawInstanced(int texId,
						const std::vector<DirectX::XMFLOAT4X4>& world_matrices,
						const DirectX::XMFLOAT4& material_color,
						RenderState render_state);
void Cube_DrawGrassInstanced(int texId,
							 const std::vector<DirectX::XMFLOAT4X4>& world_matrices,
							 const DirectX::XMFLOAT4& material_color,
							 ID3D11ShaderResourceView* wind_field_srv,
							 float time_seconds,
							 float field_uv_scale,
							 float bend_scale,
							 RenderState render_state);
void Cube_DrawShadow(const DirectX::XMMATRIX& mtxWorld);

Collision::AABB Cube_GetAABB(const DirectX::XMFLOAT3& position);

#endif // CUBE_H
