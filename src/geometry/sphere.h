#ifndef SPHERE_H
#define SPHERE_H

#include <DirectXMath.h>
#include <d3d11.h>
#include "material_type.h"


void Sphere_Initialize(ID3D11Device *pDevice, ID3D11DeviceContext *pContext);
void Sphere_Finalize(void);
void Sphere_Draw(int texId,
                 const DirectX::XMMATRIX &mtxWorld,
                 MaterialType material_type = MaterialType::Lit);
void Sphere_DrawShadow(const DirectX::XMMATRIX &mtxWorld);

#endif // SPHERE_H

