#ifndef SCENE_RENDER_ADAPTER_H
#define SCENE_RENDER_ADAPTER_H

#include "render_scene.h"

class SceneController;
struct WaterSurfaceDesc;

class SceneRenderAdapter : public RenderScene
{
public:
	explicit SceneRenderAdapter(SceneController& scene_controller);

	void drawShadow(const RenderFrameContext& frame_context) override;
	void drawDepthPrePass(const RenderFrameContext& frame_context) override;
	void drawForwardOpaque(const RenderFrameContext& frame_context) override;
	bool getWaterSurfaceDesc(WaterSurfaceDesc& out_desc) override;
	void drawTransparency(const RenderFrameContext& frame_context) override;
	void drawParticles(const RenderFrameContext& frame_context) override;
	void drawUI(const RenderFrameContext& frame_context) override;

private:
	SceneController& m_scene_controller;
};

#endif // SCENE_RENDER_ADAPTER_H
