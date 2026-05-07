#ifndef OCTREE_H
#define OCTREE_H

#include <array>
#include <vector>

#include "collision.h"

class ViewFrustum;

struct OctreeItem
{
	Collision::AABB bounds;
	int payload_index = -1;
};

class Octree
{
public:
	void build(const std::vector<OctreeItem>& items,
			   int max_depth = 6,
			   int max_items_per_leaf = 24);
	void query(const ViewFrustum& view_frustum,
			   std::vector<int>& visible_payload_indices) const;
	bool empty() const;

private:
	struct Node
	{
		Collision::AABB bounds{};
		std::array<int, 8> children{ -1, -1, -1, -1, -1, -1, -1, -1 };
		std::vector<int> item_indices;
		bool is_leaf = true;
	};

	bool intersectsChildBounds(const Collision::AABB& child_bounds,
							   const Collision::AABB& item_bounds) const;
	void subdivideNode(int node_index,
					   const std::vector<OctreeItem>& items,
					   int depth,
					   int max_depth,
					   int max_items_per_leaf);
	void queryNode(int node_index,
				   const ViewFrustum& view_frustum,
				   const std::vector<OctreeItem>& items,
				   std::vector<int>& visible_payload_indices) const;
	std::array<Collision::AABB, 8> buildChildBounds(const Collision::AABB& bounds) const;
	Collision::AABB buildRootBounds(const std::vector<OctreeItem>& items) const;

	std::vector<Node> m_nodes;
	std::vector<OctreeItem> m_items;
};

#endif // OCTREE_H
