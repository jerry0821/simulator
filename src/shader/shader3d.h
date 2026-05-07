/*==============================================================================

   シェーダー3D [shader3d.h]
														 Author : Youhei Sato
														 Date   : 2025/05/15
--------------------------------------------------------------------------------

==============================================================================*/
#ifndef SHADER3D_H
#define	SHADER3D_H

#include <d3d11.h>
#include <DirectXMath.h>

struct Vertex33
{
	DirectX::XMFLOAT3 position;	// 座標
	DirectX::XMFLOAT3 normal;	// 法線
	DirectX::XMFLOAT4 color;	// 顏色
	DirectX::XMFLOAT2 texcoord;	// UV
};

bool Shader3D_Initialize();
void Shader3D_Finalize();
		   
void Shader3D_SetWorldMatrix(const DirectX::XMMATRIX& matrix);

void Shader3D_SetViewMatrix(const DirectX::XMMATRIX& matrix);

void Shader3D_SetProjMatrix(const DirectX::XMMATRIX& matrix);
	   
void Shader3D_SetMaterialColor(const DirectX::XMFLOAT4& material_color);

void Shader3D_Begin();

#endif // SHADER3D_H
