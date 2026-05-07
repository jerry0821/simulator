// --------------------------------------------
// ビルボード [billboard.h]
// ============================================
// Created by: Jerry
// Date: 2025-11-14
//---------------------------------------------
#include "billboard.h"
#include "direct3d.h"
#include <DirectXMath.h>
using namespace DirectX;
#include "shader_billboard.h"
#include "texture.h"
#include "camera.h"

static constexpr int NUM_VERTEX = 4; // 頂点

static ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ

static XMFLOAT4X4 g_mtxView{}; //ビュー行列の平行移動成分をカットした行列

// 頂点構造体
struct Vertex_Billboard
{
	XMFLOAT3 position;	// 頂点座標
	XMFLOAT4 color;		// 色
	XMFLOAT2 texcoord;	// テクスチャーUV
};


void Billboard_Initialize()
{
	ShaderBillBoard_Initialize();

	static Vertex_Billboard vertex[]{
	{{ -0.5f,  0.5f, 0.0f}, { 1.0f, 1.0f, 1.0f, 1.0f},{  0.0f,  0.0f}}, // 左上
	{{  0.5f,  0.5f, 0.0f}, { 1.0f, 1.0f, 1.0f, 1.0f},{  1.0f,  0.0f}}, // 右上
	{{ -0.5f, -0.5f, 0.0f}, { 1.0f, 1.0f, 1.0f, 1.0f},{  0.0f,  1.0f}}, // 左下
	{{  0.5f, -0.5f, 0.0f}, { 1.0f, 1.0f, 1.0f, 1.0f},{  1.0f,  1.0f}}, // 右下
	};



	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex_Billboard) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	Direct3D_GetDevice()->CreateBuffer(&bd, &sd, &g_pVertexBuffer);

}

void Billboard_Finalize(void)
{
	SAFE_RELEASE(g_pVertexBuffer); // 頂点バッファの解放
}

void BillBoard_SetViewMatrix(const DirectX::XMFLOAT4X4& view)
{
	//　カメラ行列の平行移動成分をカット
	g_mtxView = view;
	g_mtxView._41 = g_mtxView._42 = g_mtxView._43 = 0.0f;
}

void Billboard_Draw(int texId, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT2& scale, const DirectX::XMFLOAT2& pivot)
{
	//ShaderBillBoard_SetUVParameter({{ 0.25f, 0.5f }, { 0.25f * 2, 0.5f }});
	// シェーダーを描画パイプラインに設定
	ShaderBillBoard_Begin();

	ShaderBillBoard_SetUVParameter({ { 1.0f, 1.0f }, { 0.0f, 0.0f } });

	ShaderBillBoard_SetMaterialColor({ 0.7f, 0.7f, 0.7f, 1.0f });

	// テクスチャの設定
	Texture_SetTexture(texId);


	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex_Billboard);
	UINT offset = 0;
	Direct3D_GetContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// プリミティブトポロジ設定
	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// ワールド座標変換行列の作成


	// 頂点シェーダーにワールド座標変換行列を設定
	// 回転軸までのオフセット行列
	XMMATRIX pivot_offset = XMMatrixTranslation(-pivot.x, -pivot.y, 0.0f);
	//　カメラ行列の回転だけ逆行列を作る
	//XMMATRIX iv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&mtxCamera));
	XMMATRIX iv = XMMatrixTranspose(XMLoadFloat4x4(&g_mtxView));


	//正方直交行列の逆行列は転置行列と等しい


	XMMATRIX s = XMMatrixScaling(scale.x, scale.y, 1.0f);
	XMMATRIX t = XMMatrixTranslation(position.x, position.y, position.z);
	//XMMATRIX t = XMMatrixTranslation(position.x + pivot.x, position.y + pivot.y, position.z);

	ShaderBillBoard_SetWorldMatrix(s * pivot_offset * iv * t);
	//ShaderBillBoard_SetWorldMatrix(pivot_offset * s * iv * t);


	// ポリゴン描画命令発行
	Direct3D_GetContext()->Draw(NUM_VERTEX, 0);

}

