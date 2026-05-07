#include "cylinder.h"
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

void Cylinder_Initialize(ID3D11Device *pDevice, ID3D11DeviceContext *pContext) {
  g_pDevice = pDevice;
  g_pContext = pContext;

  int slices = 32;
  float radius = 0.5f;
  float halfHeight = 0.5f;

  std::vector<Vertex3D> vertices;
  std::vector<unsigned short> indices;

  // Body vertices
  for (int i = 0; i <= slices; ++i) {
    float u = (float)i / slices;
    float theta = u * XM_2PI;
    float c = cosf(theta);
    float s = sinf(theta);

    // Top vertex representing top edge
    vertices.push_back({{radius * c, halfHeight, radius * s},
                        {c, 0, s},
                        {1, 1, 1, 1},
                        {u, 0.0f}});
    // Bottom vertex representing bottom edge
    vertices.push_back({{radius * c, -halfHeight, radius * s},
                        {c, 0, s},
                        {1, 1, 1, 1},
                        {u, 1.0f}});
  }

  // Body indices
  for (int i = 0; i < slices; ++i) {
    unsigned short top = i * 2;
    unsigned short bottom = i * 2 + 1;
    unsigned short nextTop = (i + 1) * 2;
    unsigned short nextBottom = (i + 1) * 2 + 1;

    indices.push_back(top);
    indices.push_back(nextTop);
    indices.push_back(bottom);

    indices.push_back(nextTop);
    indices.push_back(nextBottom);
    indices.push_back(bottom);
  }

  // Caps
  int baseIndex = (int)vertices.size();

  // Top Cap Center
  vertices.push_back(
      {{0, halfHeight, 0}, {0, 1, 0}, {1, 1, 1, 1}, {0.5f, 0.5f}});
  unsigned short topCenter = baseIndex;
  baseIndex++;

  for (int i = 0; i <= slices; ++i) {
    float theta = ((float)i / slices) * XM_2PI;
    float c = cosf(theta);
    float s = sinf(theta);
    float u = c * 0.5f + 0.5f;
    float v = s * 0.5f + 0.5f;
    vertices.push_back({{radius * c, halfHeight, radius * s},
                        {0, 1, 0},
                        {1, 1, 1, 1},
                        {u, v}});
  }

  for (int i = 0; i < slices; ++i) {
    indices.push_back(topCenter);
    indices.push_back(baseIndex + i + 1);
    indices.push_back(baseIndex + i);
  }

  baseIndex += slices + 1;

  // Bottom Cap Center
  vertices.push_back(
      {{0, -halfHeight, 0}, {0, -1, 0}, {1, 1, 1, 1}, {0.5f, 0.5f}});
  unsigned short bottomCenter = baseIndex;
  baseIndex++;

  for (int i = 0; i <= slices; ++i) {
    float theta = ((float)i / slices) * XM_2PI;
    float c = cosf(theta);
    float s = sinf(theta);
    float u = c * 0.5f + 0.5f;
    float v = s * 0.5f + 0.5f;
    vertices.push_back({{radius * c, -halfHeight, radius * s},
                        {0, -1, 0},
                        {1, 1, 1, 1},
                        {u, v}});
  }

  for (int i = 0; i < slices; ++i) {
    indices.push_back(bottomCenter);
    indices.push_back(baseIndex + i);
    indices.push_back(baseIndex + i + 1);
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

void Cylinder_Finalize(void) {
  SAFE_RELEASE(g_pVertexBuffer);
  SAFE_RELEASE(g_pIndexBuffer);
}

void Cylinder_Draw(int texId,
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

void Cylinder_DrawShadow(const DirectX::XMMATRIX &mtxWorld) {
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

