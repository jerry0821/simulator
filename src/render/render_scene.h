#ifndef RENDER_SCENE_H
#define RENDER_SCENE_H

struct WaterSurfaceDesc;
struct RenderFrameContext;

class RenderScene
{
public:
	virtual ~RenderScene() = default;

	virtual void drawShadow(const RenderFrameContext& frame_context) = 0;
	virtual void drawDepthPrePass(const RenderFrameContext& frame_context) = 0;
	virtual void drawForwardOpaque(const RenderFrameContext& frame_context) = 0;
	virtual bool getWaterSurfaceDesc(WaterSurfaceDesc& out_desc) = 0;
	virtual void drawTransparency(const RenderFrameContext& frame_context) = 0;
	virtual void drawParticles(const RenderFrameContext& frame_context) = 0;
	virtual void drawUI(const RenderFrameContext& frame_context) = 0;
};

#endif // RENDER_SCENE_H
