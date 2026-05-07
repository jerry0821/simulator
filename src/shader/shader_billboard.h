/*==============================================================================

   シェーダービルボード [shader_billboard.h]
														 Author : Youhei Sato
														 Date   : 2025/11/14
--------------------------------------------------------------------------------

==============================================================================*/
#ifndef SHADERBILLBOARD_H
#define	SHADERBILLBOARD_H

#include <d3d11.h>
#include <DirectXMath.h>

struct UVParameter
{
	DirectX::XMFLOAT2 scale;
	DirectX::XMFLOAT2 translation;
};

bool ShaderBillBoard_Initialize();
void ShaderBillBoard_Finalize();
		   
void ShaderBillBoard_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
		   
void ShaderBillBoard_SetMaterialColor(const DirectX::XMFLOAT4& material_color);
		   
void ShaderBillBoard_SetUVParameter(const UVParameter& parameter);


void ShaderBillBoard_Begin();

#endif // SHADERBILLBOARD_H
