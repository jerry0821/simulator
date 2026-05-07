#include "shader_shadow.h"

#include <d3d11.h>
#include <fstream>
#include <vector>

#include "direct3d.h"

namespace
{
struct ShadowConstants
{
	DirectX::XMFLOAT4X4 world;
	DirectX::XMFLOAT4X4 light_view_projection;
};

ID3D11VertexShader* g_shadow_vs = nullptr;
ID3D11InputLayout* g_shadow_input_layout = nullptr;
ID3D11Buffer* g_shadow_constant_buffer = nullptr;
ID3D11RasterizerState* g_shadow_rasterizer_state = nullptr;
ID3D11RasterizerState* g_previous_rasterizer_state = nullptr;
ShadowConstants g_shadow_constants = {};
}

bool ShaderShadow_Initialize()
{
	std::ifstream ifs("resource/shader/shader_vertex_shadow.cso", std::ios::binary);
	if (!ifs) {
		MessageBox(nullptr, "shadow vertex shader load failed", "Error", MB_OK);
		return false;
	}

	ifs.seekg(0, std::ios::end);
	const std::streamsize file_size = ifs.tellg();
	ifs.seekg(0, std::ios::beg);

	std::vector<char> bytecode(static_cast<size_t>(file_size));
	ifs.read(bytecode.data(), file_size);

	HRESULT hr = Direct3D_GetDevice()->CreateVertexShader(
		bytecode.data(), bytecode.size(), nullptr, &g_shadow_vs);
	if (FAILED(hr)) return false;

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	hr = Direct3D_GetDevice()->CreateInputLayout(
		layout, ARRAYSIZE(layout), bytecode.data(), bytecode.size(), &g_shadow_input_layout);
	if (FAILED(hr)) return false;

	D3D11_BUFFER_DESC cb_desc = {};
	cb_desc.ByteWidth = sizeof(ShadowConstants);
	cb_desc.Usage = D3D11_USAGE_DEFAULT;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = Direct3D_GetDevice()->CreateBuffer(&cb_desc, nullptr, &g_shadow_constant_buffer);
	if (FAILED(hr)) return false;

	D3D11_RASTERIZER_DESC rs_desc = {};
	rs_desc.FillMode = D3D11_FILL_SOLID;
	rs_desc.CullMode = D3D11_CULL_BACK;
	rs_desc.DepthClipEnable = TRUE;
	rs_desc.SlopeScaledDepthBias = 2.0f;
	rs_desc.DepthBias = 2000;
	rs_desc.DepthBiasClamp = 0.0f;
	hr = Direct3D_GetDevice()->CreateRasterizerState(&rs_desc, &g_shadow_rasterizer_state);
	if (FAILED(hr)) return false;

	return true;
}

void ShaderShadow_Finalize()
{
	SAFE_RELEASE(g_shadow_rasterizer_state);
	SAFE_RELEASE(g_shadow_constant_buffer);
	SAFE_RELEASE(g_shadow_input_layout);
	SAFE_RELEASE(g_shadow_vs);
}

void ShaderShadow_Begin()
{
	auto* context = Direct3D_GetContext();
	context->RSGetState(&g_previous_rasterizer_state);
	context->VSSetShader(g_shadow_vs, nullptr, 0);
	context->PSSetShader(nullptr, nullptr, 0);
	context->IASetInputLayout(g_shadow_input_layout);
	context->VSSetConstantBuffers(0, 1, &g_shadow_constant_buffer);
	context->RSSetState(g_shadow_rasterizer_state);
}

void ShaderShadow_End()
{
	auto* context = Direct3D_GetContext();
	context->RSSetState(g_previous_rasterizer_state);
	SAFE_RELEASE(g_previous_rasterizer_state);
}

void ShaderShadow_SetWorldMatrix(const DirectX::XMMATRIX& world)
{
	DirectX::XMStoreFloat4x4(&g_shadow_constants.world, DirectX::XMMatrixTranspose(world));
	Direct3D_GetContext()->UpdateSubresource(g_shadow_constant_buffer, 0, nullptr, &g_shadow_constants, 0, 0);
}

void ShaderShadow_SetViewProjection(const DirectX::XMMATRIX& view_projection)
{
	DirectX::XMStoreFloat4x4(
		&g_shadow_constants.light_view_projection,
		DirectX::XMMatrixTranspose(view_projection));
	Direct3D_GetContext()->UpdateSubresource(g_shadow_constant_buffer, 0, nullptr, &g_shadow_constants, 0, 0);
}

void ShaderShadow_SetLightViewProjection(const DirectX::XMMATRIX& light_view_projection)
{
	ShaderShadow_SetViewProjection(light_view_projection);
}
