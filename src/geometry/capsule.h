// ----------------------------------------------------
// 繧ｫ繝励そ繝ｫ [capsule.h]
// ====================================================

#ifndef CAPSULE_H
#define CAPSULE_H

#include <DirectXMath.h>
#include <d3d11.h>
#include "material_type.h"


void Capsule_Initialize(ID3D11Device *pDevice, ID3D11DeviceContext *pContext);
void Capsule_Finalize(void);
void Capsule_Update(double elapsed_Time);
void Capsule_Draw(int texId,
                  const DirectX::XMMATRIX &mtxWorld,
                  MaterialType material_type = MaterialType::Lit);
void Capsule_DrawShadow(const DirectX::XMMATRIX &mtxWorld);

#endif // CAPSULE_H

