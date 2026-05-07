//// ----------------------------------------------------
//// XZ平面グリッドの表示 [grid.h]
//// ====================================================
//// Created by: Jerry
//// Date: 2025-09-11
//// Version: 1.0
//// ----------------------------------------------------
// #include "grid.h"
// #include "direct3d.h"
// #include <DirectXMath.h>
// using namespace DirectX;
// #include "shader3d.h"
//
// static constexpr int GRID_H_COUNT = 10; // グリッドの横線の数
// static constexpr int GRID_V_COUNT = 10; // グリッドの縦線の数
// static constexpr int GRID_H_LINE_COUNT = GRID_H_COUNT + 1; // 横線の頂点数
// static constexpr int GRID_V_LINE_COUNT = GRID_V_COUNT + 1; // 縦線の頂点数
// static constexpr int NUM_VERTEX = (GRID_H_LINE_COUNT + GRID_V_LINE_COUNT) *
// 2; // 頂点
//
// static ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ
//
//// 注意！初期化で外部から設定されるもの。Release不要。
// static ID3D11Device* g_pDevice = nullptr;
// static ID3D11DeviceContext* g_pContext = nullptr;
//
//// 頂点構造体
// struct Vertex3D
//{
//	XMFLOAT3 position;	// 頂点座標
//	XMFLOAT4 color;		// 色
// };
//
// static Vertex3D g_GridVertex[NUM_VERTEX]{};
//
// void Grid_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
//{
//	// デバイスとデバイスコンテキストの保存
//	g_pDevice = pDevice;
//	g_pContext = pContext;
//
//	// 頂点バッファ生成
//	D3D11_BUFFER_DESC bd = {};
//	bd.Usage = D3D11_USAGE_DEFAULT;
//	bd.ByteWidth = sizeof(Vertex3D) * NUM_VERTEX;
//	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
//	bd.CPUAccessFlags = 0;
//
//	D3D11_SUBRESOURCE_DATA sd{};
//	sd.pSysMem = g_GridVertex;
//
//	float x = -((float)GRID_H_COUNT / 2.0f);
//	float z = -((float)GRID_V_COUNT / 2.0f);
//	for (int i = 0; i < GRID_H_LINE_COUNT; i++)
//	{
//		g_GridVertex[i * 2 + 0] = { { x + i, 0.0f,  (float)GRID_V_COUNT
/// 2.0f}, { 0.0f, 1.0f, 0.0f, 1.0f} }; 		g_GridVertex[i * 2 + 1]
/// = { { x + i,
// 0.0f, -(float)GRID_V_COUNT / 2.0f}, { 0.0f, 1.0f, 0.0f, 1.0f} };
//	}
//	for (int i = 0; i < GRID_V_LINE_COUNT; i++)
//	{
//		g_GridVertex[(GRID_H_LINE_COUNT * 2) + i * 2 + 0] = { {
//(float)GRID_H_COUNT / 2.0f, 0.0f, z + i}, { 0.0f, 0.0f, 0.0f, 1.0f} };
//		g_GridVertex[(GRID_H_LINE_COUNT * 2) + i * 2 + 1] = {
//{-(float)GRID_H_COUNT / 2.0f, 0.0f, z + i}, { 0.0f, 0.0f, 0.0f, 1.0f} };
//	}
//
//	g_pDevice->CreateBuffer(&bd, &sd, &g_pVertexBuffer);
//
//  }
//
//  void Grid_Finalize(void)
//{
//	SAFE_RELEASE(g_pVertexBuffer);
//  }
//
//  void Grid_Draw(void)
//{
//	// シェーダーを描画パイプラインに設定
//	Shader3D_Begin();
//
//	// 頂点バッファを描画パイプラインに設定
//	UINT stride = sizeof(Vertex3D);
//	UINT offset = 0;
//	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride,
//&offset);
//
//	// ワールド座標変換行列の作成
//	XMMATRIX mtxWorld = XMMatrixIdentity(); // 単位行列の作成
//
//	// 頂点シェーダーにワールド座標変換行列を設定
//	Shader3D_SetWorldMatrix(mtxWorld);
//
//	//// ビュー座標変換行列の作成
//	//XMMATRIX mtxView = XMMatrixLookAtLH(
//	//	{ 2.0f,  5.0f,  5.0f },	//EyePosition(カメラの座標)
//	//	{ 0.0f,  0.0f,  0.0f },	//FocusPosition(視点)
//	//	{ 0.0f,  1.0f,  0.0f });	//UpDirection(頭頂方向)
//	//
//	//Shader3D_SetViewMatrix(mtxView);
//	//
//	//// パースペクティブ行列の作成
//	//constexpr float fovAngleY = XMConvertToRadians(60.0f); // 視野角
//	//float aspectRatio = (float)Direct3D_GetBackBufferWidth() /
//(float)Direct3D_GetBackBufferHeight(); // アスペクト比
//	//float nearZ = 0.1f; // ニアクリップ距離
//	//float farZ = 100.0f; // ファークリップ距離
//	//XMMATRIX mtxPerspective = XMMatrixPerspectiveFovLH(fovAngleY,
// aspectRatio, nearZ, farZ);
//
//	// 頂点シェーダーにプロジェクション変換行列を設定
//	//Shader3D_SetProjectionMatrix(mtxPerspective);
//
//	// プリミティブトポロジ設定
//	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
//
//	// ポリゴン描画命令発行
//	g_pContext->Draw(NUM_VERTEX, 0); // 6頂点で1面分
//  }
#include "grid.h"
#include "direct3d.h"
#include "shader3d.h"
#include "texture.h"
#include <DirectXMath.h>

