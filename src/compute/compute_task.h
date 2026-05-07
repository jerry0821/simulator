#ifndef COMPUTE_TASK_H
#define COMPUTE_TASK_H

#include <span>

#include "compute_shared_resource_registry.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

enum class ComputeTaskDispatchMode
{
	Manual,
	Once,
	EveryFrame,
	FixedFrequency,
};

class ComputeTask
{
public:
	using ResourceSpan = std::span<const ComputeSharedResourceId>;

	virtual ~ComputeTask() = default;

	virtual const char* DebugName() const = 0;
	virtual ComputeTaskDispatchMode DispatchMode() const
	{
		return ComputeTaskDispatchMode::Manual;
	}

	virtual double FixedFrequencySeconds() const
	{
		return 0.0;
	}

	virtual ResourceSpan ReadResources() const
	{
		return {};
	}

	virtual ResourceSpan WriteResources() const
	{
		return {};
	}

	virtual bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) = 0;
	virtual void Finalize() = 0;
	virtual bool IsValid() const = 0;
};

#endif // COMPUTE_TASK_H
