#ifndef FRUSTUM_CULLING_H
#define FRUSTUM_CULLING_H

#include <DirectXCollision.h>
#include <DirectXMath.h>

#include "collision.h"

class ViewFrustum
{
public:
	void build(const DirectX::XMFLOAT4X4& view_matrix,
			   const DirectX::XMFLOAT4X4& projection_matrix);
	bool intersects(const Collision::AABB& aabb) const;

private:
	DirectX::BoundingFrustum m_world_frustum{};
};

#endif // FRUSTUM_CULLING_H
