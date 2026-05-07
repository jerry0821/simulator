#include "compute_task_runner.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <numeric>
#include <queue>

#include "debug_ostream.h"

namespace
{
constexpr double kMinFixedFrequencySeconds = 1.0 / 240.0;

std::size_t ToResourceIndex(ComputeSharedResourceId id)
{
	return static_cast<std::size_t>(id);
}
}

void ComputeTaskRunner::Clear()
{
	m_entries.clear();
	m_dispatch_order.clear();
	m_schedule_dirty = true;
}

void ComputeTaskRunner::Register(ComputeTask& task, DispatchCallback dispatch)
{
	m_entries.push_back(Entry{
		.task = &task,
		.dispatch = std::move(dispatch),
		.accumulated_time = 0.0,
		.executed_once = false,
	});
	m_schedule_dirty = true;
}

bool ComputeTaskRunner::InitializeEntry(Entry& entry, ID3D11Device* device, ID3D11DeviceContext* context)
{
	entry.accumulated_time = 0.0;
	entry.executed_once = false;

	if (entry.task == nullptr)
	{
		return false;
	}

	if (entry.task->Initialize(device, context))
	{
		return true;
	}

	hal::dout << "ComputeTaskRunner: compute task disabled -> " << entry.task->DebugName() << std::endl;
	return false;
}

bool ComputeTaskRunner::ReloadEntry(Entry& entry, ID3D11Device* device, ID3D11DeviceContext* context)
{
	entry.accumulated_time = 0.0;
	entry.executed_once = false;

	if (entry.task == nullptr)
	{
		return false;
	}

	entry.task->Finalize();
	if (entry.task->Initialize(device, context))
	{
		return true;
	}

	hal::dout << "ComputeTaskRunner: compute task reload failed -> " << entry.task->DebugName() << std::endl;
	return false;
}

bool ComputeTaskRunner::InitializeAll(ID3D11Device* device, ID3D11DeviceContext* context)
{
	RebuildDispatchOrder();
	bool all_initialized = true;
	for (const std::size_t index : m_dispatch_order)
	{
		all_initialized &= InitializeEntry(m_entries[index], device, context);
	}
	return all_initialized;
}

bool ComputeTaskRunner::ReloadAll(ID3D11Device* device, ID3D11DeviceContext* context)
{
	RebuildDispatchOrder();
	bool all_reloaded = true;
	for (const std::size_t index : m_dispatch_order)
	{
		all_reloaded &= ReloadEntry(m_entries[index], device, context);
	}
	return all_reloaded;
}

void ComputeTaskRunner::FinalizeAll()
{
	for (Entry& entry : m_entries)
	{
		if (entry.task != nullptr)
		{
			entry.task->Finalize();
		}
		entry.accumulated_time = 0.0;
		entry.executed_once = false;
	}
}

bool ComputeTaskRunner::RebuildDispatchOrder()
{
	m_dispatch_order.clear();
	if (m_entries.empty())
	{
		m_schedule_dirty = false;
		return true;
	}

	struct EdgeList
	{
		std::vector<std::size_t> outgoing{};
		int indegree = 0;
	};

	std::vector<EdgeList> graph(m_entries.size());
	std::array<int, static_cast<std::size_t>(ComputeSharedResourceId::Count)> writers{};
	writers.fill(-1);
	bool schedule_valid = true;

	for (std::size_t task_index = 0; task_index < m_entries.size(); ++task_index)
	{
		const Entry& entry = m_entries[task_index];
		if (entry.task == nullptr)
		{
			continue;
		}

		for (const ComputeSharedResourceId resource_id : entry.task->WriteResources())
		{
			const std::size_t resource_index = ToResourceIndex(resource_id);
			if (writers[resource_index] >= 0 && writers[resource_index] != static_cast<int>(task_index))
			{
				hal::dout << "ComputeTaskRunner: duplicate writer detected for resource "
						  << resource_index
						  << " between tasks "
						  << m_entries[writers[resource_index]].task->DebugName()
						  << " and "
						  << entry.task->DebugName()
						  << std::endl;
				schedule_valid = false;
				continue;
			}
			writers[resource_index] = static_cast<int>(task_index);
		}
	}

	auto add_edge = [&graph](std::size_t from, std::size_t to)
	{
		if (from == to)
		{
			return;
		}

		std::vector<std::size_t>& outgoing = graph[from].outgoing;
		if (std::find(outgoing.begin(), outgoing.end(), to) != outgoing.end())
		{
			return;
		}

		outgoing.push_back(to);
		++graph[to].indegree;
	};

	for (std::size_t task_index = 0; task_index < m_entries.size(); ++task_index)
	{
		const Entry& entry = m_entries[task_index];
		if (entry.task == nullptr)
		{
			continue;
		}

		for (const ComputeSharedResourceId resource_id : entry.task->ReadResources())
		{
			const int writer_index = writers[ToResourceIndex(resource_id)];
			if (writer_index >= 0)
			{
				add_edge(static_cast<std::size_t>(writer_index), task_index);
			}
		}
	}

	std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<std::size_t>> ready_queue{};
	for (std::size_t task_index = 0; task_index < graph.size(); ++task_index)
	{
		if (graph[task_index].indegree == 0)
		{
			ready_queue.push(task_index);
		}
	}

	while (!ready_queue.empty())
	{
		const std::size_t task_index = ready_queue.top();
		ready_queue.pop();
		m_dispatch_order.push_back(task_index);

		for (const std::size_t next_index : graph[task_index].outgoing)
		{
			--graph[next_index].indegree;
			if (graph[next_index].indegree == 0)
			{
				ready_queue.push(next_index);
			}
		}
	}

	if (m_dispatch_order.size() != m_entries.size())
	{
		hal::dout << "ComputeTaskRunner: dependency cycle detected, falling back to registration order" << std::endl;
		m_dispatch_order.resize(m_entries.size());
		std::iota(m_dispatch_order.begin(), m_dispatch_order.end(), 0);
		schedule_valid = false;
	}

	m_schedule_dirty = false;
	return schedule_valid;
}

void ComputeTaskRunner::Dispatch(double current_time, double elapsed_time)
{
	if (m_schedule_dirty)
	{
		RebuildDispatchOrder();
	}

	for (const std::size_t index : m_dispatch_order)
	{
		Entry& entry = m_entries[index];
		if (entry.task == nullptr || entry.dispatch == nullptr || !entry.task->IsValid())
		{
			continue;
		}

		switch (entry.task->DispatchMode())
		{
		case ComputeTaskDispatchMode::Manual:
			break;
		case ComputeTaskDispatchMode::Once:
			if (!entry.executed_once)
			{
				entry.dispatch(current_time, elapsed_time);
				entry.executed_once = true;
			}
			break;
		case ComputeTaskDispatchMode::EveryFrame:
			entry.dispatch(current_time, elapsed_time);
			entry.executed_once = true;
			break;
		case ComputeTaskDispatchMode::FixedFrequency:
		{
			const double interval_seconds =
				std::max(entry.task->FixedFrequencySeconds(), kMinFixedFrequencySeconds);
			if (!entry.executed_once)
			{
				entry.dispatch(current_time, elapsed_time);
				entry.executed_once = true;
				entry.accumulated_time = 0.0;
				break;
			}

			entry.accumulated_time += elapsed_time;
			if (entry.accumulated_time + 1.0e-9 < interval_seconds)
			{
				break;
			}

			entry.dispatch(current_time, elapsed_time);
			entry.accumulated_time = std::fmod(entry.accumulated_time, interval_seconds);
			break;
		}
		}
	}
}
