#ifndef RENDER_PASS_H
#define RENDER_PASS_H

#include <span>
#include <string_view>

struct RenderResourceUsage;
struct RenderFrameContext;

class RenderPass
{
public:
	virtual ~RenderPass() = default;

	virtual std::string_view name() const = 0;
	virtual std::span<const RenderResourceUsage> resources() const = 0;
	virtual void execute(const RenderFrameContext& frame_context) = 0;
};

#endif // RENDER_PASS_H
