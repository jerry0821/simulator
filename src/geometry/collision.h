// ----------------------------------------------------
// コリジョン判定 [collision.h]
// ====================================================
// Created by: Yasuda Atsushi
// Date: 2025-07-03
// Version: 1.1 (Modern C++ Refactor)
// ----------------------------------------------------
#ifndef COLLISION_H
#define COLLISION_H

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

namespace Collision {

struct Sphere {
  DirectX::XMFLOAT3 center; // 中心位置
  float radius;             // 半径

  bool Intersects(const Sphere &other) const;
  bool Intersects(const DirectX::XMFLOAT3 &point) const;
};

struct Circle {
  DirectX::XMFLOAT2 center; // 中心位置
  float radius;             // 半径

  bool Intersects(const Circle &other) const;
};

struct Box {
  DirectX::XMFLOAT2 center;
  float half_width;  // 幅
  float half_height; // 高さ

  bool Intersects(const Box &other) const;
};

struct Hit {
  bool isHit;               // 当たったかどうか
  DirectX::XMFLOAT3 normal; // 法線ベクトル
};

struct RayHit {
  bool isHit;
  float distance;
  DirectX::XMFLOAT3 normal;
};

struct AABB {
  DirectX::XMFLOAT3 min; // 最小座標 (左前下)
  DirectX::XMFLOAT3 max; // 最大座標 (右後上)

  DirectX::XMFLOAT3 GetCenter() const {
    return DirectX::XMFLOAT3{(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f,
                             (min.z + max.z) * 0.5f};
  }
  // インライン関数で半分の大きさを取得
  DirectX::XMFLOAT3 GetHalfSize() const {
    DirectX::XMFLOAT3 half_size;
    half_size.x = (max.x - min.x) * 0.5f;
    half_size.y = (max.y - min.y) * 0.5f;
    half_size.z = (max.z - min.z) * 0.5f;
    return half_size;
  }

  bool Intersects(const AABB &other) const;
  Hit CheckHit(const AABB &other) const; // aのどの面にbが衝突した情報
};

// 射線とAABBの交差判定 (t: 距離, normal: 法線)
RayHit IntersectRayAABB(const DirectX::XMFLOAT3 &rayStart,
                        const DirectX::XMFLOAT3 &rayDir,
                        const AABB &aabb);

// 射線とOBBの交差判定
RayHit IntersectRayOBB(const DirectX::XMFLOAT3 &rayStart,
                       const DirectX::XMFLOAT3 &rayDir,
                       const AABB &localAabb,
                       const DirectX::XMMATRIX &mtxWorld);

// 射線とSphereの交差判定 (mxtWorldでスケーリングと回転適用)
RayHit IntersectRaySphere(const DirectX::XMFLOAT3 &rayStart,
                          const DirectX::XMFLOAT3 &rayDir,
                          float radius,
                          const DirectX::XMMATRIX &mtxWorld);

// 射線とCylinderの交差判定
RayHit IntersectRayCylinder(const DirectX::XMFLOAT3 &rayStart,
                            const DirectX::XMFLOAT3 &rayDir,
                            float radius, float halfHeight,
                            const DirectX::XMMATRIX &mtxWorld);

// 射線とCapsuleの交差判定
RayHit IntersectRayCapsule(const DirectX::XMFLOAT3 &rayStart,
                           const DirectX::XMFLOAT3 &rayDir,
                           float radius, float halfHeight,
                           const DirectX::XMMATRIX &mtxWorld);

namespace Debug {

void Initialize(ID3D11Device *pDevice, ID3D11DeviceContext *pContext);
void Finalize();

void Draw(const Circle &circle,
          const DirectX::XMFLOAT4 &color = {1.0f, 0.0f, 0.0f, 1.0f});
void Draw(const Box &box, 
          const DirectX::XMFLOAT4 &color = {1.0f, 0.0f, 0.0f, 1.0f});

// 3d形状のデバッグ描画
void Draw(const AABB &aabb, 
          const DirectX::XMFLOAT4 &color = {1.0f, 0.0f, 0.0f, 1.0f});

// OBB描画
void DrawOBB(const AABB &localAabb,
             const DirectX::XMMATRIX &mtxWorld,
             const DirectX::XMFLOAT4 &color = {1.0f, 0.0f, 0.0f, 1.0f});

// 形状描画
void DrawSphereMtx(float radius,
                   const DirectX::XMMATRIX &mtxWorld,
                   const DirectX::XMFLOAT4 &color = {1.0f, 0.0f, 0.0f, 1.0f});
void DrawCapsuleMtx(float radius, float halfHeight,
                    const DirectX::XMMATRIX &mtxWorld,
                    const DirectX::XMFLOAT4 &color = {1.0f, 0.0f, 0.0f, 1.0f});

// Cone描画 (Gizmo用)
void DrawConeOBB(const DirectX::XMFLOAT3 &baseCenter,
                 const DirectX::XMFLOAT3 &tip, float radius,
                 const DirectX::XMMATRIX &mtxWorld,
                 const DirectX::XMFLOAT4 &color = {1.0f, 0.0f, 0.0f, 1.0f});

void Draw(const Sphere &sphere,
          const DirectX::XMFLOAT4 &color = {1.0f, 0.0f, 0.0f, 1.0f});

void DrawSphere(const Sphere &sphere,
                const DirectX::XMFLOAT4 &color = {1.0f, 0.0f, 0.0f, 1.0f});
void DrawTest();

} // namespace Debug
} // namespace Collision

#endif // COLLISION_H
