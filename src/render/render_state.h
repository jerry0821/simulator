#ifndef RENDER_STATE_H
#define RENDER_STATE_H

enum class DepthMode
{
	Disabled,
	ReadWrite,
	ReadOnly
};

enum class BlendMode
{
	Opaque,
	Alpha,
	Additive
};

enum class CullMode
{
	None,
	Front,
	Back
};

struct RenderState
{
	DepthMode depth_mode = DepthMode::ReadWrite;
	BlendMode blend_mode = BlendMode::Opaque;
	CullMode cull_mode = CullMode::Back;
};

#endif // RENDER_STATE_H
