#ifndef MATERIAL_PASS_H
#define MATERIAL_PASS_H

#include <DirectXMath.h>

#include "material_type.h"
#include "render_state.h"

class MaterialPass
{
public:
	MaterialPass(MaterialType material_type, RenderState render_state);

	void begin(const DirectX::XMMATRIX& world_matrix,
	           const DirectX::XMFLOAT4& material_color) const;
	void end() const;

private:
	MaterialType material_type_;
	RenderState render_state_;
};

#endif // MATERIAL_PASS_H
