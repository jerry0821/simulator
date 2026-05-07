#include "shader_water.h"

#include <DirectXMath.h>
#include <fstream>
#include "debug_ostream.h"
#include "direct3d.h"
#include "sampler.h"

using namespace DirectX;

namespace
{
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11Buffer* g_pVSConstantBuffer0 = nullptr;
ID3D11Buffer* g_pVSConstantBuffer1 = nullptr;
ID3D11Buffer* g_pVSConstantBuffer2 = nullptr;
ID3D11Buffer* g_pPSConstantBuffer0 = nullptr;
ID3D11Buffer* g_pPSConstantBuffer1 = nullptr;
ID3D11ShaderResourceView* g_pWaterSurfaceHeightSRV = nullptr;
ID3D11ShaderResourceView* g_pSurfaceWaterSRV = nullptr;
ID3D11ShaderResourceView* g_pFlowFieldSRV = nullptr;
ID3D11ShaderResourceView* g_pSceneDepthSRV = nullptr;

struct WaterSurfaceSettings
{
	XMFLOAT3 camera_position{ 0.0f, 0.0f, 0.0f };
	float fresnel_power = 4.5f;
	float highlight_strength = 0.28f;
	float time_seconds = 0.0f;
	float surface_center_x = 0.0f;
	float surface_center_z = 0.0f;
	float surface_size_x = 512.0f;
	float surface_size_z = 512.0f;
	float padding0 = 0.0f;
	float padding1 = 0.0f;
};

WaterSurfaceSettings g_surface_settings{};
}

bool ShaderWater_Initialize()
{
	HRESULT hr = S_OK;

	std::ifstream ifs_vs("resource/shader/shader_vertex_water.cso", std::ios::binary);
	if (!ifs_vs)
	{
		MessageBox(nullptr, "頂点シェーダーの読み込みに失敗しました\n\nshader_vertex_water.cso", "エラー", MB_OK);
		return false;
	}

	ifs_vs.seekg(0, std::ios::end);
	const std::streamsize vs_filesize = ifs_vs.tellg();
	ifs_vs.seekg(0, std::ios::beg);

	unsigned char* vs_binary = new unsigned char[vs_filesize];
	ifs_vs.read(reinterpret_cast<char*>(vs_binary), vs_filesize);
	ifs_vs.close();

	hr = Direct3D_GetDevice()->CreateVertexShader(vs_binary, vs_filesize, nullptr, &g_pVertexShader);
	if (FAILED(hr))
	{
		hal::dout << "ShaderWater_Initialize(): failed to create vertex shader" << std::endl;
		delete[] vs_binary;
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	hr = Direct3D_GetDevice()->CreateInputLayout(
		layout,
		static_cast<UINT>(std::size(layout)),
		vs_binary,
		vs_filesize,
		&g_pInputLayout);

	delete[] vs_binary;

	if (FAILED(hr))
	{
		hal::dout << "ShaderWater_Initialize(): failed to create input layout" << std::endl;
		return false;
	}

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(XMFLOAT4X4);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	Direct3D_GetDevice()->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer0);
	Direct3D_GetDevice()->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer1);
	Direct3D_GetDevice()->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer2);

	std::ifstream ifs_ps("resource/shader/shader_pixel_water.cso", std::ios::binary);
	if (!ifs_ps)
	{
		MessageBox(nullptr, "ピクセルシェーダーの読み込みに失敗しました\n\nshader_pixel_water.cso", "エラー", MB_OK);
		return false;
	}

	ifs_ps.seekg(0, std::ios::end);
	const std::streamsize ps_filesize = ifs_ps.tellg();
	ifs_ps.seekg(0, std::ios::beg);

	unsigned char* ps_binary = new unsigned char[ps_filesize];
	ifs_ps.read(reinterpret_cast<char*>(ps_binary), ps_filesize);
	ifs_ps.close();

	hr = Direct3D_GetDevice()->CreatePixelShader(ps_binary, ps_filesize, nullptr, &g_pPixelShader);
	delete[] ps_binary;
	if (FAILED(hr))
	{
		hal::dout << "ShaderWater_Initialize(): failed to create pixel shader" << std::endl;
		return false;
	}

	buffer_desc.ByteWidth = sizeof(XMFLOAT4);
	Direct3D_GetDevice()->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer0);
	buffer_desc.ByteWidth = sizeof(WaterSurfaceSettings);
	Direct3D_GetDevice()->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer1);

	return true;
}

void ShaderWater_Finalize()
{
	SAFE_RELEASE(g_pPSConstantBuffer1);
	SAFE_RELEASE(g_pPSConstantBuffer0);
	SAFE_RELEASE(g_pPixelShader);
	SAFE_RELEASE(g_pVSConstantBuffer2);
	SAFE_RELEASE(g_pVSConstantBuffer1);
	SAFE_RELEASE(g_pVSConstantBuffer0);
	SAFE_RELEASE(g_pInputLayout);
	SAFE_RELEASE(g_pVertexShader);
	g_pWaterSurfaceHeightSRV = nullptr;
	g_pSurfaceWaterSRV = nullptr;
	g_pFlowFieldSRV = nullptr;
	g_pSceneDepthSRV = nullptr;
}

