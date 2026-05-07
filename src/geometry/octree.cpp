#include "octree.h"

#include <algorithm>

#include "frustum_culling.h"

namespace
{
bool ContainsAabb(const Collision::AABB& outer, const Collision::AABB& inner)
{
	return inner.min.x >= outer.min.x && inner.max.x <= outer.max.x &&
		   inner.min.y >= outer.min.y && inner.max.y <= outer.max.y &&
		   inner.min.z >= outer.min.z && inner.max.z <= outer.max.z;
}
}

void Octree::build(const std::vector<OctreeItem>& items,
				   int max_depth,
				   int max_items_per_leaf)
{
	m_nodes.clear();
	m_items = items;

	if (m_items.empty())
	{
		return;
	}

	Node root{};
	root.bounds = buildRootBounds(m_items);
	root.item_indices.resize(m_items.size());
	for (int index = 0; index < static_cast<int>(m_items.size()); ++index)
	{
		root.item_indices[index] = index;
	}

	m_nodes.push_back(std::move(root));
	subdivideNode(0, m_items, 0, max_depth, max_items_per_leaf);
}

void Octree::query(const ViewFrustum& view_frustum,
				   std::vector<int>& visible_payload_indices) const
{
	visible_payload_indices.clear();

	if (m_nodes.empty())
	{
		return;
	}

	queryNode(0, view_frustum, m_items, visible_payload_indices);
}

bool Octree::empty() const
{
	return m_nodes.empty();
}

bool Octree::intersectsChildBounds(const Collision::AABB& child_bounds,
								   const Collision::AABB& item_bounds) const
{
	return ContainsAabb(child_bounds, item_bounds);
}

void Octree::subdivideNode(int node_index,
						   const std::vector<OctreeItem>& items,
						   int depth,
						   int max_depth,
						   int max_items_per_leaf)
{
	Node& node = m_nodes[node_index];
	if (depth >= max_depth || static_cast<int>(node.item_indices.size()) <= max_items_per_leaf)
	{
		return;
	}

	const auto child_bounds = buildChildBounds(node.bounds);
	std::array<std::vector<int>, 8> child_item_lists;
	std::vector<int> remaining_items;

	for (int item_index : node.item_indices)
	{
		bool inserted = false;
		for (int child_index = 0; child_index < 8; ++child_index)
		{
			if (intersectsChildBounds(child_bounds[child_index], items[item_index].bounds))
			{
				child_item_lists[child_index].push_back(item_index);
				inserted = true;
				break;
			}
		}

		if (!inserted)
		{
			remaining_items.push_back(item_index);
		}
	}

	bool created_child = false;
	for (int child_index = 0; child_index < 8; ++child_index)
	{
		if (child_item_lists[child_index].empty())
		{
			continue;
		}

		Node child{};
		child.bounds = child_bounds[child_index];
		child.item_indices = std::move(child_item_lists[child_index]);
		node.children[child_index] = static_cast<int>(m_nodes.size());
		m_nodes.push_back(std::move(child));
		created_child = true;
	}

	if (!created_child)
	{
		return;
	}

	node.is_leaf = false;
	node.item_indices = std::move(remaining_items);

	for (int child_index : node.children)
	{
		if (child_index >= 0)
		{
			subdivideNode(child_index, items, depth + 1, max_depth, max_items_per_leaf);
		}
	}
}

void Octree::queryNode(int node_index,
					   const ViewFrustum& view_frustum,
					   const std::vector<OctreeItem>& items,
					   std::vector<int>& visible_payload_indices) const
{
	const Node& node = m_nodes[node_index];
	if (!view_frustum.intersects(node.bounds))
	{
		return;
	}

	for (int item_index : node.item_indices)
	{
		if (view_frustum.intersects(items[item_index].bounds))
		{
			visible_payload_indices.push_back(items[item_index].payload_index);
		}
	}

	for (int child_index : node.children)
	{
		if (child_index >= 0)
		{
			queryNode(child_index, view_frustum, items, visible_payload_indices);
		}
	}
}

std::array<Collision::AABB, 8> Octree::buildChildBounds(const Collision::AABB& bounds) const
{
	const DirectX::XMFLOAT3 center = bounds.GetCenter();
	std::array<Collision::AABB, 8> children{};

	for (int child_index = 0; child_index < 8; ++child_index)
	{
		const bool high_x = (child_index & 1) != 0;
		const bool high_y = (child_index & 2) != 0;
		const bool high_z = (child_index & 4) != 0;

		children[child_index].min = {
			high_x ? center.x : bounds.min.x,
			high_y ? center.y : bounds.min.y,
			high_z ? center.z : bounds.min.z };
		children[child_index].max = {
			high_x ? bounds.max.x : center.x,
			high_y ? bounds.max.y : center.y,
			high_z ? bounds.max.z : center.z };
	}

	return children;
}

Collision::AABB Octree::buildRootBounds(const std::vector<OctreeItem>& items) const
{
	Collision::AABB root_bounds = items.front().bounds;

	for (const auto& item : items)
	{
		root_bounds.min.x = std::min(root_bounds.min.x, item.bounds.min.x);
		root_bounds.min.y = std::min(root_bounds.min.y, item.bounds.min.y);
		root_bounds.min.z = std::min(root_bounds.min.z, item.bounds.min.z);
		root_bounds.max.x = std::max(root_bounds.max.x, item.bounds.max.x);
		root_bounds.max.y = std::max(root_bounds.max.y, item.bounds.max.y);
		root_bounds.max.z = std::max(root_bounds.max.z, item.bounds.max.z);
	}

	const DirectX::XMFLOAT3 center = root_bounds.GetCenter();
	const DirectX::XMFLOAT3 half_size = root_bounds.GetHalfSize();
	const float max_half_extent =
		std::max({ half_size.x, half_size.y, half_size.z }) + 0.01f;

	root_bounds.min = {
		center.x - max_half_extent,
		center.y - max_half_extent,
		center.z - max_half_extent };
	root_bounds.max = {
		center.x + max_half_extent,
		center.y + max_half_extent,
		center.z + max_half_extent };

	return root_bounds;
}
