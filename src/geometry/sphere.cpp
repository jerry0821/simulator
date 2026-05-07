#include "sphere.h"
#include "direct3d.h"
#include "shader3d.h"
#include "shader_shadow.h"
#include "material_pass.h"
#include "texture.h"
#include <cmath>
#include <vector>

using namespace DirectX;

struct Vertex3D {
  XMFLOAT3 position;
  XMFLOAT3 normal;
  XMFLOAT4 color;
  XMFLOAT2 texcoord;
};

static ID3D11Buffer *g_pVertexBuffer = nullptr;
static ID3D11Buffer *g_pIndexBuffer = nullptr;
static int g_IndexCount = 0;

static ID3D11Device *g_pDevice = nullptr;
static ID3D11DeviceContext *g_pContext = nullptr;

void Sphere_Initialize(ID3D11Device *pDevice, ID3D11DeviceContext *pContext) {
  g_pDevice = pDevice;
  g_pContext = pContext;

  int slices = 32;
  int stacks = 16;
  float radius = 0.5f;

  std::vector<Vertex3D> vertices;
  std::vector<unsigned short> indices;

  // Generate Vertices
  for (int i = 0; i <= stacks; ++i) {
    float v = (float)i / stacks;
    float phi = v * XM_PI;

    for (int j = 0; j <= slices; ++j) {
      float u = (float)j / slices;
      float theta = u * XM_2PI;

      float x = radius * sinf(phi) * cosf(theta);
      float y = radius * cosf(phi);
      float z = radius * sinf(phi) * sinf(theta);

      XMFLOAT3 pos(x, y, z);
      XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&pos));
      XMFLOAT3 normal;
      XMStoreFloat3(&normal, n);

      vertices.push_back({pos, normal, {1.0f, 1.0f, 1.0f, 1.0f}, {u, v}});
    }
  }

  // Generate Indices
  for (int i = 0; i < stacks; ++i) {
    for (int j = 0; j < slices; ++j) {
      unsigned short top = i * (slices + 1) + j;
      unsigned short bottom = (i + 1) * (slices + 1) + j;

      indices.push_back(top);
      indices.push_back(bottom);
      indices.push_back(top + 1);

      indices.push_back(top + 1);
      indices.push_back(bottom);
      indices.push_back(bottom + 1);
    }
  }

  g_IndexCount = (int)indices.size();

  // Create Vertex Buffer
  D3D11_BUFFER_DESC vbDesc = {};
  vbDesc.Usage = D3D11_USAGE_DEFAULT;
  vbDesc.ByteWidth = sizeof(Vertex3D) * (UINT)vertices.size();
  vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  D3D11_SUBRESOURCE_DATA vbInitData = {};
  vbInitData.pSysMem = vertices.data();
  g_pDevice->CreateBuffer(&vbDesc, &vbInitData, &g_pVertexBuffer);

  // Create Index Buffer
  D3D11_BUFFER_DESC ibDesc = {};
  ibDesc.Usage = D3D11_USAGE_DEFAULT;
  ibDesc.ByteWidth = sizeof(unsigned short) * (UINT)indices.size();
  ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  D3D11_SUBRESOURCE_DATA ibInitData = {};
  ibInitData.pSysMem = indices.data();
  g_pDevice->CreateBuffer(&ibDesc, &ibInitData, &g_pIndexBuffer);
}

void Sphere_Finalize(void) {
  SAFE_RELEASE(g_pVertexBuffer);
  SAFE_RELEASE(g_pIndexBuffer);
}

void Sphere_Draw(int texId,
                 const DirectX::XMMATRIX &mtxWorld,
                 MaterialType material_type) {
  if (!g_pVertexBuffer || !g_pIndexBuffer)
    return;

  const MaterialPass material_pass{material_type,
                                   RenderState{DepthMode::ReadWrite}};
  material_pass.begin(mtxWorld, {1.0f, 1.0f, 1.0f, 1.0f});

  UINT stride = sizeof(Vertex3D);
  UINT offset = 0;
  g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
  g_pContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
  g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  Texture_SetTexture(texId);

  g_pContext->DrawIndexed(g_IndexCount, 0, 0);
  material_pass.end();
}

void Sphere_DrawShadow(const DirectX::XMMATRIX &mtxWorld) {
  if (!g_pVertexBuffer || !g_pIndexBuffer)
    return;

  UINT stride = sizeof(Vertex3D);
  UINT offset = 0;
  g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
  g_pContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
  g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ShaderShadow_SetWorldMatrix(mtxWorld);
  g_pContext->DrawIndexed(g_IndexCount, 0, 0);
}

