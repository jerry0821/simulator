#include "frustum_culling.h"

using namespace DirectX;

void ViewFrustum::build(const XMFLOAT4X4& view_matrix,
						const XMFLOAT4X4& projection_matrix)
{
	BoundingFrustum local_frustum{};
	BoundingFrustum::CreateFromMatrix(local_frustum,
									  XMLoadFloat4x4(&projection_matrix));

	const XMMATRIX view = XMLoadFloat4x4(&view_matrix);
	const XMMATRIX inverse_view = XMMatrixInverse(nullptr, view);
	local_frustum.Transform(m_world_frustum, inverse_view);
}

bool ViewFrustum::intersects(const Collision::AABB& aabb) const
{
	const XMFLOAT3 center = aabb.GetCenter();
	const XMFLOAT3 extents = aabb.GetHalfSize();
	BoundingBox bounding_box{};
	bounding_box.Center = center;
	bounding_box.Extents = extents;

	return m_world_frustum.Contains(bounding_box) != DISJOINT;
}
