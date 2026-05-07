#ifndef UI_PASS_H
#define UI_PASS_H

#include "render_pass.h"

class UIPass : public RenderPass
{
public:
	std::string_view name() const override;
	std::span<const RenderResourceUsage> resources() const override;
	void execute(const RenderFrameContext& frame_context) override;
};

#endif // UI_PASS_H
