#include "shader_particle.h"
#include "direct3d.h" 
#include "debug_ostream.h" 
#include "sampler.h"
#include <fstream>
#include <vector>
using namespace DirectX;
#include "texture.h"

static ID3D11VertexShader* g_pVertexShader = nullptr;
static ID3D11PixelShader* g_pPixelShader = nullptr;
static ID3D11InputLayout* g_pInputLayout = nullptr;
static ID3D11Buffer* g_pConstantBuffer = nullptr;
static ID3D11Buffer* g_pDynamicVertexBuffer = nullptr;
static const int MAX_BATCH_SIZE = 4000;

struct CB_Matrix {
    XMFLOAT4X4 World;
    XMFLOAT4X4 View;
    XMFLOAT4X4 Projection;
};


static XMMATRIX g_WorldMatrix;
static XMMATRIX g_ViewMatrix;
static XMMATRIX g_ProjectionMatrix;

static bool LoadShaderFile(const char* filename, std::vector<char>& buffer)
{
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) return false;

    ifs.seekg(0, std::ios::end);
    size_t size = (size_t)ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    buffer.resize(size);
    ifs.read(buffer.data(), size);
    return true;
}

bool ShaderParticle_Initialize()
{
    HRESULT hr;

    // 行列の初期化
    g_WorldMatrix = XMMatrixIdentity();
    g_ViewMatrix = XMMatrixIdentity();
    g_ProjectionMatrix = XMMatrixIdentity();

    // 1. 頂点シェーダー読み込み
    std::vector<char> vsData;
    if (!LoadShaderFile("resource/shader/shader_vertex_particle.cso", vsData)) {
        hal::dout << "shader_vertex_particle読み込み失敗" << std::endl;
        return false;
    }

    hr = Direct3D_GetDevice()->CreateVertexShader(vsData.data(), vsData.size(), nullptr, &g_pVertexShader);
    if (FAILED(hr)) return false;


    // 2. 入力レイアウト作成 (★ここが重要！ VertexParticle 構造体に合わせる)
    // 構造: Position(float3), Color(float4), UV(float2)
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = Direct3D_GetDevice()->CreateInputLayout(layout, ARRAYSIZE(layout), vsData.data(), vsData.size(), &g_pInputLayout);
    if (FAILED(hr)) return false;


    // 3. ピクセルシェーダー読み込み
    std::vector<char> psData;
    // もし shader_pixel_particle.cso がなければ、一時的に shader_pixel_2d.cso を使っても動きます
    if (!LoadShaderFile("resource/shader/shader_pixel_particle.cso", psData)) {
        hal::dout << "shader_pixel_particle読み込み失敗" << std::endl;
        return false;
    }

    hr = Direct3D_GetDevice()->CreatePixelShader(psData.data(), psData.size(), nullptr, &g_pPixelShader);
    if (FAILED(hr)) return false;


    // 4. 定数バッファ作成 (World, View, Projection)
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(CB_Matrix);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    hr = Direct3D_GetDevice()->CreateBuffer(&bd, nullptr, &g_pConstantBuffer);
    if (FAILED(hr)) return false;

    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DYNAMIC; // CPU書き込み可
    bd.ByteWidth = sizeof(VertexParticle) * MAX_BATCH_SIZE * 6; // 容量確保
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = Direct3D_GetDevice()->CreateBuffer(&bd, nullptr, &g_pDynamicVertexBuffer);
    if (FAILED(hr)) return false;

    return true;
}

void ShaderParticle_Finalize()
{
	SAFE_RELEASE(g_pVertexShader);
	SAFE_RELEASE(g_pPixelShader);
	SAFE_RELEASE(g_pInputLayout);
	SAFE_RELEASE(g_pConstantBuffer);
	SAFE_RELEASE(g_pDynamicVertexBuffer);
}

void ShaderParticle_SetWorldMatrix(const XMMATRIX& world) { g_WorldMatrix = world; }
void ShaderParticle_SetViewMatrix(const XMMATRIX& view) { g_ViewMatrix = view; }
void ShaderParticle_SetProjectionMatrix(const XMMATRIX& projection) { g_ProjectionMatrix = projection; }

void ShaderParticle_Begin()
{
    if (!Direct3D_GetContext()) return;

    // 1. 定数バッファの更新 (CPU -> GPU)
    CB_Matrix data;
    XMStoreFloat4x4(&data.World, XMMatrixTranspose(g_WorldMatrix));
    XMStoreFloat4x4(&data.View, XMMatrixTranspose(g_ViewMatrix));
    XMStoreFloat4x4(&data.Projection, XMMatrixTranspose(g_ProjectionMatrix));

    Direct3D_GetContext()->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &data, 0, 0);

    // 2. シェーダーの設定
    Direct3D_GetContext()->VSSetShader(g_pVertexShader, nullptr, 0);
    Direct3D_GetContext()->PSSetShader(g_pPixelShader, nullptr, 0);

    // 3. 入力レイアウトの設定 (Particle専用)
    Direct3D_GetContext()->IASetInputLayout(g_pInputLayout);

    // 4. 定数バッファの設定 (Vertex Shader b0)
    Direct3D_GetContext()->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);

    // 5. サンプラーの設定 (もし画像を使うなら必要)
    Backend::DX11::Sampler::SetLinearFilter();

}

void ShaderParticle_DrawBatch(const std::vector<VertexParticle>& vertices, int texID)
{
    if (vertices.empty()) return;
    if (!Direct3D_GetContext()) return;

    ID3D11ShaderResourceView* pSRV = TextureManager::GetSRV(texID);
    Direct3D_GetContext()->PSSetShaderResources(0, 1, &pSRV);

    D3D11_MAPPED_SUBRESOURCE msr;
    
    Direct3D_GetContext()->Map(g_pDynamicVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);//  D3D11_MAP_WRITE_NO_OVERWRITE 

    VertexParticle* dataDest = (VertexParticle*)msr.pData;
    size_t copySize = vertices.size() * sizeof(VertexParticle);
    memcpy(dataDest, vertices.data(), copySize);

    Direct3D_GetContext()->Unmap(g_pDynamicVertexBuffer, 0);

    UINT stride = sizeof(VertexParticle);
    UINT offset = 0;
    Direct3D_GetContext()->IASetVertexBuffers(0, 1, &g_pDynamicVertexBuffer, &stride, &offset);
    Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    Direct3D_GetContext()->Draw((UINT)vertices.size(), 0);
}