void Billboard_Draw(int texId, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT2& scale, const DirectX::XMFLOAT4& text_cut, const DirectX::XMFLOAT2& pivot)
{
	ShaderBillBoard_Begin();

	//ShaderBillBoard_SetUVParameter({ { 1.0f, 1.0f }, { 0.0f , 0.0f } });

	float uv_x = (float)text_cut.x / Texture_Width(texId);
	float uv_y = (float)text_cut.y / Texture_Height(texId);
	float uv_w = (float)text_cut.z / Texture_Width(texId);
	float uv_h = (float)text_cut.w / Texture_Height(texId);

	ShaderBillBoard_SetUVParameter({ { uv_w, uv_h }, { uv_x, uv_y } });
	// シェーダーを描画パイプラインに設定


	ShaderBillBoard_SetMaterialColor({ 1.0f, 1.0f, 1.0f, 1.0f }); // 白色

	// テクスチャの設定
	Texture_SetTexture(texId);


	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex_Billboard);
	UINT offset = 0;
	Direct3D_GetContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// プリミティブトポロジ設定
	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// ワールド座標変換行列の作成


	// 頂点シェーダーにワールド座標変換行列を設定
	// 回転軸までのオフセット行列
	XMMATRIX pivot_offset = XMMatrixTranslation(-pivot.x, -pivot.y, 0.0f);
	//　カメラ行列の回転だけ逆行列を作る
	XMFLOAT4X4 mtxCamera = Camera_GetMatrix();
	mtxCamera._41 = mtxCamera._42 = mtxCamera._43 = 0.0f;
	//XMMATRIX iv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&mtxCamera));
	XMMATRIX iv = XMMatrixTranspose(XMLoadFloat4x4(&g_mtxView));

	//正方直交行列の逆行列は転置行列と等しい


	XMMATRIX s = XMMatrixScaling(scale.x, scale.y, 1.0f);
	XMMATRIX t = XMMatrixTranslation(position.x, position.y, position.z);
	//XMMATRIX t = XMMatrixTranslation(position.x + pivot.x, position.y + pivot.y, position.z);

	ShaderBillBoard_SetWorldMatrix(XMMatrixTranslation(-pivot.x, -pivot.y, 0.0f) * s * iv * t);
	//ShaderBillBoard_SetWorldMatrix(pivot_offset * s * iv * t);


	// ポリゴン描画命令発行
	Direct3D_GetContext()->Draw(NUM_VERTEX, 0);

}

void Billboard_Draw(int texId, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT2& scale, const DirectX::XMFLOAT4& uv_rect, const DirectX::XMFLOAT4& color)
{
	ShaderBillBoard_Begin();

	// 1. 設定 UV (支援切圖，如果不切圖傳 {0,0,1,1} 即可)
	ShaderBillBoard_SetUVParameter({ { uv_rect.z, uv_rect.w }, { uv_rect.x, uv_rect.y } });

	// 2. 【關鍵】使用傳進來的顏色 (包含透明度)
	ShaderBillBoard_SetMaterialColor(color);

	Texture_SetTexture(texId);

	UINT stride = sizeof(Vertex_Billboard);
	UINT offset = 0;
	Direct3D_GetContext()->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// 矩陣計算
	XMMATRIX iv = XMMatrixTranspose(XMLoadFloat4x4(&g_mtxView));
	XMMATRIX s = XMMatrixScaling(scale.x, scale.y, 1.0f);
	XMMATRIX t = XMMatrixTranslation(position.x, position.y, position.z);

	// P.S. 特效通常不需要 Pivot，所以這裡簡化計算
	ShaderBillBoard_SetWorldMatrix(s * iv * t);

	Direct3D_GetContext()->Draw(NUM_VERTEX, 0);
}

void Billboard_DrawAnim(int texId, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT2& scale, int currentFrame, int splitX, int splitY, const DirectX::XMFLOAT4& color)
{
	float texW = (float)Texture_Width(texId);
	float texH = (float)Texture_Height(texId);

	float cellW = texW / splitX;
	float cellH = texH / splitY;

	int col = currentFrame % splitX;
	int row = currentFrame / splitX;

	XMFLOAT4 cutRect;
	cutRect.x = col * cellW; // X 起點
	cutRect.y = row * cellH; // Y 起點
	cutRect.z = cellW;       // 寬度
	cutRect.w = cellH;       // 高度


	XMFLOAT4 uv_rect;
	uv_rect.x = cutRect.x / texW; // u min
	uv_rect.y = cutRect.y / texH; // v min
	uv_rect.z = cellW / texW;     // u size (width)
	uv_rect.w = cellH / texH;     // v size (height)

	Billboard_Draw(texId, position, scale, uv_rect, color);
}
