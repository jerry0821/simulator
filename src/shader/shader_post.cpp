#include "shader_post.h"

#include <d3d11.h>
#include <fstream>
#include <vector>

#include "debug_ostream.h"
#include "direct3d.h"

namespace
{
struct PostVertex
{
	float position[4];
	float uv[2];
};

ID3D11Buffer* g_post_vertex_buffer = nullptr;
ID3D11VertexShader* g_post_vertex_shader = nullptr;
ID3D11InputLayout* g_post_input_layout = nullptr;
}

bool ShaderPost_Initialize()
{
	const PostVertex vertices[] = {
		{{-1.0f,  1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
		{{ 1.0f,  1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
		{{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
		{{ 1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
	};

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.ByteWidth = sizeof(vertices);
	buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA init_data{};
	init_data.pSysMem = vertices;

	if (FAILED(Direct3D_GetDevice()->CreateBuffer(
		&buffer_desc,
		&init_data,
		&g_post_vertex_buffer)))
	{
		return false;
	}

	std::ifstream input("resource/shader/shader_vertex_post.cso", std::ios::binary);
	if (!input)
	{
		hal::dout << "ShaderPost_Initialize(): failed to open shader_vertex_post.cso" << std::endl;
		return false;
	}

	input.seekg(0, std::ios::end);
	const std::streamsize size = input.tellg();
	input.seekg(0, std::ios::beg);

	std::vector<char> shader_data(static_cast<size_t>(size));
	input.read(shader_data.data(), size);

	if (FAILED(Direct3D_GetDevice()->CreateVertexShader(
		shader_data.data(),
		shader_data.size(),
		nullptr,
		&g_post_vertex_shader)))
	{
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	if (FAILED(Direct3D_GetDevice()->CreateInputLayout(
		layout,
		ARRAYSIZE(layout),
		shader_data.data(),
		shader_data.size(),
		&g_post_input_layout)))
	{
		return false;
	}

	return true;
}

void ShaderPost_Finalize()
{
	SAFE_RELEASE(g_post_input_layout);
	SAFE_RELEASE(g_post_vertex_shader);
	SAFE_RELEASE(g_post_vertex_buffer);
}

void ShaderPost_BindVS()
{
	Direct3D_GetContext()->VSSetShader(g_post_vertex_shader, nullptr, 0);
	Direct3D_GetContext()->IASetInputLayout(g_post_input_layout);
}

void ShaderPost_Draw()
{
	const UINT stride = sizeof(PostVertex);
	const UINT offset = 0;

	Direct3D_GetContext()->IASetVertexBuffers(0, 1, &g_post_vertex_buffer, &stride, &offset);
	Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Direct3D_GetContext()->Draw(4, 0);
}
