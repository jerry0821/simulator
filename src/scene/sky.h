// ----------------------------------------------------
// 空の表示 [sky.h]
// ====================================================
// Created by: Jerry
// Date: 2025-11-21
// ----------------------------------------------------
#ifndef SKY_H
#define SKY_H

#include <DirectXMath.h>

void Sky_Initialize();
void Sky_Finalize();
void Sky_SetPosition(const DirectX::XMFLOAT3& position);
void Sky_Draw();

#endif // SKY_H
