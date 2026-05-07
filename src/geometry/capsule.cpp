#include "capsule.h"
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

void Capsule_Initialize(ID3D11Device *pDevice, ID3D11DeviceContext *pContext) {
  g_pDevice = pDevice;
  g_pContext = pContext;

  int slices = 32;
  int rings = 16; // Half-sphere rings
  float radius = 0.5f;
  float cylinderHalfHeight = 0.5f;

  std::vector<Vertex3D> vertices;
  std::vector<unsigned short> indices;

  // 1. Top Hemisphere (rings 0 to rings/2)
  for (int i = 0; i <= rings / 2; ++i) {
    float v = (float)i / (rings / 2.0f); // 0 (top pole) to 1 (equator)
    float phi = v * XM_PIDIV2;           // 0 to PI/2

    for (int j = 0; j <= slices; ++j) {
      float u = (float)j / slices;
      float theta = u * XM_2PI;

      float x = radius * sinf(phi) * cosf(theta);
      float y = radius * cosf(phi) + cylinderHalfHeight;
      float z = radius * sinf(phi) * sinf(theta);

      XMFLOAT3 normal = {sinf(phi) * cosf(theta), cosf(phi),
                         sinf(phi) * sinf(theta)};

      vertices.push_back({{x, y, z}, normal, {1, 1, 1, 1}, {u, v * 0.25f}});
    }
  }

  // 2. Bottom Hemisphere (rings/2 to rings)
  for (int i = 0; i <= rings / 2; ++i) {
    float v = (float)i / (rings / 2.0f);   // 0 (equator) to 1 (bottom pole)
    float phi = XM_PIDIV2 + v * XM_PIDIV2; // PI/2 to PI

    for (int j = 0; j <= slices; ++j) {
      float u = (float)j / slices;
      float theta = u * XM_2PI;

      float x = radius * sinf(phi) * cosf(theta);
      float y = radius * cosf(phi) - cylinderHalfHeight;
      float z = radius * sinf(phi) * sinf(theta);

      XMFLOAT3 normal = {sinf(phi) * cosf(theta), cosf(phi),
                         sinf(phi) * sinf(theta)};

      // Map bottom hemisphere UVs
      vertices.push_back(
          {{x, y, z}, normal, {1, 1, 1, 1}, {u, 0.75f + v * 0.25f}});
    }
  }

  // Connect Cylinder Body
  // The equator of the top hemisphere is the last ring added in phase 1.
  // The equator of the bottom hemisphere is the first ring added in phase 2.
  // We just add indices for the top hemisphere, cylinder body, and bottom
  // hemisphere.

  // Indices for Top Hemisphere
  int ringVertexCount = slices + 1;
  for (int i = 0; i < rings / 2; ++i) {
    for (int j = 0; j < slices; ++j) {
      unsigned short v1 = i * ringVertexCount + j;
      unsigned short v2 = (i + 1) * ringVertexCount + j;
      unsigned short v3 = i * ringVertexCount + (j + 1);
      unsigned short v4 = (i + 1) * ringVertexCount + (j + 1);

      indices.push_back(v3);
      indices.push_back(v2);
      indices.push_back(v1);

      indices.push_back(v4);
      indices.push_back(v2);
      indices.push_back(v3);
    }
  }

  // Indices for Cylinder Body connecting Top Equator to Bottom Equator
  int topEquatorStart = (rings / 2) * ringVertexCount;
  int bottomEquatorStart = (rings / 2 + 1) * ringVertexCount;

  for (int j = 0; j < slices; ++j) {
    unsigned short top1 = topEquatorStart + j;
    unsigned short bottom1 = bottomEquatorStart + j;
    unsigned short top2 = topEquatorStart + (j + 1);
    unsigned short bottom2 = bottomEquatorStart + (j + 1);

    indices.push_back(top2);
    indices.push_back(bottom1);
    indices.push_back(top1);

    indices.push_back(bottom2);
    indices.push_back(bottom1);
    indices.push_back(top2);
  }

  // Indices for Bottom Hemisphere
  for (int i = 0; i < rings / 2; ++i) {
    for (int j = 0; j < slices; ++j) {
      unsigned short v1 = bottomEquatorStart + i * ringVertexCount + j;
      unsigned short v2 = bottomEquatorStart + (i + 1) * ringVertexCount + j;
      unsigned short v3 = bottomEquatorStart + i * ringVertexCount + (j + 1);
      unsigned short v4 =
          bottomEquatorStart + (i + 1) * ringVertexCount + (j + 1);

      indices.push_back(v3);
      indices.push_back(v2);
      indices.push_back(v1);

      indices.push_back(v4);
      indices.push_back(v2);
      indices.push_back(v3);
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

void Capsule_Finalize(void) {
  SAFE_RELEASE(g_pVertexBuffer);
  SAFE_RELEASE(g_pIndexBuffer);
}

void Capsule_Update(double elapsed_Time) {
  // Not used right now
}

void Capsule_Draw(int texId,
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

void Capsule_DrawShadow(const DirectX::XMMATRIX &mtxWorld) {
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

