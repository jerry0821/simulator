#ifndef CYLINDER_H
#define CYLINDER_H

#include <DirectXMath.h>
#include <d3d11.h>
#include "material_type.h"


void Cylinder_Initialize(ID3D11Device *pDevice, ID3D11DeviceContext *pContext);
void Cylinder_Finalize(void);
void Cylinder_Draw(int texId,
                   const DirectX::XMMATRIX &mtxWorld,
                   MaterialType material_type = MaterialType::Lit);
void Cylinder_DrawShadow(const DirectX::XMMATRIX &mtxWorld);

#endif // CYLINDER_H

