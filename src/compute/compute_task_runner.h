#ifndef COMPUTE_TASK_RUNNER_H
#define COMPUTE_TASK_RUNNER_H

#include <cstddef>
#include <functional>
#include <vector>

#include "compute_task.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

class ComputeTaskRunner
{
public:
	using DispatchCallback = std::function<void(double current_time, double elapsed_time)>;

	struct Entry
	{
		ComputeTask* task = nullptr;
		DispatchCallback dispatch{};
		double accumulated_time = 0.0;
		bool executed_once = false;
	};

	void Clear();
	void Register(ComputeTask& task, DispatchCallback dispatch);

	bool InitializeAll(ID3D11Device* device, ID3D11DeviceContext* context);
	bool ReloadAll(ID3D11Device* device, ID3D11DeviceContext* context);
	void FinalizeAll();
	void Dispatch(double current_time, double elapsed_time);

private:
	static bool InitializeEntry(Entry& entry, ID3D11Device* device, ID3D11DeviceContext* context);
	static bool ReloadEntry(Entry& entry, ID3D11Device* device, ID3D11DeviceContext* context);
	bool RebuildDispatchOrder();

	std::vector<Entry> m_entries{};
	std::vector<std::size_t> m_dispatch_order{};
	bool m_schedule_dirty = true;
};

#endif // COMPUTE_TASK_RUNNER_H
