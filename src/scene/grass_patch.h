#ifndef GRASS_PATCH_H
#define GRASS_PATCH_H

#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>

#include "render_state.h"

void GrassPatch_Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
void GrassPatch_Finalize();

void GrassPatch_DrawInstanced(int tex_id,
                              const std::vector<DirectX::XMFLOAT4X4>& world_matrices,
                              const DirectX::XMFLOAT4& material_color,
                              ID3D11ShaderResourceView* wind_field_srv,
                              float time_seconds,
                              float field_uv_scale,
                              float bend_scale,
                              RenderState render_state);

void GrassPatch_DrawInstancedIndirect(int tex_id,
                                      ID3D11Buffer* instance_buffer,
                                      unsigned int instance_stride,
                                      ID3D11Buffer* args_buffer,
                                      const DirectX::XMFLOAT4& material_color,
                                      ID3D11ShaderResourceView* wind_field_srv,
                                      float time_seconds,
                                      float field_uv_scale,
                                      float bend_scale,
                                      RenderState render_state);

void GrassPatch_DrawShadow(const DirectX::XMMATRIX& world_matrix);

#endif // GRASS_PATCH_H
