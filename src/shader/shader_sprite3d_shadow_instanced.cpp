// ----------------------------------------------------
// 3D sprite shadow instanced shader [shader_sprite3d_shadow_instanced.cpp]
// ====================================================

#include "shader_sprite3d_shadow_instanced.h"

#include <d3d11.h>
#include <cmath>
#include <fstream>
#include <vector>

#include "compute_texture_dimensions.h"
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
ID3D11ShaderResourceView* g_wind_field_srv = nullptr;
ID3D11ShaderResourceView* g_instance_buffer_srv = nullptr;

struct Sprite3DShadowWindConstants
{
    float time_seconds = 0.0f;
    float wind_dir_x = 1.0f;
    float wind_dir_y = 0.0f;
    float wind_strength = 0.0f;
    float world_min_x = -ComputeTextureDimensions::kWorldHalfExtent;
    float world_max_x = ComputeTextureDimensions::kWorldHalfExtent;
    float world_min_z = -ComputeTextureDimensions::kWorldHalfExtent;
    float world_max_z = ComputeTextureDimensions::kWorldHalfExtent;
};
} // namespace

bool ShaderSprite3D_ShadowInstanced_Initialize()
{
    std::ifstream ifs_vs("resource/shader/shader_vertex_sprite3d_shadow_instanced.cso", std::ios::binary);
    if (!ifs_vs)
    {
        MessageBox(nullptr, "sprite3d shadow instanced vertex shader load failed", "Error", MB_OK);
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
        hal::dout << "ShaderSprite3D_ShadowInstanced: failed to create VS" << std::endl;
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    hr = device->CreateInputLayout(
        layout,
        ARRAYSIZE(layout),
        vs_binary.data(),
        static_cast<SIZE_T>(vs_binary.size()),
        &g_input_layout);
    if (FAILED(hr))
    {
        hal::dout << "ShaderSprite3D_ShadowInstanced: failed to create input layout" << std::endl;
        return false;
    }

    std::ifstream ifs_ps("resource/shader/shader_pixel_3d_shadow_cutout.cso", std::ios::binary);
    if (!ifs_ps)
    {
        MessageBox(nullptr, "sprite3d shadow instanced pixel shader load failed", "Error", MB_OK);
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
        hal::dout << "ShaderSprite3D_ShadowInstanced: failed to create PS" << std::endl;
        return false;
    }

    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = sizeof(XMFLOAT4X4);
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    device->CreateBuffer(&buffer_desc, nullptr, &g_vs_constant_buffer0);

    buffer_desc.ByteWidth = sizeof(Sprite3DShadowWindConstants);
    device->CreateBuffer(&buffer_desc, nullptr, &g_vs_constant_buffer1);

    return true;
}

void ShaderSprite3D_ShadowInstanced_Finalize()
{
    g_wind_field_srv = nullptr;
    g_instance_buffer_srv = nullptr;
    SAFE_RELEASE(g_vs_constant_buffer1);
    SAFE_RELEASE(g_vs_constant_buffer0);
    SAFE_RELEASE(g_input_layout);
    SAFE_RELEASE(g_pixel_shader);
    SAFE_RELEASE(g_vertex_shader);
}

void ShaderSprite3D_ShadowInstanced_SetLightViewProjection(const XMMATRIX& matrix)
{
    XMFLOAT4X4 transpose{};
    XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));
    Direct3D_GetContext()->UpdateSubresource(g_vs_constant_buffer0, 0, nullptr, &transpose, 0, 0);
}

void ShaderSprite3D_ShadowInstanced_SetWindField(ID3D11ShaderResourceView* wind_field_srv)
{
    g_wind_field_srv = wind_field_srv;
}

void ShaderSprite3D_ShadowInstanced_SetInstanceBuffer(ID3D11ShaderResourceView* instance_buffer_srv)
{
    g_instance_buffer_srv = instance_buffer_srv;
}

void ShaderSprite3D_ShadowInstanced_SetWindSettings(float time_seconds,
                                                    const XMFLOAT2& wind_direction,
                                                    float wind_strength,
                                                    float world_min_x,
                                                    float world_max_x,
                                                    float world_min_z,
                                                    float world_max_z)
{
    const float length_sq =
        wind_direction.x * wind_direction.x + wind_direction.y * wind_direction.y;
    XMFLOAT2 normalized_direction = { 1.0f, 0.0f };
    if (length_sq > 1.0e-6f)
    {
        const float inv_length = 1.0f / std::sqrt(length_sq);
        normalized_direction.x = wind_direction.x * inv_length;
        normalized_direction.y = wind_direction.y * inv_length;
    }

    const Sprite3DShadowWindConstants constants{
        time_seconds,
        normalized_direction.x,
        normalized_direction.y,
        wind_strength,
        world_min_x,
        world_max_x,
        world_min_z,
        world_max_z
    };
    Direct3D_GetContext()->UpdateSubresource(g_vs_constant_buffer1, 0, nullptr, &constants, 0, 0);
}

void ShaderSprite3D_ShadowInstanced_Begin()
{
    auto* context = Direct3D_GetContext();
    context->VSSetShader(g_vertex_shader, nullptr, 0);
    context->PSSetShader(g_pixel_shader, nullptr, 0);
    context->IASetInputLayout(g_input_layout);

    ID3D11Buffer* vs_buffers[2] = { g_vs_constant_buffer0, g_vs_constant_buffer1 };
    context->VSSetConstantBuffers(0, 2, vs_buffers);

    ID3D11ShaderResourceView* vs_srvs[2] = { g_wind_field_srv, g_instance_buffer_srv };
    context->VSSetShaderResources(0, 2, vs_srvs);

    Backend::DX11::Sampler::SetAnisotropicFilter();
}

void ShaderSprite3D_ShadowInstanced_Clear()
{
    ID3D11ShaderResourceView* null_vs_srvs[2] = { nullptr, nullptr };
    ID3D11ShaderResourceView* null_ps_srv = nullptr;
    Direct3D_GetContext()->VSSetShaderResources(0, 2, null_vs_srvs);
    Direct3D_GetContext()->PSSetShaderResources(0, 1, &null_ps_srv);
    g_wind_field_srv = nullptr;
    g_instance_buffer_srv = nullptr;
}
