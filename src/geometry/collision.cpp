// ----------------------------------------------------
// コリジョン判定 [collision.cpp]
// ====================================================
// Created by: Yasuda Atsushi
// Date: 2025-07-03
// Version: 1.1 (Modern C++ Refactor)
// ----------------------------------------------------

#include "collision.h"

#include <algorithm>
using namespace DirectX;
using namespace Collision;

static constexpr float RAY_MAX_DIST = 1e5f; // レイ判定の最大距離

namespace Collision {

bool Sphere::Intersects(const Sphere &other) const {
  XMVECTOR ac = XMLoadFloat3(&center);
  XMVECTOR bc = XMLoadFloat3(&other.center);
  XMVECTOR lsq = XMVector3LengthSq(bc - ac); // 中心間の距離の二乗

  return (radius + other.radius) * (radius + other.radius) > XMVectorGetX(lsq);
}

bool Sphere::Intersects(const DirectX::XMFLOAT3 &point) const {
  XMVECTOR ac = XMLoadFloat3(&center);
  XMVECTOR bc = XMLoadFloat3(&point);
  XMVECTOR lsq = XMVector3LengthSq(bc - ac); // 中心間の距離の二乗

  return (radius * radius) > XMVectorGetX(lsq); // 半径の和の二乗と比較
}

bool Circle::Intersects(const Circle &other) const {
  float xl = other.center.x - center.x;
  float yl = other.center.y - center.y;

  return (radius + other.radius) * (radius + other.radius) > (xl * xl + yl * yl);
}

bool Box::Intersects(const Box &other) const {
  float at = center.y - half_height;
  float ab = center.y + half_height;
  float al = center.x - half_width;
  float ar = center.x + half_width;
  float bt = other.center.y - other.half_height;
  float bb = other.center.y + other.half_height;
  float bl = other.center.x - other.half_width;
  float br = other.center.x + other.half_width;

  return al < br && ar > bl && at < bb && ab > bt;
}

bool AABB::Intersects(const AABB &other) const {
  return min.x < other.max.x && max.x > other.min.x && min.y < other.max.y &&
         max.y > other.min.y && min.z < other.max.z && max.z > other.min.z;
}

Hit AABB::CheckHit(const AABB &other) const {
  Hit hit{};

  // 当たっているかどうかの判定
  hit.isHit = this->Intersects(other);

  if (!hit.isHit) {
    return hit; // 当たっていなければ終了
  }

  float xdepth = (std::min)(max.x, other.max.x) - (std::max)(min.x, other.min.x);
  float ydepth = (std::min)(max.y, other.max.y) - (std::max)(min.y, other.min.y);
  float zdepth = (std::min)(max.z, other.max.z) - (std::max)(min.z, other.min.z);

  // 一番浅い軸を見つける
  bool isShallowX = false;
  bool isShallowY = false;
  bool isShallowZ = false;

  if (xdepth > ydepth) {
    if (ydepth > zdepth) {
      isShallowZ = true;
    } else {
      isShallowY = true;
    }
  } else {
    if (xdepth > zdepth) {
      isShallowZ = true;
    } else {
      isShallowX = true;
    }
  }
  XMFLOAT3 a_center = this->GetCenter();
  XMFLOAT3 b_center = other.GetCenter();
  XMVECTOR normal = XMLoadFloat3(&b_center) - XMLoadFloat3(&a_center);
  if (isShallowX) {
    normal = XMVector3Normalize(normal * XMVECTOR{1.0f, 0.0f, 0.0f});
  } else if (isShallowY) {
    normal = XMVector3Normalize(normal * XMVECTOR{0.0f, 1.0f, 0.0f});
  } else if (isShallowZ) {
    normal = XMVector3Normalize(normal * XMVECTOR{0.0f, 0.0f, 1.0f});
  }

  XMStoreFloat3(&hit.normal, normal);

  return hit;
}

RayHit IntersectRayAABB(const DirectX::XMFLOAT3 &rayStart,
                        const DirectX::XMFLOAT3 &rayDir,
                        const AABB &aabb) {
  RayHit hit{};
  hit.isHit = false;
  hit.distance = -1.0f;
  hit.normal = {0.0f, 0.0f, 0.0f};

  float tMin = 0.0f;
  float tMax = RAY_MAX_DIST;

  DirectX::XMFLOAT3 normals[3] = {
      {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

  DirectX::XMFLOAT3 hitNormalMin = {0.0f, 0.0f, 0.0f};
  DirectX::XMFLOAT3 hitNormalMax = {0.0f, 0.0f, 0.0f};

  for (int i = 0; i < 3; ++i) {
    float start, dir, minBound, maxBound;
    if (i == 0) {
      start = rayStart.x;
      dir = rayDir.x;
      minBound = aabb.min.x;
      maxBound = aabb.max.x;
    } else if (i == 1) {
      start = rayStart.y;
      dir = rayDir.y;
      minBound = aabb.min.y;
      maxBound = aabb.max.y;
    } else {
      start = rayStart.z;
      dir = rayDir.z;
      minBound = aabb.min.z;
      maxBound = aabb.max.z;
    }

    if (std::abs(dir) < 0.000001f) {
      if (start < minBound || start > maxBound)
        return hit; // Ray is parallel and outside bounds
    } else {
      float ood = 1.0f / dir;
      float t1 = (minBound - start) * ood;
      float t2 = (maxBound - start) * ood;

      DirectX::XMFLOAT3 n1 = {-normals[i].x, -normals[i].y, -normals[i].z};
      DirectX::XMFLOAT3 n2 = normals[i];

      if (t1 > t2) {
        std::swap(t1, t2);
        std::swap(n1, n2);
      }

      if (t1 > tMin) {
        tMin = t1;
        hitNormalMin = n1;
      }
      if (t2 < tMax) {
        tMax = t2;
        hitNormalMax = n2;
      }

      if (tMin > tMax)
        return hit; // No intersection
    }
  }

  if (tMin < 0.0f) {
    tMin = tMax;
    hitNormalMin = hitNormalMax;
    if (tMin < 0.0f)
      return hit; // AABB is behind ray
  }

  hit.isHit = true;
  hit.distance = tMin;
  hit.normal = hitNormalMin;

  return hit;
}

RayHit IntersectRayOBB(const DirectX::XMFLOAT3 &rayStart,
                       const DirectX::XMFLOAT3 &rayDir,
                       const AABB &localAabb,
                       const DirectX::XMMATRIX &mtxWorld) {
  RayHit hit{};
  hit.isHit = false;

  XMVECTOR det;
  XMMATRIX mtxInv = XMMatrixInverse(&det, mtxWorld);

  XMVECTOR vrStart = XMLoadFloat3(&rayStart);
  XMVECTOR vrDir = XMLoadFloat3(&rayDir);

  XMVECTOR localStart = XMVector3TransformCoord(vrStart, mtxInv);
  XMVECTOR localDir = XMVector3TransformNormal(vrDir, mtxInv);

  XMFLOAT3 lStart, lDir;
  XMStoreFloat3(&lStart, localStart);
  XMStoreFloat3(&lDir, localDir);

  RayHit localHit = IntersectRayAABB(lStart, lDir, localAabb);

  if (localHit.isHit) {
    hit.isHit = true;

    XMVECTOR localIntersect = localStart + localDir * localHit.distance;
    XMVECTOR worldIntersect = XMVector3TransformCoord(localIntersect, mtxWorld);
    XMVECTOR worldDiff = worldIntersect - vrStart;
    hit.distance = XMVectorGetX(XMVector3Length(worldDiff));

    XMVECTOR localNormal = XMLoadFloat3(&localHit.normal);
    XMVECTOR worldNormal = XMVector3TransformNormal(localNormal, mtxWorld);
    worldNormal = XMVector3Normalize(worldNormal);
    XMStoreFloat3(&hit.normal, worldNormal);
  }

  return hit;
}

RayHit IntersectRaySphere(const DirectX::XMFLOAT3 &rayStart,
                          const DirectX::XMFLOAT3 &rayDir,
                          float radius,
                          const DirectX::XMMATRIX &mtxWorld) {
  RayHit hit{};
  hit.isHit = false;

  XMVECTOR det;
  XMMATRIX mtxInv = XMMatrixInverse(&det, mtxWorld);

  XMVECTOR vrStart = XMLoadFloat3(&rayStart);
  XMVECTOR vrDir = XMLoadFloat3(&rayDir);

  XMVECTOR localStart = XMVector3TransformCoord(vrStart, mtxInv);
  XMVECTOR localDir = XMVector3TransformNormal(vrDir, mtxInv);

  XMFLOAT3 lStart, lDir;
  XMStoreFloat3(&lStart, localStart);
  XMStoreFloat3(&lDir, localDir);

  float a = lDir.x * lDir.x + lDir.y * lDir.y + lDir.z * lDir.z;
  float b = 2.0f * (lDir.x * lStart.x + lDir.y * lStart.y + lDir.z * lStart.z);
  float c = (lStart.x * lStart.x + lStart.y * lStart.y + lStart.z * lStart.z) -
            radius * radius;

  float discriminant = b * b - 4 * a * c;
  if (discriminant >= 0.0f) {
    float t1 = (-b - sqrtf(discriminant)) / (2.0f * a);
    float t2 = (-b + sqrtf(discriminant)) / (2.0f * a);

    float t = -1.0f;
    if (t1 >= 0.0f)
      t = t1;
    else if (t2 >= 0.0f)
      t = t2;

    if (t >= 0.0f) {
      hit.isHit = true;
      XMVECTOR localIntersect = localStart + localDir * t;
      XMVECTOR worldIntersect =
          XMVector3TransformCoord(localIntersect, mtxWorld);
      XMVECTOR worldDiff = worldIntersect - vrStart;
      hit.distance = XMVectorGetX(XMVector3Length(worldDiff));

      XMVECTOR localNormal = XMVector3Normalize(localIntersect);
      XMVECTOR worldNormal =
          XMVector3Normalize(XMVector3TransformNormal(localNormal, mtxWorld));
      XMStoreFloat3(&hit.normal, worldNormal);
    }
  }
  return hit;
}

RayHit IntersectRayCylinder(const DirectX::XMFLOAT3 &rayStart,
                            const DirectX::XMFLOAT3 &rayDir,
                            float radius, float halfHeight,
                            const DirectX::XMMATRIX &mtxWorld) {
  RayHit hit{};
  hit.isHit = false;

  XMVECTOR det;
  XMMATRIX mtxInv = XMMatrixInverse(&det, mtxWorld);

  XMVECTOR vrStart = XMLoadFloat3(&rayStart);
  XMVECTOR vrDir = XMLoadFloat3(&rayDir);

  XMVECTOR localStart = XMVector3TransformCoord(vrStart, mtxInv);
  XMVECTOR localDir = XMVector3TransformNormal(vrDir, mtxInv);

  XMFLOAT3 lStart, lDir;
  XMStoreFloat3(&lStart, localStart);
  XMStoreFloat3(&lDir, localDir);

  float tMin = RAY_MAX_DIST;
  XMFLOAT3 bestNormal = {0, 0, 0};

  float a = lDir.x * lDir.x + lDir.z * lDir.z;
  float b = 2.0f * (lDir.x * lStart.x + lDir.z * lStart.z);
  float c = (lStart.x * lStart.x + lStart.z * lStart.z) - radius * radius;

  if (a > 0.00001f) {
    float discriminant = b * b - 4 * a * c;
    if (discriminant >= 0.0f) {
      float t1 = (-b - sqrtf(discriminant)) / (2.0f * a);
      float t2 = (-b + sqrtf(discriminant)) / (2.0f * a);

      float ts[2] = {t1, t2};
      for (int i = 0; i < 2; ++i) {
        if (ts[i] >= 0.0f && ts[i] < tMin) {
          float yHit = lStart.y + lDir.y * ts[i];
          if (yHit >= -halfHeight && yHit <= halfHeight) {
            tMin = ts[i];
            bestNormal = {lStart.x + lDir.x * ts[i], 0.0f,
                          lStart.z + lDir.z * ts[i]};
            float len = sqrtf(bestNormal.x * bestNormal.x +
                              bestNormal.z * bestNormal.z);
            if (len > 0) {
              bestNormal.x /= len;
              bestNormal.z /= len;
            }
          }
        }
      }
    }
  }

  if (std::abs(lDir.y) > 0.00001f) {
    float tTop = (halfHeight - lStart.y) / lDir.y;
    if (tTop >= 0.0f && tTop < tMin) {
      float xHit = lStart.x + lDir.x * tTop;
      float zHit = lStart.z + lDir.z * tTop;
      if (xHit * xHit + zHit * zHit <= radius * radius) {
        tMin = tTop;
        bestNormal = {0.0f, 1.0f, 0.0f};
      }
    }

    float tBot = (-halfHeight - lStart.y) / lDir.y;
    if (tBot >= 0.0f && tBot < tMin) {
      float xHit = lStart.x + lDir.x * tBot;
      float zHit = lStart.z + lDir.z * tBot;
      if (xHit * xHit + zHit * zHit <= radius * radius) {
        tMin = tBot;
        bestNormal = {0.0f, -1.0f, 0.0f};
      }
    }
  }

  if (tMin < RAY_MAX_DIST) {
    hit.isHit = true;
    XMVECTOR localIntersect = localStart + localDir * tMin;
    XMVECTOR worldIntersect = XMVector3TransformCoord(localIntersect, mtxWorld);
    XMVECTOR worldDiff = worldIntersect - vrStart;
    hit.distance = XMVectorGetX(XMVector3Length(worldDiff));

    XMVECTOR localNormal = XMLoadFloat3(&bestNormal);
    XMVECTOR worldNormal =
        XMVector3Normalize(XMVector3TransformNormal(localNormal, mtxWorld));
    XMStoreFloat3(&hit.normal, worldNormal);
  }

  return hit;
}

RayHit IntersectRayCapsule(const DirectX::XMFLOAT3 &rayStart,
                           const DirectX::XMFLOAT3 &rayDir,
                           float radius, float halfHeight,
                           const DirectX::XMMATRIX &mtxWorld) {
  RayHit hit{};
  hit.isHit = false;

  XMVECTOR det;
  XMMATRIX mtxInv = XMMatrixInverse(&det, mtxWorld);

  XMVECTOR vrStart = XMLoadFloat3(&rayStart);
  XMVECTOR vrDir = XMLoadFloat3(&rayDir);

  XMVECTOR localStart = XMVector3TransformCoord(vrStart, mtxInv);
  XMVECTOR localDir = XMVector3TransformNormal(vrDir, mtxInv);

  XMFLOAT3 lStart, lDir;
  XMStoreFloat3(&lStart, localStart);
  XMStoreFloat3(&lDir, localDir);

  float tMin = RAY_MAX_DIST;
  XMFLOAT3 bestNormal = {0, 0, 0};

  // --- 1. Check Cylinder Body ---
  float a = lDir.x * lDir.x + lDir.z * lDir.z;
  float b = 2.0f * (lDir.x * lStart.x + lDir.z * lStart.z);
  float c = (lStart.x * lStart.x + lStart.z * lStart.z) - radius * radius;

  if (a > 0.00001f) {
    float discriminant = b * b - 4 * a * c;
    if (discriminant >= 0.0f) {
      float ts[2] = {(-b - sqrtf(discriminant)) / (2.0f * a),
                     (-b + sqrtf(discriminant)) / (2.0f * a)};
      for (int i = 0; i < 2; ++i) {
        if (ts[i] >= 0.0f && ts[i] < tMin) {
          float yHit = lStart.y + lDir.y * ts[i];
          if (yHit >= -halfHeight && yHit <= halfHeight) {
            tMin = ts[i];
            bestNormal = {lStart.x + lDir.x * ts[i], 0.0f,
                          lStart.z + lDir.z * ts[i]};
            float len = sqrtf(bestNormal.x * bestNormal.x +
                              bestNormal.z * bestNormal.z);
            if (len > 0) {
              bestNormal.x /= len;
              bestNormal.z /= len;
            }
          }
        }
      }
    }
  }

  // --- 2. Check Top Hemisphere ---
  XMFLOAT3 topCenter = {0.0f, halfHeight, 0.0f};
  float a_top = lDir.x * lDir.x + lDir.y * lDir.y + lDir.z * lDir.z;
  float b_top = 2.0f * (lDir.x * (lStart.x - topCenter.x) +
                        lDir.y * (lStart.y - topCenter.y) +
                        lDir.z * (lStart.z - topCenter.z));
  float c_top = ((lStart.x - topCenter.x) * (lStart.x - topCenter.x) +
                 (lStart.y - topCenter.y) * (lStart.y - topCenter.y) +
                 (lStart.z - topCenter.z) * (lStart.z - topCenter.z)) -
                radius * radius;

  float disc_top = b_top * b_top - 4 * a_top * c_top;
  if (disc_top >= 0.0f) {
    float ts[2] = {(-b_top - sqrtf(disc_top)) / (2.0f * a_top),
                   (-b_top + sqrtf(disc_top)) / (2.0f * a_top)};
    for (int i = 0; i < 2; ++i) {
      if (ts[i] >= 0.0f && ts[i] < tMin) {
        float yHit = lStart.y + lDir.y * ts[i];
        if (yHit >= halfHeight) { // Only upper half
          tMin = ts[i];
          XMFLOAT3 intersect = {lStart.x + lDir.x * ts[i], yHit,
                                lStart.z + lDir.z * ts[i]};
          bestNormal = {intersect.x - topCenter.x, intersect.y - topCenter.y,
                        intersect.z - topCenter.z};
          float len =
              sqrtf(bestNormal.x * bestNormal.x + bestNormal.y * bestNormal.y +
                    bestNormal.z * bestNormal.z);
          if (len > 0) {
            bestNormal.x /= len;
            bestNormal.y /= len;
            bestNormal.z /= len;
          }
        }
      }
    }
  }

  // --- 3. Check Bottom Hemisphere ---
  XMFLOAT3 botCenter = {0.0f, -halfHeight, 0.0f};
  float a_bot = a_top; // Same
  float b_bot = 2.0f * (lDir.x * (lStart.x - botCenter.x) +
                        lDir.y * (lStart.y - botCenter.y) +
                        lDir.z * (lStart.z - botCenter.z));
  float c_bot = ((lStart.x - botCenter.x) * (lStart.x - botCenter.x) +
                 (lStart.y - botCenter.y) * (lStart.y - botCenter.y) +
                 (lStart.z - botCenter.z) * (lStart.z - botCenter.z)) -
                radius * radius;

  float disc_bot = b_bot * b_bot - 4 * a_bot * c_bot;
  if (disc_bot >= 0.0f) {
    float ts[2] = {(-b_bot - sqrtf(disc_bot)) / (2.0f * a_bot),
                   (-b_bot + sqrtf(disc_bot)) / (2.0f * a_bot)};
    for (int i = 0; i < 2; ++i) {
      if (ts[i] >= 0.0f && ts[i] < tMin) {
        float yHit = lStart.y + lDir.y * ts[i];
        if (yHit <= -halfHeight) { // Only lower half
          tMin = ts[i];
          XMFLOAT3 intersect = {lStart.x + lDir.x * ts[i], yHit,
                                lStart.z + lDir.z * ts[i]};
          bestNormal = {intersect.x - botCenter.x, intersect.y - botCenter.y,
                        intersect.z - botCenter.z};
          float len =
              sqrtf(bestNormal.x * bestNormal.x + bestNormal.y * bestNormal.y +
                    bestNormal.z * bestNormal.z);
          if (len > 0) {
            bestNormal.x /= len;
            bestNormal.y /= len;
            bestNormal.z /= len;
          }
        }
      }
    }
  }

  // Finalize world hit
  if (tMin < RAY_MAX_DIST) {
    hit.isHit = true;
    XMVECTOR localIntersect = localStart + localDir * tMin;
    XMVECTOR worldIntersect = XMVector3TransformCoord(localIntersect, mtxWorld);
    XMVECTOR worldDiff = worldIntersect - vrStart;
    hit.distance = XMVectorGetX(XMVector3Length(worldDiff));

    XMVECTOR localNormal = XMLoadFloat3(&bestNormal);
    XMVECTOR worldNormal =
        XMVector3Normalize(XMVector3TransformNormal(localNormal, mtxWorld));
    XMStoreFloat3(&hit.normal, worldNormal);
  }

  return hit;
}

} // namespace Collision