using namespace DirectX;

// 網格設定 (10x10 格 = 10.0f x 10.0f 的範圍)
static constexpr int GRID_H_COUNT = 30; // 橫向線條數
static constexpr int GRID_V_COUNT = 30; // 縱向線條數
static constexpr int GRID_H_LINE_COUNT = GRID_H_COUNT + 1;
static constexpr int GRID_V_LINE_COUNT = GRID_V_COUNT + 1;
static constexpr int NUM_VERTEX = (GRID_H_LINE_COUNT + GRID_V_LINE_COUNT) * 2;

static ID3D11Buffer *g_pVertexBuffer = nullptr;

// 頂点構造体
struct Vertex3D {
  XMFLOAT3 position; // 頂点座標
  XMFLOAT3 normal;   // 法線
  XMFLOAT4 color;    // 色
  XMFLOAT2 texcoord; // UV
};

// 頂點資料陣列
static Vertex3D g_GridVertex[NUM_VERTEX]{};

// 用來保存 Device 指標
static ID3D11Device *g_pDevice = nullptr;
static ID3D11DeviceContext *g_pContext = nullptr;

void Grid_Initialize(ID3D11Device *pDevice, ID3D11DeviceContext *pContext) {
  g_pDevice = pDevice;
  g_pContext = pContext;

  // 1. 建立頂點資料 (以 0,0,0 為中心)
  float x_start = -((float)GRID_H_COUNT / 2.0f); // -5.0f
  float z_start = -((float)GRID_V_COUNT / 2.0f); // -5.0f

  // 縱向線 (平行於 Z 軸)
  for (int i = 0; i < GRID_H_LINE_COUNT; i++) {
    float x = x_start + i;
    g_GridVertex[i * 2 + 0] = {{x, 0.0f, (float)GRID_V_COUNT / 2.0f},
                               {0, 1, 0},
                               {0.0f, 1.0f, 0.0f, 1.0f},
                               {0, 0}}; // 遠端
    g_GridVertex[i * 2 + 1] = {{x, 0.0f, -(float)GRID_V_COUNT / 2.0f},
                               {0, 1, 0},
                               {0.0f, 1.0f, 0.0f, 1.0f},
                               {0, 0}}; // 近端
  }

  // 橫向線 (平行於 X 軸)
  for (int i = 0; i < GRID_V_LINE_COUNT; i++) {
    float z = z_start + i;
    int offset = GRID_H_LINE_COUNT * 2;
    g_GridVertex[offset + i * 2 + 0] = {{(float)GRID_H_COUNT / 2.0f, 0.0f, z},
                                        {0, 1, 0},
                                        {0.0f, 1.0f, 0.0f, 1.0f},
                                        {0, 0}}; // 右端
    g_GridVertex[offset + i * 2 + 1] = {{-(float)GRID_H_COUNT / 2.0f, 0.0f, z},
                                        {0, 1, 0},
                                        {0.0f, 1.0f, 0.0f, 1.0f},
                                        {0, 0}}; // 左端
  }

  // 2. 建立頂點緩衝區
  D3D11_BUFFER_DESC bd = {};
  bd.Usage = D3D11_USAGE_DEFAULT;
  bd.ByteWidth = sizeof(Vertex3D) * NUM_VERTEX;
  bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  bd.CPUAccessFlags = 0;

  D3D11_SUBRESOURCE_DATA sd{};
  sd.pSysMem = g_GridVertex;

  g_pDevice->CreateBuffer(&bd, &sd, &g_pVertexBuffer);
}

void Grid_Finalize(void) { SAFE_RELEASE(g_pVertexBuffer); }

void Grid_Draw(void) {
  // 開始 Shader
  Shader3D_Begin();

  // 設定頂點緩衝
  UINT stride = sizeof(Vertex3D);
  UINT offset = 0;
  g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

  // 世界矩陣 (單位矩陣 = 原點不移動)
  XMMATRIX mtxWorld = XMMatrixIdentity();
  Shader3D_SetWorldMatrix(mtxWorld);

  // 【重要】這裡假設 View 和 Projection 矩陣已經在 Game_Draw 裡設定好了
  // 所以這裡不需要再設定一次 Camera

  // 設定畫線模式
  g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

  // 設定紋理 (使用 0 號貼圖以防止前一次畫的貼圖污染線條顏色)
  Texture_SetTexture(0);

  g_pContext->Draw(NUM_VERTEX, 0);
}