void ShaderWater_SetWorldMatrix(const XMMATRIX& matrix)
{
	XMFLOAT4X4 transpose{};
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));
	Direct3D_GetContext()->UpdateSubresource(g_pVSConstantBuffer0, 0, nullptr, &transpose, 0, 0);
}

void ShaderWater_SetViewMatrix(const XMMATRIX& matrix)
{
	XMFLOAT4X4 transpose{};
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));
	Direct3D_GetContext()->UpdateSubresource(g_pVSConstantBuffer1, 0, nullptr, &transpose, 0, 0);
}

void ShaderWater_SetProjMatrix(const XMMATRIX& matrix)
{
	XMFLOAT4X4 transpose{};
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));
	Direct3D_GetContext()->UpdateSubresource(g_pVSConstantBuffer2, 0, nullptr, &transpose, 0, 0);
}

void ShaderWater_SetMaterialColor(const XMFLOAT4& material_color)
{
	Direct3D_GetContext()->UpdateSubresource(g_pPSConstantBuffer0, 0, nullptr, &material_color, 0, 0);
}

void ShaderWater_SetCameraPosition(const XMFLOAT3& camera_position)
{
	g_surface_settings.camera_position = camera_position;
	Direct3D_GetContext()->UpdateSubresource(g_pPSConstantBuffer1, 0, nullptr, &g_surface_settings, 0, 0);
}

void ShaderWater_SetSurfaceSettings(float fresnel_power,
									float highlight_strength,
									float time_seconds,
									float surface_center_x,
									float surface_center_z,
									float surface_size_x,
									float surface_size_z)
{
	g_surface_settings.fresnel_power = fresnel_power;
	g_surface_settings.highlight_strength = highlight_strength;
	g_surface_settings.time_seconds = time_seconds;
	g_surface_settings.surface_center_x = surface_center_x;
	g_surface_settings.surface_center_z = surface_center_z;
	g_surface_settings.surface_size_x = surface_size_x;
	g_surface_settings.surface_size_z = surface_size_z;
	Direct3D_GetContext()->UpdateSubresource(g_pPSConstantBuffer1, 0, nullptr, &g_surface_settings, 0, 0);
}

void ShaderWater_SetWaterSurfaceHeight(ID3D11ShaderResourceView* water_surface_height_srv)
{
	g_pWaterSurfaceHeightSRV = water_surface_height_srv;
}

void ShaderWater_SetSurfaceWater(ID3D11ShaderResourceView* surface_water_srv)
{
	g_pSurfaceWaterSRV = surface_water_srv;
}

void ShaderWater_SetFlowField(ID3D11ShaderResourceView* flow_field_srv)
{
	g_pFlowFieldSRV = flow_field_srv;
}

void ShaderWater_SetSceneDepth(ID3D11ShaderResourceView* scene_depth_srv)
{
	g_pSceneDepthSRV = scene_depth_srv;
}

void ShaderWater_Begin()
{
	Direct3D_GetContext()->VSSetShader(g_pVertexShader, nullptr, 0);
	Direct3D_GetContext()->PSSetShader(g_pPixelShader, nullptr, 0);
	Direct3D_GetContext()->IASetInputLayout(g_pInputLayout);
	Direct3D_GetContext()->VSSetConstantBuffers(0, 1, &g_pVSConstantBuffer0);
	Direct3D_GetContext()->VSSetConstantBuffers(1, 1, &g_pVSConstantBuffer1);
	Direct3D_GetContext()->VSSetConstantBuffers(2, 1, &g_pVSConstantBuffer2);
	Direct3D_GetContext()->PSSetConstantBuffers(0, 1, &g_pPSConstantBuffer0);
	Direct3D_GetContext()->PSSetConstantBuffers(1, 1, &g_pPSConstantBuffer1);
	ID3D11ShaderResourceView* vs_srvs[2] = { g_pWaterSurfaceHeightSRV, g_pSurfaceWaterSRV };
	Direct3D_GetContext()->VSSetShaderResources(0, 2, vs_srvs);
	ID3D11ShaderResourceView* ps_srvs[2] = { g_pFlowFieldSRV, g_pSceneDepthSRV };
	Direct3D_GetContext()->PSSetShaderResources(1, 2, ps_srvs);
	Backend::DX11::Sampler::SetAnisotropicFilter();
}

void ShaderWater_End()
{
	ID3D11ShaderResourceView* null_srvs[3] = { nullptr, nullptr, nullptr };
	ID3D11Buffer* null_ps_buffer = nullptr;
	Direct3D_GetContext()->VSSetShaderResources(0, 2, null_srvs);
	Direct3D_GetContext()->PSSetShaderResources(0, 3, null_srvs);
	Direct3D_GetContext()->PSSetConstantBuffers(1, 1, &null_ps_buffer);
}
