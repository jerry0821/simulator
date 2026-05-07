#include "shader_sprite3d_transparent_instanced.h"

#include <d3d11.h>
#include <fstream>
#include <vector>

#include "debug_ostream.h"
#include "direct3d.h"
#include "sampler.h"

using namespace DirectX;

namespace
{
ID3D11VertexShader* g_vertex_shader = nullptr;
ID3D11PixelShader* g_pixel_shader = nullptr;
ID3D11InputLayout* g_input_layout = nullptr;
ID3D11Buffer* g_vs_constant_buffer0 = nullptr;
ID3D11Buffer* g_vs_constant_buffer1 = nullptr;
ID3D11Buffer* g_ps_constant_buffer0 = nullptr;
}

bool ShaderSprite3D_TransparentInstanced_Initialize()
{
	std::ifstream ifs_vs("resource/shader/shader_vertex_sprite3d_transparent_instanced.cso", std::ios::binary);
	if (!ifs_vs)
	{
		MessageBox(nullptr, "sprite3d transparent instanced vertex shader load failed", "Error", MB_OK);
		return false;
	}

	ifs_vs.seekg(0, std::ios::end);
	const std::streamsize vs_filesize = ifs_vs.tellg();
	ifs_vs.seekg(0, std::ios::beg);

	std::vector<unsigned char> vs_binary(static_cast<size_t>(vs_filesize));
	ifs_vs.read(reinterpret_cast<char*>(vs_binary.data()), vs_filesize);

	auto* device = Direct3D_GetDevice();
	HRESULT hr = device->CreateVertexShader(
		vs_binary.data(),
		static_cast<SIZE_T>(vs_binary.size()),
		nullptr,
		&g_vertex_shader);
	if (FAILED(hr))
	{
		hal::dout << "ShaderSprite3D_TransparentInstanced: failed to create VS" << std::endl;
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,   0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,   0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,   0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,   0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,                             D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16,                            D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32,                            D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "TEXCOORD", 4, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48,                            D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "COLOR",    1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64,                            D3D11_INPUT_PER_INSTANCE_DATA, 1 }
	};

	hr = device->CreateInputLayout(
		layout,
		ARRAYSIZE(layout),
		vs_binary.data(),
		static_cast<SIZE_T>(vs_binary.size()),
		&g_input_layout);
	if (FAILED(hr))
	{
		hal::dout << "ShaderSprite3D_TransparentInstanced: failed to create input layout" << std::endl;
		return false;
	}

	std::ifstream ifs_ps("resource/shader/shader_pixel_3d_transparent.cso", std::ios::binary);
	if (!ifs_ps)
	{
		MessageBox(nullptr, "sprite3d transparent instanced pixel shader load failed", "Error", MB_OK);
		return false;
	}

	ifs_ps.seekg(0, std::ios::end);
	const std::streamsize ps_filesize = ifs_ps.tellg();
	ifs_ps.seekg(0, std::ios::beg);

	std::vector<unsigned char> ps_binary(static_cast<size_t>(ps_filesize));
	ifs_ps.read(reinterpret_cast<char*>(ps_binary.data()), ps_filesize);

	hr = device->CreatePixelShader(
		ps_binary.data(),
		static_cast<SIZE_T>(ps_binary.size()),
		nullptr,
		&g_pixel_shader);
	if (FAILED(hr))
	{
		hal::dout << "ShaderSprite3D_TransparentInstanced: failed to create PS" << std::endl;
		return false;
	}

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(XMFLOAT4X4);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	device->CreateBuffer(&buffer_desc, nullptr, &g_vs_constant_buffer0);
	device->CreateBuffer(&buffer_desc, nullptr, &g_vs_constant_buffer1);

	buffer_desc.ByteWidth = sizeof(XMFLOAT4);
	device->CreateBuffer(&buffer_desc, nullptr, &g_ps_constant_buffer0);

	return true;
}

void ShaderSprite3D_TransparentInstanced_Finalize()
{
	SAFE_RELEASE(g_ps_constant_buffer0);
	SAFE_RELEASE(g_vs_constant_buffer1);
	SAFE_RELEASE(g_vs_constant_buffer0);
	SAFE_RELEASE(g_input_layout);
	SAFE_RELEASE(g_pixel_shader);
	SAFE_RELEASE(g_vertex_shader);
}

void ShaderSprite3D_TransparentInstanced_SetViewMatrix(const XMMATRIX& matrix)
{
	XMFLOAT4X4 transpose{};
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));
	Direct3D_GetContext()->UpdateSubresource(g_vs_constant_buffer0, 0, nullptr, &transpose, 0, 0);
}

void ShaderSprite3D_TransparentInstanced_SetProjMatrix(const XMMATRIX& matrix)
{
	XMFLOAT4X4 transpose{};
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));
	Direct3D_GetContext()->UpdateSubresource(g_vs_constant_buffer1, 0, nullptr, &transpose, 0, 0);
}

void ShaderSprite3D_TransparentInstanced_SetMaterialColor(const XMFLOAT4& material_color)
{
	Direct3D_GetContext()->UpdateSubresource(g_ps_constant_buffer0, 0, nullptr, &material_color, 0, 0);
}

void ShaderSprite3D_TransparentInstanced_Begin()
{
	auto* context = Direct3D_GetContext();
	context->VSSetShader(g_vertex_shader, nullptr, 0);
	context->PSSetShader(g_pixel_shader, nullptr, 0);
	context->IASetInputLayout(g_input_layout);

	ID3D11Buffer* vs_buffers[2] = {
		g_vs_constant_buffer0,
		g_vs_constant_buffer1
	};
	context->VSSetConstantBuffers(0, 2, vs_buffers);
	context->PSSetConstantBuffers(0, 1, &g_ps_constant_buffer0);

	Backend::DX11::Sampler::SetAnisotropicFilter();
}

void ShaderSprite3D_TransparentInstanced_Clear()
{
}
