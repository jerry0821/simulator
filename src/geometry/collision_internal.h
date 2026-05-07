// ----------------------------------------------------
// コリジョンデバッグ描画 内部共有ヘッダー [collision_internal.h]
// ====================================================
// collision.cpp と collision_debug.cpp 間で共有する
// 内部状態の宣言。外部からincludeしないこと。
// ----------------------------------------------------
#ifndef COLLISION_INTERNAL_H
#define COLLISION_INTERNAL_H

#include <DirectXMath.h>
#include <d3d11.h>

// デバッグ描画用の頂点構造体
struct DebugVertex {
  DirectX::XMFLOAT3 position;
  DirectX::XMFLOAT3 normal;
  DirectX::XMFLOAT4 color;
  DirectX::XMFLOAT2 uv;
};

// 頂点構造体 (旧式、Circle/Box/Collision::AABB/OBB描画用)
struct CollisionVertex {
  DirectX::XMFLOAT3 position;
  DirectX::XMFLOAT3 normal;
  DirectX::XMFLOAT4 color;
  DirectX::XMFLOAT2 uv;
};

static constexpr int NUM_VERTEX = 5000;
static constexpr int MAX_VERTICES = 200;

// 共有グローバル変数 (collision_debug.cpp で定義)
extern ID3D11Buffer *g_pCollDebugVertexBuffer;
extern ID3D11DeviceContext *g_pCollDebugContext;
extern int g_CollDebugWhiteTexId;

#endif // COLLISION_INTERNAL_H
