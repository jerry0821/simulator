// ----------------------------------------------------
// 3D sprite shadow instanced shader [shader_sprite3d_shadow_instanced.h]
// ====================================================
#ifndef SHADER_SPRITE3D_SHADOW_INSTANCED_H
#define SHADER_SPRITE3D_SHADOW_INSTANCED_H

#include <d3d11.h>
#include <DirectXMath.h>

bool ShaderSprite3D_ShadowInstanced_Initialize();
void ShaderSprite3D_ShadowInstanced_Finalize();

void ShaderSprite3D_ShadowInstanced_SetLightViewProjection(const DirectX::XMMATRIX& matrix);
void ShaderSprite3D_ShadowInstanced_SetWindField(ID3D11ShaderResourceView* wind_field_srv);
void ShaderSprite3D_ShadowInstanced_SetInstanceBuffer(ID3D11ShaderResourceView* instance_buffer_srv);
void ShaderSprite3D_ShadowInstanced_SetWindSettings(float time_seconds,
                                                    const DirectX::XMFLOAT2& wind_direction,
                                                    float wind_strength,
                                                    float world_min_x,
                                                    float world_max_x,
                                                    float world_min_z,
                                                    float world_max_z);
void ShaderSprite3D_ShadowInstanced_Begin();
void ShaderSprite3D_ShadowInstanced_Clear();

#endif // SHADER_SPRITE3D_SHADOW_INSTANCED_H
