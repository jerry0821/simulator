#ifndef RENDER_WATER_SURFACE_H
#define RENDER_WATER_SURFACE_H

#include <DirectXMath.h>

struct WaterSurfaceDesc
{
	bool enabled = false;
	float center_x = 0.0f;
	float center_z = 0.0f;
	float height = 0.0f;
	float size_x = 0.0f;
	float size_z = 0.0f;

	DirectX::XMFLOAT4 base_color = { 0.06f, 0.22f, 0.46f, 0.36f };

	float ripple_strength = 1.0f;
	float edge_emphasis = 1.0f;
};

#endif // RENDER_WATER_SURFACE_H
