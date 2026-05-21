#ifndef RENDER_SHADOW_MAP_RESOURCE_H
#define RENDER_SHADOW_MAP_RESOURCE_H

struct ID3D11ShaderResourceView;
struct ID3D11DepthStencilView;
class RenderBackendDX11;
class ForwardOpaquePass;
class TransparencyPass;
class PostProcessPass;
class ComputeNoiseTexture;
class ComputeTerrainClassificationTexture;
class ComputeGrassDataTexture;
class ComputeMeteorographTexture;
class ComputeRainMapTexture;
class ComputeWaterSurfaceHeightTexture;
class MeshFieldRenderer;
class TerrainHeightField;

namespace Backend
{
// Thin renderer-level wrapper for shader-readable render resources.
class RenderShaderResource
{
public:
	RenderShaderResource() = default;

	bool isValid() const
	{
		return native_srv_ != nullptr;
	}

	ID3D11ShaderResourceView* shaderResourceView() const
	{
		return native_srv_;
	}

private:
	explicit RenderShaderResource(ID3D11ShaderResourceView* native_srv)
		: native_srv_(native_srv)
	{
	}

	ID3D11ShaderResourceView* native_srv_ = nullptr;

	friend class ::RenderBackendDX11;
	friend class ::ForwardOpaquePass;
	friend class ::TransparencyPass;
	friend class ::PostProcessPass;
	friend class ::ComputeNoiseTexture;
	friend class ::ComputeTerrainClassificationTexture;
	friend class ::ComputeGrassDataTexture;
	friend class ::ComputeMeteorographTexture;
	friend class ::ComputeRainMapTexture;
	friend class ::ComputeWaterSurfaceHeightTexture;
	friend class ::MeshFieldRenderer;
	friend class ::TerrainHeightField;
};

class RenderDepthResource
{
public:
	RenderDepthResource() = default;

	bool isValid() const
	{
		return native_dsv_ != nullptr;
	}

	ID3D11DepthStencilView* depthStencilView() const
	{
		return native_dsv_;
	}

	ID3D11ShaderResourceView* shaderResourceView() const
	{
		return native_srv_;
	}

private:
	explicit RenderDepthResource(ID3D11DepthStencilView* native_dsv,
								 ID3D11ShaderResourceView* native_srv = nullptr)
		: native_dsv_(native_dsv)
		, native_srv_(native_srv)
	{
	}

	ID3D11DepthStencilView* native_dsv_ = nullptr;
	ID3D11ShaderResourceView* native_srv_ = nullptr;

	friend class ::RenderBackendDX11;
};

using RenderShadowMapResource = RenderShaderResource;
using RenderSceneColorResource = RenderShaderResource;
} // namespace Backend

#endif // RENDER_SHADOW_MAP_RESOURCE_H
