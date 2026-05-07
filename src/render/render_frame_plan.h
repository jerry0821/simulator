#ifndef RENDER_FRAME_PLAN_H
#define RENDER_FRAME_PLAN_H

#include <string_view>
#include <vector>

#include "render_resource_usage.h"

struct RenderPassPlan
{
	std::string_view pass_name{};
	std::vector<RenderResourceUsage> resources;
};

struct RenderFramePlan
{
	std::vector<RenderPassPlan> passes;
};

#endif // RENDER_FRAME_PLAN_H
