#include "renderer.h"

#include "depth_prepass.h"
#include "forward_opaque_pass.h"
#include "particle_pass.h"
#include "post_process_pass.h"
#include "render_pass.h"
#include "shadow_pass.h"
#include "transparency_pass.h"
#include "ui_pass.h"
#include "water_pass.h"

Renderer::Renderer(RenderBackendDX11& backend)
	: backend_(backend)
{
	InstallDefaultPasses();
}

RenderFramePlan Renderer::BuildFramePlan() const
{
	RenderFramePlan frame_plan;
	frame_plan.passes.reserve(passes_.size());

	for (const auto& pass : passes_)
	{
		RenderPassPlan pass_plan{};
		pass_plan.pass_name = pass->name();

		for (const auto& usage : pass->resources())
		{
			pass_plan.resources.push_back(usage);
		}

		frame_plan.passes.push_back(std::move(pass_plan));
	}

	return frame_plan;
}

void Renderer::RenderFrame(const RenderFrameContext& frame_context)
{
	backend_.beginFrame();
	RenderFrameContext pass_context = frame_context;
	pass_context.resources.scene_color = backend_.sceneColorResource();
	pass_context.resources.scene_depth = backend_.sceneDepthResource();

	if (forward_opaque_pass_ != nullptr && shadow_pass_ != nullptr)
	{
		forward_opaque_pass_->setShadowShaderResourceView(
			shadow_pass_->shadowShaderResourceView());
	}

	if (transparency_pass_ != nullptr && shadow_pass_ != nullptr)
	{
		transparency_pass_->setShadowShaderResourceView(
			shadow_pass_->shadowShaderResourceView());
	}

	for (const auto& pass : passes_)
	{
		pass->execute(pass_context);
	}

	backend_.endFrame();
}

void Renderer::InstallDefaultPasses()
{
	auto shadow_pass = std::make_unique<ShadowPass>(backend_);
	shadow_pass_ = shadow_pass.get();
	passes_.push_back(std::move(shadow_pass));

	passes_.push_back(std::make_unique<DepthPrePass>(backend_));

	auto forward_opaque_pass = std::make_unique<ForwardOpaquePass>(backend_);
	forward_opaque_pass_ = forward_opaque_pass.get();
	passes_.push_back(std::move(forward_opaque_pass));

	passes_.push_back(std::make_unique<WaterPass>(backend_));

	auto transparency_pass = std::make_unique<TransparencyPass>(backend_);
	transparency_pass_ = transparency_pass.get();
	passes_.push_back(std::move(transparency_pass));

	passes_.push_back(std::make_unique<ParticlePass>(backend_));
	passes_.push_back(std::make_unique<PostProcessPass>(backend_));
	passes_.push_back(std::make_unique<UIPass>());
}
