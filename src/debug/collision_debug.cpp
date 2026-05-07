// ----------------------------------------------------
// コリジョンデバッグ描画 [collision_debug.cpp]
// ====================================================
// collision.cpp から分離されたデバッグ描画関数群。
// Version: 1.1 (Modern C++ Refactor)
// ----------------------------------------------------

#include "collision.h"
#include "direct3d.h"
#include "shader.h"
#include "shader3d.h"
#include "texture.h"

#include <algorithm>
#include <cmath>
#include <wrl/client.h>
using namespace DirectX;
using Microsoft::WRL::ComPtr;

#include "camera.h"

namespace Collision {
namespace Debug {

struct DebugVertex {
  XMFLOAT3 position;
  XMFLOAT3 normal;
  XMFLOAT4 color;
  XMFLOAT2 uv;
};

static constexpr int MAX_VERTICES = 200;

static ComPtr<ID3D11Buffer> g_pVertexBuffer = nullptr;
static ID3D11DeviceContext *g_pContext = nullptr;
static int g_WhiteTexId = -1;

struct Vertex {
  XMFLOAT3 position;
  XMFLOAT3 normal;
  XMFLOAT4 color;
  XMFLOAT2 uv;
};

void Initialize(ID3D11Device *pDevice, ID3D11DeviceContext *pContext) {
  g_pContext = Direct3D_GetContext();
  D3D11_BUFFER_DESC bd = {};
  bd.Usage = D3D11_USAGE_DYNAMIC;
  bd.ByteWidth = sizeof(DebugVertex) * MAX_VERTICES;
  bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  
  // Use GetAddressOf() for ComPtr
  Direct3D_GetDevice()->CreateBuffer(&bd, nullptr, g_pVertexBuffer.GetAddressOf());
  
  g_WhiteTexId = Texture_Load(L"resource/texture/white.png");
  if (g_WhiteTexId < 0) g_WhiteTexId = 0;
}

void Finalize() {
  // ComPtr automatically releases, but we can reset it explicitly
  g_pVertexBuffer.Reset();
}

void Draw(const Circle &circle, const DirectX::XMFLOAT4 &color) {
  int numVertex = (int)(circle.radius * 2.0f * XM_PI + 1);
  Shader_Begin();
  D3D11_MAPPED_SUBRESOURCE msr;
  g_pContext->Map(g_pVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
  Vertex *v = (Vertex *)msr.pData;
  const float rad = XM_2PI / numVertex;
  for (int i = 0; i < numVertex; i++) {
    v[i].position.x = cosf(rad * i) * circle.radius + circle.center.x;
    v[i].position.y = sinf(rad * i) * circle.radius + circle.center.y;
    v[i].position.z = 0.0f;
    v[i].color = color;
    v[i].uv = {0.0f, 0.0f};
  }
  g_pContext->Unmap(g_pVertexBuffer.Get(), 0);
  Shader_SetWorldMatrix(XMMatrixIdentity());
  UINT stride = sizeof(Vertex); UINT offset = 0;
  g_pContext->IASetVertexBuffers(0, 1, g_pVertexBuffer.GetAddressOf(), &stride, &offset);
  g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
  Texture_SetTexture(g_WhiteTexId);
  g_pContext->Draw(numVertex, 0);
}

void Draw(const Box &box, const DirectX::XMFLOAT4 &color) {
  Shader_Begin();
  D3D11_MAPPED_SUBRESOURCE msr;
  g_pContext->Map(g_pVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
  Vertex *v = (Vertex *)msr.pData;
  v[0].position = {box.center.x - box.half_width, box.center.y - box.half_height, 0.0f};
  v[1].position = {box.center.x + box.half_width, box.center.y - box.half_height, 0.0f};
  v[2].position = {box.center.x + box.half_width, box.center.y + box.half_height, 0.0f};
  v[3].position = {box.center.x - box.half_width, box.center.y + box.half_height, 0.0f};
  v[4].position = {box.center.x - box.half_width, box.center.y - box.half_height, 0.0f};
  for (int i = 0; i < 5; i++) { v[i].color = color; v[i].uv = {0.0f, 0.0f}; }
  g_pContext->Unmap(g_pVertexBuffer.Get(), 0);
  Shader_SetWorldMatrix(XMMatrixIdentity());
  UINT stride = sizeof(Vertex); UINT offset = 0;
  g_pContext->IASetVertexBuffers(0, 1, g_pVertexBuffer.GetAddressOf(), &stride, &offset);
  g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
  Texture_SetTexture(g_WhiteTexId);
  g_pContext->Draw(5, 0);
}

void Draw(const AABB &aabb, const DirectX::XMFLOAT4 &color) {
  const int numVertex = 24;
  Shader3D_Begin();
  D3D11_MAPPED_SUBRESOURCE msr;
  if (FAILED(g_pContext->Map(g_pVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr))) return;
  Vertex *v = (Vertex *)msr.pData;
  XMFLOAT3 p[8];
  p[0]={aabb.min.x,aabb.min.y,aabb.min.z}; p[1]={aabb.max.x,aabb.min.y,aabb.min.z};
  p[2]={aabb.min.x,aabb.max.y,aabb.min.z}; p[3]={aabb.max.x,aabb.max.y,aabb.min.z};
  p[4]={aabb.min.x,aabb.min.y,aabb.max.z}; p[5]={aabb.max.x,aabb.min.y,aabb.max.z};
  p[6]={aabb.min.x,aabb.max.y,aabb.max.z}; p[7]={aabb.max.x,aabb.max.y,aabb.max.z};
  int indices[]={0,1,2,3,4,5,6,7,0,2,1,3,4,6,5,7,0,4,1,5,2,6,3,7};
  for (int i = 0; i < numVertex; i++) {
    v[i].position = p[indices[i]]; v[i].normal = XMFLOAT3(0,1,0); v[i].color = color; v[i].uv = {0,0};
  }
  g_pContext->Unmap(g_pVertexBuffer.Get(), 0);
  Shader3D_SetWorldMatrix(XMMatrixIdentity());
  UINT stride = sizeof(Vertex); UINT offset = 0;
  g_pContext->IASetVertexBuffers(0, 1, g_pVertexBuffer.GetAddressOf(), &stride, &offset);
  g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
  Texture_SetTexture(g_WhiteTexId);
  g_pContext->Draw(numVertex, 0);
}

void DrawOBB(const AABB &localAabb, const DirectX::XMMATRIX &mtxWorld, const DirectX::XMFLOAT4 &color) {
  const int numVertex = 24;
  Shader3D_Begin();
  D3D11_MAPPED_SUBRESOURCE msr;
  if (FAILED(g_pContext->Map(g_pVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr))) return;
  Vertex *v = (Vertex *)msr.pData;
  XMFLOAT3 p[8]={{localAabb.min.x,localAabb.min.y,localAabb.min.z},{localAabb.max.x,localAabb.min.y,localAabb.min.z},
    {localAabb.min.x,localAabb.max.y,localAabb.min.z},{localAabb.max.x,localAabb.max.y,localAabb.min.z},
    {localAabb.min.x,localAabb.min.y,localAabb.max.z},{localAabb.max.x,localAabb.min.y,localAabb.max.z},
    {localAabb.min.x,localAabb.max.y,localAabb.max.z},{localAabb.max.x,localAabb.max.y,localAabb.max.z}};
  int indices[]={0,1,2,3,4,5,6,7,0,2,1,3,4,6,5,7,0,4,1,5,2,6,3,7};
  for (int i = 0; i < numVertex; i++) {
    v[i].position = p[indices[i]]; v[i].normal = XMFLOAT3(0,1,0); v[i].color = color; v[i].uv = {0,0};
  }
  g_pContext->Unmap(g_pVertexBuffer.Get(), 0);
  Shader3D_SetWorldMatrix(mtxWorld);
  UINT stride = sizeof(Vertex); UINT offset = 0;
  g_pContext->IASetVertexBuffers(0, 1, g_pVertexBuffer.GetAddressOf(), &stride, &offset);
  g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
  Texture_SetTexture(g_WhiteTexId);
  g_pContext->Draw(numVertex, 0);
}

void DrawSphereMtx(float radius, const DirectX::XMMATRIX &mtxWorld, const DirectX::XMFLOAT4 &color) {
  const int segments = 16;
  Shader3D_Begin();
  D3D11_MAPPED_SUBRESOURCE msr;
  if (FAILED(g_pContext->Map(g_pVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr))) return;
  Vertex *v = (Vertex *)msr.pData;
  int idx = 0;
  float step = XM_2PI / segments;
  for (int i = 0; i < segments; ++i) {
    float c1=cosf(step*i)*radius,s1=sinf(step*i)*radius,c2=cosf(step*(i+1))*radius,s2=sinf(step*(i+1))*radius;
    v[idx].position={c1,0,s1};v[idx].normal={0,1,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={c2,0,s2};v[idx].normal={0,1,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={c1,s1,0};v[idx].normal={0,1,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={c2,s2,0};v[idx].normal={0,1,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={0,c1,s1};v[idx].normal={1,0,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={0,c2,s2};v[idx].normal={1,0,0};v[idx].color=color;v[idx++].uv={0,0};
  }
  g_pContext->Unmap(g_pVertexBuffer.Get(), 0);
  Shader3D_SetWorldMatrix(mtxWorld);
  UINT stride = sizeof(Vertex); UINT offset = 0;
  g_pContext->IASetVertexBuffers(0, 1, g_pVertexBuffer.GetAddressOf(), &stride, &offset);
  g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
  Texture_SetTexture(g_WhiteTexId);
  g_pContext->Draw(idx, 0);
}

void DrawCapsuleMtx(float radius, float halfHeight, const DirectX::XMMATRIX &mtxWorld, const DirectX::XMFLOAT4 &color) {
  const int segments = 16;
  Shader3D_Begin();
  D3D11_MAPPED_SUBRESOURCE msr;
  if (FAILED(g_pContext->Map(g_pVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr))) return;
  Vertex *v = (Vertex *)msr.pData;
  int idx = 0;
  float step = XM_2PI / segments;
  for (int i = 0; i < segments; ++i) {
    float c1=cosf(step*i)*radius,s1=sinf(step*i)*radius,c2=cosf(step*(i+1))*radius,s2=sinf(step*(i+1))*radius;
    v[idx].position={c1,halfHeight,s1};v[idx].normal={0,1,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={c2,halfHeight,s2};v[idx].normal={0,1,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={c1,-halfHeight,s1};v[idx].normal={0,-1,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={c2,-halfHeight,s2};v[idx].normal={0,-1,0};v[idx].color=color;v[idx++].uv={0,0};
  }
  for (int i = 0; i < 4; ++i) {
    float c1=cosf(step*(i*4))*radius,s1=sinf(step*(i*4))*radius;
    v[idx].position={c1,halfHeight,s1};v[idx].normal={1,0,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={c1,-halfHeight,s1};v[idx].normal={1,0,0};v[idx].color=color;v[idx++].uv={0,0};
  }
  int hemiSeg = segments / 2;
  float hStep = XM_PI / hemiSeg;
  for (int i = 0; i < hemiSeg; ++i) {
    float hc1=cosf(hStep*i)*radius,hs1=sinf(hStep*i)*radius,hc2=cosf(hStep*(i+1))*radius,hs2=sinf(hStep*(i+1))*radius;
    v[idx].position={hs1,halfHeight+hc1,0};v[idx].normal={0,1,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={hs2,halfHeight+hc2,0};v[idx].normal={0,1,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={0,halfHeight+hc1,hs1};v[idx].normal={1,0,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={0,halfHeight+hc2,hs2};v[idx].normal={1,0,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={hs1,-halfHeight-hc1,0};v[idx].normal={0,-1,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={hs2,-halfHeight-hc2,0};v[idx].normal={0,-1,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={0,-halfHeight-hc1,hs1};v[idx].normal={-1,0,0};v[idx].color=color;v[idx++].uv={0,0};
    v[idx].position={0,-halfHeight-hc2,hs2};v[idx].normal={-1,0,0};v[idx].color=color;v[idx++].uv={0,0};
  }
  g_pContext->Unmap(g_pVertexBuffer.Get(), 0);
  Shader3D_SetWorldMatrix(mtxWorld);
  UINT stride = sizeof(Vertex); UINT offset = 0;
  g_pContext->IASetVertexBuffers(0, 1, g_pVertexBuffer.GetAddressOf(), &stride, &offset);
  g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
  Texture_SetTexture(g_WhiteTexId);
  g_pContext->Draw(idx, 0);
}

void DrawConeOBB(const DirectX::XMFLOAT3 &baseCenter, const DirectX::XMFLOAT3 &tip, float radius, const DirectX::XMMATRIX &mtxWorld, const DirectX::XMFLOAT4 &color) {
  const int segments = 16;
  const int numVertex = segments * 4;
  Shader3D_Begin();
  D3D11_MAPPED_SUBRESOURCE msr;
  if (FAILED(g_pContext->Map(g_pVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr))) return;
  Vertex *v = (Vertex *)msr.pData;
  XMVECTOR vBase=XMLoadFloat3(&baseCenter),vTip=XMLoadFloat3(&tip),vDir=XMVector3Normalize(vTip-vBase);
  XMVECTOR vUp=XMVectorSet(0,1,0,0);
  if(std::abs(XMVectorGetY(vDir))>0.99f) vUp=XMVectorSet(1,0,0,0);
  XMVECTOR vRight=XMVector3Normalize(XMVector3Cross(vUp,vDir));
  vUp=XMVector3Normalize(XMVector3Cross(vDir,vRight));
  XMFLOAT3 bp[16];
  for(int i=0;i<segments;i++){float a=(XM_2PI*i)/segments;XMStoreFloat3(&bp[i],vBase+vRight*(radius*cosf(a))+vUp*(radius*sinf(a)));}
  int vi=0;
  for(int i=0;i<segments;i++){
    int n=(i+1)%segments;
    v[vi].position=bp[i];v[vi].normal=XMFLOAT3(0,1,0);v[vi].color=color;v[vi].uv=XMFLOAT2(0,0);vi++;
    v[vi].position=bp[n];v[vi].normal=XMFLOAT3(0,1,0);v[vi].color=color;v[vi].uv=XMFLOAT2(0,0);vi++;
    v[vi].position=bp[i];v[vi].normal=XMFLOAT3(0,1,0);v[vi].color=color;v[vi].uv=XMFLOAT2(0,0);vi++;
    v[vi].position=tip;  v[vi].normal=XMFLOAT3(0,1,0);v[vi].color=color;v[vi].uv=XMFLOAT2(0,0);vi++;
  }
  g_pContext->Unmap(g_pVertexBuffer.Get(), 0);
  Shader3D_SetWorldMatrix(mtxWorld);
  UINT stride=sizeof(Vertex);UINT offset=0;
  g_pContext->IASetVertexBuffers(0,1,g_pVertexBuffer.GetAddressOf(),&stride,&offset);
  g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
  Texture_SetTexture(g_WhiteTexId);
  g_pContext->Draw(numVertex,0);
}

void Draw(const Sphere &sphere, const DirectX::XMFLOAT4 &color) {
  const int segments=24;
  const int vpr=segments+1;
  XMMATRIX view=XMLoadFloat4x4(&Camera_GetMatrix()),proj=XMLoadFloat4x4(&Camera_GetPerspectiveMatrix());
  Shader3D_Begin();
  Shader3D_SetViewMatrix(view);
  Shader3D_SetProjMatrix(proj);
  D3D11_MAPPED_SUBRESOURCE msr;
  if(FAILED(g_pContext->Map(g_pVertexBuffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&msr))) return;
  Vertex33 *v=(Vertex33*)msr.pData;
  float as=XM_2PI/segments;
  for(int i=0;i<=segments;i++){float a=i*as;v[i].position={sphere.center.x+cosf(a)*sphere.radius,sphere.center.y,sphere.center.z+sinf(a)*sphere.radius};v[i].normal={0,1,0};v[i].color=color;v[i].texcoord={0,0};}
  for(int i=0;i<=segments;i++){float a=i*as;v[vpr+i].position={sphere.center.x+cosf(a)*sphere.radius,sphere.center.y+sinf(a)*sphere.radius,sphere.center.z};v[vpr+i].normal={0,1,0};v[vpr+i].color=color;v[vpr+i].texcoord={0,0};}
  for(int i=0;i<=segments;i++){float a=i*as;v[vpr*2+i].position={sphere.center.x,sphere.center.y+cosf(a)*sphere.radius,sphere.center.z+sinf(a)*sphere.radius};v[vpr*2+i].normal={0,1,0};v[vpr*2+i].color=color;v[vpr*2+i].texcoord={0,0};}
  g_pContext->Unmap(g_pVertexBuffer.Get(),0);
  Shader3D_SetWorldMatrix(XMMatrixIdentity());
  UINT stride=sizeof(Vertex);UINT offset=0;
  g_pContext->IASetVertexBuffers(0,1,g_pVertexBuffer.GetAddressOf(),&stride,&offset);
  g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
  Texture_SetTexture(g_WhiteTexId);
  g_pContext->Draw(vpr,0);
  g_pContext->Draw(vpr,vpr);
  g_pContext->Draw(vpr,vpr*2);
}

void DrawSphere(const Sphere &sphere, const XMFLOAT4 &color) {
  if(!g_pVertexBuffer) return;
  const int segments=32;
  const int vpr=segments+1;
  D3D11_MAPPED_SUBRESOURCE msr;
  if(FAILED(g_pContext->Map(g_pVertexBuffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&msr))) return;
  DebugVertex *v=(DebugVertex*)msr.pData;
  float as=XM_2PI/segments;
  for(int i=0;i<=segments;i++){float a=i*as;v[i].position={sphere.center.x+cosf(a)*sphere.radius,sphere.center.y,sphere.center.z+sinf(a)*sphere.radius};v[i].normal={0,1,0};v[i].color=color;v[i].uv={0,0};}
  for(int i=0;i<=segments;i++){float a=i*as;v[vpr+i].position={sphere.center.x+cosf(a)*sphere.radius,sphere.center.y+sinf(a)*sphere.radius,sphere.center.z};v[vpr+i].normal={0,0,1};v[vpr+i].color=color;v[vpr+i].uv={0,0};}
  for(int i=0;i<=segments;i++){float a=i*as;v[vpr*2+i].position={sphere.center.x,sphere.center.y+cosf(a)*sphere.radius,sphere.center.z+sinf(a)*sphere.radius};v[vpr*2+i].normal={1,0,0};v[vpr*2+i].color=color;v[vpr*2+i].uv={0,0};}
  g_pContext->Unmap(g_pVertexBuffer.Get(),0);
  Shader3D_SetWorldMatrix(XMMatrixIdentity());
  Shader3D_Begin();
  UINT stride=sizeof(DebugVertex);UINT offset=0;
  g_pContext->IASetVertexBuffers(0,1,g_pVertexBuffer.GetAddressOf(),&stride,&offset);
  g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
  Texture_SetTexture(g_WhiteTexId);
  g_pContext->Draw(vpr,0);g_pContext->Draw(vpr,vpr);g_pContext->Draw(vpr,vpr*2);
}

void DrawTest() {
  if(!g_pVertexBuffer) return;
  D3D11_MAPPED_SUBRESOURCE msr;
  if(FAILED(g_pContext->Map(g_pVertexBuffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&msr))) return;
  DebugVertex *v=(DebugVertex*)msr.pData;
  v[0].position={0,0.5f,0};v[0].color={100,0,0,1};v[0].normal={0,0,1};v[0].uv={0,0};
  v[1].position={-0.5f,-0.5f,0};v[1].color={0,100,0,1};v[1].normal={0,0,1};v[1].uv={0,0};
  v[2].position={0.5f,-0.5f,0};v[2].color={0,0,100,1};v[2].normal={0,0,1};v[2].uv={0,0};
  g_pContext->Unmap(g_pVertexBuffer.Get(),0);
  Shader3D_SetWorldMatrix(XMMatrixIdentity());
  Shader3D_Begin();
  UINT stride=sizeof(DebugVertex);UINT offset=0;
  g_pContext->IASetVertexBuffers(0,1,g_pVertexBuffer.GetAddressOf(),&stride,&offset);
  g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  Texture_SetTexture(g_WhiteTexId);
  g_pContext->Draw(3,0);
}

} // namespace Debug
} // namespace Collision
