// ----------------------------------------------------
// Map controller [map.cpp]
// ====================================================
// Created by: Jerry
// Date: 2026-03-25
// ----------------------------------------------------
#include "map.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace DirectX;

#include "camera.h"
#include "billboard.h"
#include "capsule.h"
#include "compute_grass_instances.h"
#include "compute_noise_texture.h"
#include "compute_shared_resource_registry.h"
#include "cube.h"
#include "cylinder.h"
#include "debug_menu.h"
#include "debug_ostream.h"
#include "direct3d.h"
#include "frustum_culling.h"
#include "frustum_culling_debug.h"
#include "grass_patch.h"
#include "instancing_debug.h"
#include "light.h"
#include "meshfield.h"
#include "model.h"
#include "resource_manager.h"
#include "render_frame_context.h"
#include "scene_stress_debug.h"
#include "shader3d.h"
#include "shader3d_unlit.h"
#include "shader3d_instanced.h"
#include "shader_grass_instanced.h"
#include "shader_field.h"
#include "shader_sprite3d_cutout.h"
#include "shader_sprite3d_cutout_instanced.h"
#include "shader_sprite3d_shadow_instanced.h"
#include "shader_sprite3d_transparent_instanced.h"
#include "sphere.h"
#include "sprite.h"
#include "sprite3d.h"
#include "system_timer.h"
#include "terrain_data_model.h"
#include "texture.h"
#include "wind_field_cpu.h"

namespace
{
template <typename T>
void SafeReleaseMapResource(T*& resource)
{
  if (resource != nullptr)
  {
    resource->Release();
    resource = nullptr;
  }
}

constexpr int kMaxUndo = 50;
constexpr int kInstancedGrassCount = 192;
constexpr int kInstancedBirdCount = 24;
constexpr int kClimateOverlayCols = 18;
constexpr int kClimateOverlayRows = 18;
constexpr float kClimateWorldMinX = -640.0f;
constexpr float kClimateWorldMaxX = 640.0f;
constexpr float kClimateWorldMinZ = -640.0f;
constexpr float kClimateWorldMaxZ = 640.0f;
constexpr float kDebugQuadScaleX = 2.2f;
constexpr float kDebugQuadScaleY = 3.4f;
constexpr float kTerrainGrassMargin = 14.0f;
constexpr float kTerrainGrassSpacing = 2.4f;
constexpr float kTerrainGrassLodFullDistance = 64.0f;
constexpr float kTerrainGrassLodMaxDistance = 132.0f;
constexpr float kTerrainGrassAreaCenterX = 0.0f;
constexpr float kTerrainGrassAreaCenterZ = 0.0f;
constexpr float kTerrainGrassAreaHalfExtentX = 220.0f;
constexpr float kTerrainGrassAreaHalfExtentZ = 220.0f;
constexpr float kTerrainGrassSlopeSampleOffset = 2.2f;
constexpr float kTerrainWorldHalfWidth = 256.0f;
constexpr float kTerrainWorldHalfDepth = 256.0f;
constexpr int kFloatingDustParticleCount = 96;
constexpr bool kDrawMeteorographPanelDebug = false;
constexpr bool kDrawGrass = true;
constexpr bool kDrawTerrainGrassCompute = true;
constexpr bool kDrawFloatingDustParticles = false;
constexpr double kGrassCoverageProbeIntervalSeconds = 0.75;
constexpr float kGrassCoverageClassificationSignatureThreshold = 0.020f;
ID3D11Buffer* g_tiled_vertex_buffer = nullptr;

float SampleTerrainHeightWorld(float world_x, float world_z)
{
	return TerrainDataModel::SampleHeightWorld(world_x, world_z);
}

float Frac01(float value)
{
	return value - std::floor(value);
}

float Hash11(float value)
{
	return Frac01(std::sin(value * 12.9898f + 78.233f) * 43758.5453f);
}

DirectX::XMFLOAT3 Normalize3(const DirectX::XMFLOAT3& value)
{
  const float length_sq =
      value.x * value.x + value.y * value.y + value.z * value.z;
  if (length_sq < 1.0e-6f)
  {
    return { 0.0f, 0.0f, 1.0f };
  }
  const float inv_length = 1.0f / std::sqrt(length_sq);
  return { value.x * inv_length, value.y * inv_length, value.z * inv_length };
}

float LerpScalar(float a, float b, float t)
{
  return a + (b - a) * t;
}

DirectX::XMFLOAT4 LerpColor(
    const DirectX::XMFLOAT4& a,
    const DirectX::XMFLOAT4& b,
    float t)
{
  return {
      LerpScalar(a.x, b.x, t),
      LerpScalar(a.y, b.y, t),
      LerpScalar(a.z, b.z, t),
      LerpScalar(a.w, b.w, t) };
}

DirectX::XMFLOAT4 SampleDustPalette(float seed)
{
  static const DirectX::XMFLOAT4 kPalette[] = {
      { 0.88f, 0.60f, 1.00f, 1.0f },
      { 0.62f, 0.72f, 1.00f, 1.0f },
      { 0.98f, 0.72f, 0.90f, 1.0f },
      { 0.72f, 0.88f, 1.00f, 1.0f },
      { 1.00f, 0.84f, 0.64f, 1.0f },
  };

  constexpr int kPaletteCount = static_cast<int>(std::size(kPalette));
  const float wrapped = seed - std::floor(seed);
  const float scaled = wrapped * static_cast<float>(kPaletteCount);
  const int index0 = static_cast<int>(std::floor(scaled)) % kPaletteCount;
  const int index1 = (index0 + 1) % kPaletteCount;
  const float blend = scaled - std::floor(scaled);
  return LerpColor(kPalette[index0], kPalette[index1], blend);
}

float EstimateTerrainNormalY(float world_x, float world_z)
{
  return TerrainDataModel::SampleNormalYWorld(
      world_x,
      world_z,
      kTerrainGrassSlopeSampleOffset);
}

MapShaderType getDefaultShaderTypeForKind(int kind_id)
{
  switch (kind_id)
  {
  case SLIME:
    return MapShaderType::Toon;
  case ROCK:
  case BLOCK:
  case GRASS:
  case SPHERE:
  case CYLINDER:
  case CAPSULE:
    return MapShaderType::Lit;
  case FIELD:
  default:
    return MapShaderType::Default;
  }
}

MaterialType resolveMaterialType(MapShaderType shader_type)
{
  switch (shader_type)
  {
  case MapShaderType::Unlit:
    return MaterialType::Unlit;
  case MapShaderType::Lit:
  case MapShaderType::Toon:
  case MapShaderType::Default:
  default:
    return MaterialType::Lit;
  }
}

int getGrassDemoCount()
{
  return SceneStressDebug_IsEnabled() ? kInstancedGrassCount * 8
                                      : kInstancedGrassCount;
}

int getBirdDemoCount()
{
  return SceneStressDebug_IsEnabled() ? kInstancedBirdCount * 8
                                      : kInstancedBirdCount;
}

bool isBackupMapPath(const char* filename)
{
  if (filename == nullptr)
  {
    return false;
  }

  std::string normalized_path(filename);
  std::transform(
      normalized_path.begin(),
      normalized_path.end(),
      normalized_path.begin(),
      [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

  return normalized_path.find("backup") != std::string::npos;
}

void drawCubeTiled(int texture_id, const XMMATRIX& world_matrix)
{
  struct TiledVertex
  {
    float x, y, z, nx, ny, nz, r, g, b, a, u, v;
  };

  static const TiledVertex kVertices[36] = {
      {-0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1, 1, 1, 1, 0, 0},
      {0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1, 1, 1, 1, 1, 1},
      {-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1, 1, 1, 1, 0, 1},
      {-0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1, 1, 1, 1, 0, 0},
      {0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1, 1, 1, 1, 1, 0},
      {0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1, 1, 1, 1, 1, 1},

      {-0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1, 1, 1, 1, 0, 0},
      {-0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1, 1, 1, 1, 1, 1},
      {-0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1, 1, 1, 1, 0, 1},
      {-0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1, 1, 1, 1, 0, 0},
      {-0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1, 1, 1, 1, 1, 0},
      {-0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1, 1, 1, 1, 1, 1},

      {-0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1, 1, 1, 1, 0, 0},
      {0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1, 1, 1, 1, 1, 1},
      {-0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1, 1, 1, 1, 0, 1},
      {-0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1, 1, 1, 1, 0, 0},
      {0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1, 1, 1, 1, 1, 0},
      {0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1, 1, 1, 1, 1, 1},

      {0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1, 1, 1, 1, 0, 0},
      {0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1, 1, 1, 1, 1, 1},
      {0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1, 1, 1, 1, 0, 1},
      {0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1, 1, 1, 1, 0, 0},
      {0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1, 1, 1, 1, 1, 0},
      {0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1, 1, 1, 1, 1, 1},

      {-0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1, 1, 1, 1, 0, 0},
      {0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1, 1, 1, 1, 1, 1},
      {-0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1, 1, 1, 1, 0, 1},
      {-0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1, 1, 1, 1, 0, 0},
      {0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1, 1, 1, 1, 1, 0},
      {0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1, 1, 1, 1, 1, 1},

      {0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1, 1, 1, 1, 0, 0},
      {-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1, 1, 1, 1, 1, 1},
      {0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1, 1, 1, 1, 0, 1},
      {0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1, 1, 1, 1, 0, 0},
      {-0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1, 1, 1, 1, 1, 0},
      {-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1, 1, 1, 1, 1, 1},
  };

  if (g_tiled_vertex_buffer == nullptr)
  {
    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
    buffer_desc.ByteWidth = sizeof(kVertices);
    buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA subresource_data{kVertices, 0, 0};
    Direct3D_GetDevice()->CreateBuffer(&buffer_desc, &subresource_data, &g_tiled_vertex_buffer);
    if (g_tiled_vertex_buffer == nullptr)
    {
      return;
    }
  }

  Shader3D_Begin();
  Shader3D_SetWorldMatrix(world_matrix);
  TextureManager::SetTexture(texture_id);

  UINT stride = sizeof(TiledVertex);
  UINT offset = 0;
  Direct3D_GetContext()->IASetVertexBuffers(0, 1, &g_tiled_vertex_buffer, &stride, &offset);
  Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  Direct3D_GetContext()->Draw(36, 0);
}

float frac(float value)
{
  return value - std::floor(value);
}

float hash21(float x, float y)
{
  float px = frac(x * 123.34f);
  float py = frac(y * 345.45f);
  const float dot_term = px * (px + 34.345f) + py * (py + 34.345f);
  px += dot_term;
  py += dot_term;
  return frac(px * py);
}

float valueNoise(float x, float y)
{
  const float cell_x = std::floor(x);
  const float cell_y = std::floor(y);
  const float local_x = frac(x);
  const float local_y = frac(y);
  const float smooth_x = local_x * local_x * (3.0f - 2.0f * local_x);
  const float smooth_y = local_y * local_y * (3.0f - 2.0f * local_y);

  const float v00 = hash21(cell_x + 0.0f, cell_y + 0.0f);
  const float v10 = hash21(cell_x + 1.0f, cell_y + 0.0f);
  const float v01 = hash21(cell_x + 0.0f, cell_y + 1.0f);
  const float v11 = hash21(cell_x + 1.0f, cell_y + 1.0f);

  const float lerp_x0 = std::lerp(v00, v10, smooth_x);
  const float lerp_x1 = std::lerp(v01, v11, smooth_x);
  return std::lerp(lerp_x0, lerp_x1, smooth_y);
}

float Smoothstep(float value)
{
  const float t = std::clamp(value, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float RemapClamped(float value, float in_min, float in_max)
{
  if (in_max <= in_min)
  {
    return 0.0f;
  }

  return std::clamp((value - in_min) / (in_max - in_min), 0.0f, 1.0f);
}

float fbm(float x, float y)
{
  float value = 0.0f;
  float amplitude = 0.5f;
  float domain_x = x;
  float domain_y = y;

  for (int octave = 0; octave < 4; ++octave)
  {
    value += valueNoise(domain_x, domain_y) * amplitude;
    domain_x = domain_x * 2.03f + 17.0f;
    domain_y = domain_y * 2.03f + 9.0f;
    amplitude *= 0.5f;
  }

  return value;
}

bool IsTerrainGrassHabitat(float world_x, float world_z, float world_y, float normal_y)
{
  return TerrainDataModel::IsGrassHabitatWorld(world_x, world_z, world_y, normal_y);
}

XMFLOAT2 safeNormalize2(float x, float y)
{
  const float length_sq = x * x + y * y;
  if (length_sq < 1.0e-6f)
  {
    return {1.0f, 0.0f};
  }
  const float inv_length = 1.0f / std::sqrt(length_sq);
  return {x * inv_length, y * inv_length};
}

bool SampleTextureSignature(
    ID3D11ShaderResourceView* srv,
    ID3D11Texture2D*& staging_texture,
    unsigned int channel_index,
    float& out_signature)
{
  out_signature = 0.0f;
  if (srv == nullptr)
  {
    return false;
  }

  ID3D11Resource* resource = nullptr;
  srv->GetResource(&resource);
  if (resource == nullptr)
  {
    return false;
  }

  ID3D11Texture2D* source_texture = nullptr;
  const HRESULT query_result = resource->QueryInterface(
      __uuidof(ID3D11Texture2D),
      reinterpret_cast<void**>(&source_texture));
  resource->Release();
  if (FAILED(query_result) || source_texture == nullptr)
  {
    return false;
  }

  D3D11_TEXTURE2D_DESC source_desc{};
  source_texture->GetDesc(&source_desc);

  bool needs_recreate = staging_texture == nullptr;
  if (!needs_recreate)
  {
    D3D11_TEXTURE2D_DESC staging_desc{};
    staging_texture->GetDesc(&staging_desc);
    needs_recreate =
        staging_desc.Width != source_desc.Width ||
        staging_desc.Height != source_desc.Height ||
        staging_desc.Format != source_desc.Format;
  }

  if (needs_recreate)
  {
    SafeReleaseMapResource(staging_texture);

    D3D11_TEXTURE2D_DESC staging_desc = source_desc;
    staging_desc.BindFlags = 0;
    staging_desc.MiscFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.MipLevels = 1;
    staging_desc.ArraySize = 1;
    if (FAILED(Direct3D_GetDevice()->CreateTexture2D(&staging_desc, nullptr, &staging_texture)))
    {
      source_texture->Release();
      return false;
    }
  }

  Direct3D_GetContext()->CopyResource(staging_texture, source_texture);
  source_texture->Release();

  D3D11_MAPPED_SUBRESOURCE mapped_resource{};
  if (FAILED(Direct3D_GetContext()->Map(staging_texture, 0, D3D11_MAP_READ, 0, &mapped_resource)))
  {
    return false;
  }

  static const int kSampleCoords[9][2] = {
      {1, 1}, {2, 1}, {3, 1},
      {1, 2}, {2, 2}, {3, 2},
      {1, 3}, {2, 3}, {3, 3}};

  float signature = 0.0f;
  for (const auto& sample : kSampleCoords)
  {
    const unsigned int x = static_cast<unsigned int>((source_desc.Width - 1) * sample[0] / 4);
    const unsigned int y = static_cast<unsigned int>((source_desc.Height - 1) * sample[1] / 4);
    const unsigned char* row_ptr =
        static_cast<const unsigned char*>(mapped_resource.pData) + mapped_resource.RowPitch * y;
    const float* pixel_ptr = reinterpret_cast<const float*>(row_ptr) + x * 4u;
    signature += pixel_ptr[channel_index];
  }

  Direct3D_GetContext()->Unmap(staging_texture, 0);
  out_signature = signature / 9.0f;
  return true;
}

XMFLOAT2 vortexContribution(float uv_x, float uv_y, float center_x, float center_y, float radius, float swirl_sign)
{
  const float delta_x = uv_x - center_x;
  const float delta_y = uv_y - center_y;
  const float distance = std::sqrt(delta_x * delta_x + delta_y * delta_y);
  float falloff = std::clamp(1.0f - distance / std::max(radius, 0.001f), 0.0f, 1.0f);
  falloff = falloff * falloff * (3.0f - 2.0f * falloff);
  const XMFLOAT2 tangent = safeNormalize2(-delta_y, delta_x);
  return {tangent.x * swirl_sign * falloff, tangent.y * swirl_sign * falloff};
}

XMFLOAT3 sampleWindFieldCpu(
    float uv_x,
    float uv_y,
    float time_seconds,
    const ComputeNoiseSettings& settings)
{
  float wind_x = settings.wind_direction_x;
  float wind_y = settings.wind_direction_y;
  const XMFLOAT2 base_wind = safeNormalize2(wind_x, wind_y);
  wind_x = base_wind.x;
  wind_y = base_wind.y;

  const float cross_x = -wind_y;
  const float cross_y = wind_x;
  const float noise_scale = std::max(settings.noise_scale, 1.0f);
  const float domain_x = uv_x * noise_scale;
  const float domain_y = uv_y * noise_scale;

  const float time_a = time_seconds * 0.010f;
  const float time_b = time_seconds * 0.007f;
  const float drift_x = wind_x * time_seconds * 0.007f + cross_x * std::sin(time_seconds * 0.005f) * 0.035f;
  const float drift_y = wind_y * time_seconds * 0.007f + cross_y * std::sin(time_seconds * 0.005f) * 0.035f;
  const float vortex_a_center_x = 0.24f + std::sin(time_a) * 0.025f;
  const float vortex_a_center_y = 0.32f + std::cos(time_a * 0.7f) * 0.025f;
  const float vortex_b_center_x = 0.76f + std::cos(time_b * 0.8f) * 0.032f;
  const float vortex_b_center_y = 0.64f + std::sin(time_b) * 0.032f;

  const XMFLOAT2 vortex_a = vortexContribution(uv_x, uv_y, vortex_a_center_x, vortex_a_center_y, 0.30f, +1.0f);
  const XMFLOAT2 vortex_b = vortexContribution(uv_x, uv_y, vortex_b_center_x, vortex_b_center_y, 0.34f, -1.0f);
  const float vortex_flow_x = vortex_a.x + vortex_b.x;
  const float vortex_flow_y = vortex_a.y + vortex_b.y;

  const float jet_band_north = std::exp(-std::pow((uv_y - 0.24f) / 0.12f, 2.0f));
  const float jet_band_mid = std::exp(-std::pow((uv_y - 0.52f) / 0.18f, 2.0f));
  const float jet_band_south = std::exp(-std::pow((uv_y - 0.78f) / 0.14f, 2.0f));
  const float jet_flow_x = wind_x * (0.62f + jet_band_north * 0.95f + jet_band_mid * 0.28f - jet_band_south * 0.22f)
                           + cross_x * ((jet_band_north - jet_band_south) * 0.34f + (jet_band_mid - 0.35f) * 0.18f);
  const float jet_flow_y = wind_y * (0.62f + jet_band_north * 0.95f + jet_band_mid * 0.28f - jet_band_south * 0.22f)
                           + cross_y * ((jet_band_north - jet_band_south) * 0.34f + (jet_band_mid - 0.35f) * 0.18f);

  const float warp_x = fbm(domain_x * 0.14f + drift_x * 14.0f + 7.1f, domain_y * 0.14f + drift_y * 14.0f + 13.4f) - 0.5f;
  const float warp_y = fbm(domain_x * 0.14f - drift_x * 12.0f - 4.8f, domain_y * 0.14f - drift_y * 12.0f + 3.2f) - 0.5f;
  const float flow_uv_x = uv_x + drift_x + warp_x * 0.11f;
  const float flow_uv_y = uv_y + drift_y + warp_y * 0.11f;

  const float eps = 0.012f;
  const float potential_x1 = fbm((flow_uv_x + eps) * 2.2f + 11.0f, flow_uv_y * 2.2f + 5.0f);
  const float potential_x0 = fbm((flow_uv_x - eps) * 2.2f + 11.0f, flow_uv_y * 2.2f + 5.0f);
  const float potential_y1 = fbm(flow_uv_x * 2.2f + 11.0f, (flow_uv_y + eps) * 2.2f + 5.0f);
  const float potential_y0 = fbm(flow_uv_x * 2.2f + 11.0f, (flow_uv_y - eps) * 2.2f + 5.0f);
  const float dphi_dx = (potential_x1 - potential_x0) / (eps * 2.0f);
  const float dphi_dy = (potential_y1 - potential_y0) / (eps * 2.0f);
  const float curl_flow_x = dphi_dy;
  const float curl_flow_y = -dphi_dx;

  const float micro_angle = (fbm(flow_uv_x * 3.2f + 17.2f, flow_uv_y * 3.2f + 4.8f) - 0.5f) * 0.22f * settings.wind_cross_influence;
  const float sin_a = std::sin(micro_angle);
  const float cos_a = std::cos(micro_angle);
  const float micro_flow_x = (wind_x * cos_a - wind_y * sin_a) * 0.10f;
  const float micro_flow_y = (wind_x * sin_a + wind_y * cos_a) * 0.10f;

  const float combined_flow_x = jet_flow_x + vortex_flow_x * 0.92f + curl_flow_x * 0.34f + micro_flow_x;
  const float combined_flow_y = jet_flow_y + vortex_flow_y * 0.92f + curl_flow_y * 0.34f + micro_flow_y;
  const XMFLOAT2 local_dir = safeNormalize2(combined_flow_x, combined_flow_y);

  const float strength_noise = fbm(flow_uv_x * 1.7f + 9.6f, flow_uv_y * 1.7f - 6.4f);
  const float vortex_strength = std::clamp(
      std::sqrt(vortex_flow_x * vortex_flow_x + vortex_flow_y * vortex_flow_y) * 0.50f
          + std::max(std::max(jet_band_north, jet_band_mid), jet_band_south) * 0.28f,
      0.0f,
      1.0f);
  const float strength = std::clamp(
      0.08f + settings.wind_strength * 1.45f + vortex_strength * 0.24f
          + std::sqrt(curl_flow_x * curl_flow_x + curl_flow_y * curl_flow_y) * 0.08f
          + (strength_noise - 0.5f) * 0.06f,
      0.0f,
      1.0f);
  return {local_dir.x, local_dir.y, strength};
}

bool worldToClimateUv(float world_x, float world_z, float& out_uv_x, float& out_uv_y)
{
  out_uv_x = (world_x - kClimateWorldMinX) / (kClimateWorldMaxX - kClimateWorldMinX);
  out_uv_y = (world_z - kClimateWorldMinZ) / (kClimateWorldMaxZ - kClimateWorldMinZ);
  return out_uv_x >= 0.0f && out_uv_x <= 1.0f && out_uv_y >= 0.0f && out_uv_y <= 1.0f;
}

void drawClimatePanelArrow(
    int white_tex_id,
    const XMMATRIX& panel_basis,
    float local_x,
    float local_y,
    float angle,
    float strength)
{
  const float shaft_length = 0.16f + strength * 0.34f;
  const float shaft_thickness = 0.022f;
  const float head_length = 0.08f + strength * 0.12f;
  const float head_thickness = 0.018f;
  const XMFLOAT4 shaft_color = {1.0f, 1.0f, 1.0f, 0.82f};
  const XMFLOAT4 head_color = {1.0f, 1.0f, 1.0f, 0.92f};

  const XMMATRIX shaft_world =
      XMMatrixScaling(shaft_length, shaft_thickness, 0.02f) *
      XMMatrixRotationZ(angle) *
      XMMatrixTranslation(local_x, local_y, 0.52f) *
      panel_basis;
  Cube_DrawMaterial(
      white_tex_id,
      shaft_world,
      shaft_color,
      RenderState{DepthMode::ReadWrite, BlendMode::Alpha, CullMode::None},
      MaterialType::Unlit);

  const float tip_offset_x = std::cos(angle) * shaft_length * 0.35f;
  const float tip_offset_y = std::sin(angle) * shaft_length * 0.35f;

  const XMMATRIX head_a_world =
      XMMatrixScaling(head_length, head_thickness, 0.02f) *
      XMMatrixRotationZ(angle + 0.55f) *
      XMMatrixTranslation(local_x + tip_offset_x, local_y + tip_offset_y, 0.53f) *
      panel_basis;
  Cube_DrawMaterial(
      white_tex_id,
      head_a_world,
      head_color,
      RenderState{DepthMode::ReadWrite, BlendMode::Alpha, CullMode::None},
      MaterialType::Unlit);

  const XMMATRIX head_b_world =
      XMMatrixScaling(head_length, head_thickness, 0.02f) *
      XMMatrixRotationZ(angle - 0.55f) *
      XMMatrixTranslation(local_x + tip_offset_x, local_y + tip_offset_y, 0.53f) *
      panel_basis;
  Cube_DrawMaterial(
      white_tex_id,
      head_b_world,
      head_color,
      RenderState{DepthMode::ReadWrite, BlendMode::Alpha, CullMode::None},
      MaterialType::Unlit);
}

void drawClimatePanelMarker(
    int white_tex_id,
    const XMMATRIX& panel_basis,
    float local_x,
    float local_y)
{
  const float radius = 0.26f;
  const float thickness = 0.022f;
  const XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 0.98f};

  for (int segment = 0; segment < 12; ++segment)
  {
    const float angle = (static_cast<float>(segment) / 12.0f) * XM_2PI;
    const float px = local_x + std::cos(angle) * radius;
    const float py = local_y + std::sin(angle) * radius;
    const XMMATRIX segment_world =
        XMMatrixScaling(0.11f, thickness, 0.02f) *
        XMMatrixRotationZ(angle + XM_PIDIV2) *
        XMMatrixTranslation(px, py, 0.54f) *
        panel_basis;
    Cube_DrawMaterial(
        white_tex_id,
        segment_world,
        color,
        RenderState{DepthMode::ReadWrite, BlendMode::Alpha, CullMode::None},
        MaterialType::Unlit);
  }
}

void removeGrassObjects(std::vector<MapObject>& objects)
{
  objects.erase(
      std::remove_if(
          objects.begin(),
          objects.end(),
          [](const MapObject& object) { return object.KindId == GRASS; }),
      objects.end());
}

} // namespace

float MapController::ResolveGroundedY(const MapObject& object) const
{
  if (object.KindId == FIELD)
  {
    return object.Position.y;
  }

  float offset_from_origin_to_bottom = 0.0f;
  switch (object.KindId)
  {
  case BLOCK:
  case SPHERE:
  case CYLINDER:
  case CAPSULE:
    offset_from_origin_to_bottom = object.Scale.y * 0.5f;
    break;
  case GRASS:
    // Grass is currently a world-space quad centered on its local origin,
    // so it needs to be lifted by half its height to sit on the terrain.
    offset_from_origin_to_bottom = object.Scale.y * 0.5f;
    break;
  case SLIME:
    if (m_slime01 != nullptr)
    {
      offset_from_origin_to_bottom = -m_slime01->localAABB.min.y * object.Scale.y;
    }
    break;
  case ROCK:
    if (m_rock != nullptr)
    {
      offset_from_origin_to_bottom = -m_rock->localAABB.min.y * object.Scale.y;
    }
    break;
  default:
    return object.Position.y;
  }

  if (offset_from_origin_to_bottom <= 0.0f)
  {
    offset_from_origin_to_bottom = std::max(0.25f, object.Scale.y * 0.5f);
  }

  const float terrain_height = SampleTerrainHeightWorld(object.Position.x, object.Position.z);
  return std::max(object.Position.y, terrain_height + offset_from_origin_to_bottom);
}

int MapController::ResolveMapTexture(MapObject& obj)
{
  if (obj.TexturePath.empty())
  {
    return -1;
  }

  if (obj.TextureId < 0)
  {
    std::wstring texture_path(obj.TexturePath.begin(), obj.TexturePath.end());
    obj.TextureId = ResourceManager::GetTexture(texture_path.c_str());
  }

  return obj.TextureId;
}

XMMATRIX MapController::BuildObjectWorldMatrix(const MapObject& object) const
{
  return XMMatrixScaling(object.Scale.x, object.Scale.y, object.Scale.z) *
         XMMatrixRotationRollPitchYaw(
             object.Rotation.x,
             object.Rotation.y,
             object.Rotation.z) *
         XMMatrixTranslation(
             object.Position.x,
             ResolveGroundedY(object),
             object.Position.z);
}

void MapController::DrawObjectShadow(const MapObject& object) const
{
  switch (object.KindId)
  {
  case FIELD:
    break;
  case BLOCK:
    Cube_DrawShadow(BuildObjectWorldMatrix(object));
    break;
  case GRASS:
    if (DebugMenu_IsGrassGpuEnabled())
    {
      GrassPatch_DrawShadow(BuildObjectWorldMatrix(object));
    }
    break;
  case SPHERE:
    Sphere_DrawShadow(BuildObjectWorldMatrix(object));
    break;
  case CYLINDER:
    Cylinder_DrawShadow(BuildObjectWorldMatrix(object));
    break;
  case CAPSULE:
    Capsule_DrawShadow(BuildObjectWorldMatrix(object));
    break;
  case SLIME:
    ModelDrawShadow(
        m_slime01,
        XMMatrixScaling(object.Scale.x, object.Scale.y, object.Scale.z) *
            XMMatrixTranslation(
                object.Position.x,
                ResolveGroundedY(object),
                object.Position.z));
    break;
  case ROCK:
    if (m_rock != nullptr)
    {
      ModelDrawShadow(
          m_rock,
          XMMatrixScaling(object.Scale.x, object.Scale.y, object.Scale.z) *
              XMMatrixTranslation(
                  object.Position.x,
                  ResolveGroundedY(object),
                  object.Position.z));
    }
    break;
  }
}

bool MapController::ShouldCullAabb(const ViewFrustum& view_frustum,
                                   const Collision::AABB& aabb) const
{
  if (!FrustumCullingDebug_IsEnabled())
  {
    return false;
  }

  if (!view_frustum.intersects(aabb))
  {
    FrustumCullingDebug_RecordCulled();
    return true;
  }

  FrustumCullingDebug_RecordVisible();
  return false;
}

void MapController::AppendInstanceMatrix(std::vector<XMFLOAT4X4>& instances,
                                         const XMMATRIX& world_matrix) const
{
  XMFLOAT4X4 stored_matrix{};
  XMStoreFloat4x4(&stored_matrix, world_matrix);
  instances.push_back(stored_matrix);
}

std::vector<MapController::DemoInstanceData> MapController::BuildGrassDemoData(bool stress_mode) const
{
  std::vector<DemoInstanceData> entries;
  const int grass_count = stress_mode ? kInstancedGrassCount * 8 : kInstancedGrassCount;
  const int grid_width = stress_mode ? 32 : 16;
  entries.reserve(grass_count);

  for (int index = 0; index < grass_count; ++index)
  {
    const int grid_x = index % grid_width;
    const int grid_z = index / grid_width;
    const float offset_x = static_cast<float>(grid_x) * 1.15f -
                           static_cast<float>(grid_width - 1) * 0.575f;
    const float offset_z = static_cast<float>(grid_z) * 1.15f - 11.5f;
    const float sway = std::sin(static_cast<float>(index) * 1.37f) * 0.18f;
    const float yaw = static_cast<float>(index) * 0.31f;
    const float slime_scale = 0.42f + sway * 0.08f;
    const float half_extent = slime_scale * 0.35f;
    const float height = slime_scale * 0.7f;
    const float terrain_height = SampleTerrainHeightWorld(offset_x, offset_z);
    const float grounded_y = terrain_height + height * 0.5f;

    DemoInstanceData entry{};
    entry.bounds = {{offset_x - half_extent, terrain_height, offset_z - half_extent},
                    {offset_x + half_extent, terrain_height + height, offset_z + half_extent}};
    XMStoreFloat4x4(&entry.world_matrix,
                    XMMatrixScaling(slime_scale, slime_scale, slime_scale) *
                        XMMatrixRotationRollPitchYaw(0.0f, yaw, 0.0f) *
                        XMMatrixTranslation(offset_x, grounded_y, offset_z));
    entries.push_back(entry);
  }

  return entries;
}

Octree MapController::BuildGrassDemoOctree(const std::vector<DemoInstanceData>& entries) const
{
  std::vector<OctreeItem> octree_items;
  octree_items.reserve(entries.size());

  for (int index = 0; index < static_cast<int>(entries.size()); ++index)
  {
    octree_items.push_back({entries[index].bounds, index});
  }

  Octree octree{};
  octree.build(octree_items, 3, 24);
  return octree;
}

void MapController::EnsureGrassDemoCache()
{
  if (m_grass_demo_cache_built)
  {
    return;
  }

  m_grass_demo_normal = BuildGrassDemoData(false);
  m_grass_demo_stress = BuildGrassDemoData(true);
  m_grass_demo_normal_octree = BuildGrassDemoOctree(m_grass_demo_normal);
  m_grass_demo_stress_octree = BuildGrassDemoOctree(m_grass_demo_stress);
  m_grass_demo_cache_built = true;
}

void MapController::DrawInstancedGrassDemo(bool use_instancing,
                                           const ViewFrustum& view_frustum,
                                           std::vector<XMFLOAT4X4>& batch)
{
  EnsureGrassDemoCache();

  const auto& grass_entries = SceneStressDebug_IsEnabled() ? m_grass_demo_stress : m_grass_demo_normal;
  const auto& grass_octree = SceneStressDebug_IsEnabled() ? m_grass_demo_stress_octree
                                                          : m_grass_demo_normal_octree;

  std::vector<int> visible_indices;
  if (FrustumCullingDebug_IsEnabled())
  {
    grass_octree.query(view_frustum, visible_indices);
    FrustumCullingDebug_RecordVisibleCount(static_cast<int>(visible_indices.size()));
    FrustumCullingDebug_RecordCulledCount(
        static_cast<int>(grass_entries.size() - visible_indices.size()));
  }
  else
  {
    visible_indices.resize(grass_entries.size());
    for (int index = 0; index < static_cast<int>(grass_entries.size()); ++index)
    {
      visible_indices[index] = index;
    }
  }

  for (int visible_index : visible_indices)
  {
    const auto& entry = grass_entries[visible_index];
    const XMMATRIX world_matrix = XMLoadFloat4x4(&entry.world_matrix);

    if (use_instancing)
    {
      batch.push_back(entry.world_matrix);
    }
    else
    {
      ModelDraw(m_slime01, world_matrix);
    }
  }
}

void MapController::DrawInstancedBirdDemo(bool use_instancing,
                                          const ViewFrustum& view_frustum,
                                          std::vector<XMFLOAT4X4>& batch) const
{
  const float time = static_cast<float>(SystemTimer_GetTime());
  const int bird_count = getBirdDemoCount();

  for (int index = 0; index < bird_count; ++index)
  {
    const float phase =
        (static_cast<float>(index) / static_cast<float>(bird_count)) * XM_2PI;
    const float ring_offset =
        SceneStressDebug_IsEnabled() ? static_cast<float>(index % 4) * 2.75f : 0.0f;
    const float radius = 10.0f + ring_offset + std::sin(phase * 3.0f) * 2.2f;
    const float angle = time * (SceneStressDebug_IsEnabled() ? 0.4f : 0.55f) + phase;
    const float pos_x = std::cos(angle) * radius;
    const float pos_z = std::sin(angle) * radius;
    const float pos_y = 8.5f + std::sin(time * 1.8f + phase * 2.0f) * 0.65f;
    const Collision::AABB bird_aabb{{pos_x - 0.175f, pos_y - 0.06f, pos_z - 0.3f},
                                    {pos_x + 0.175f, pos_y + 0.06f, pos_z + 0.3f}};

    if (ShouldCullAabb(view_frustum, bird_aabb))
    {
      continue;
    }

    const XMMATRIX world_matrix =
        XMMatrixScaling(0.35f, 0.12f, 0.6f) *
        XMMatrixRotationRollPitchYaw(0.0f, -angle,
                                     0.15f * std::sin(time * 2.2f + phase)) *
        XMMatrixTranslation(pos_x, pos_y, pos_z);

    if (use_instancing)
    {
      AppendInstanceMatrix(batch, world_matrix);
    }
    else
    {
      Cube_DrawMaterial(m_white_tex_id,
                        world_matrix,
                        {0.10f, 0.12f, 0.16f, 1.0f},
                        RenderState{DepthMode::ReadWrite, BlendMode::Opaque, CullMode::Back});
    }
  }
}

void MapController::Initialize()
{
  m_cube_tex_id = ResourceManager::GetTexture(L"resource/texture/stone_floor.png");
  m_debug_billboard_tex_id = ResourceManager::GetTexture(L"resource/texture/grass_bill.png");
  m_cube_grass_tex_id = ResourceManager::GetTexture(L"resource/texture/grass_bill.png");
  m_floating_light_tex_id = ResourceManager::GetTexture(L"resource/texture/glow.png");
  m_height_map_tex_id = TextureManager::Load(L"resource/texture/height_map.png");
  m_white_tex_id = ResourceManager::GetTexture(L"resource/texture/white.png");
  m_grass_model = nullptr;
  m_slime01 = ResourceManager::GetModel("resource/model/slime.fbx", 0.5f);

  FILE* file = nullptr;
  fopen_s(&file, "resource/map_save.txt", "r");
  if (file != nullptr)
  {
    fclose(file);
    LoadFromFile("resource/map_save.txt");
  }
  else
  {
    m_map_objects.clear();

    MapObject field_object{};
    field_object.KindId = FIELD;
    field_object.Position = {0.0f, 0.0f, 0.0f};
    field_object.Collision = {{-15.0f, 0.0f, -15.0f}, {15.0f, 1.0f, 15.0f}};
    field_object.Rotation = {0.0f, 0.0f, 0.0f};
    field_object.Scale = {1.0f, 1.0f, 1.0f};
    field_object.ShaderType = MapShaderType::Default;
    field_object.TextureId = -1;
    m_map_objects.push_back(field_object);

  }

  for (MapObject& object : m_map_objects)
  {
    if (object.KindId == SLIME || object.KindId == ROCK)
    {
      object.Collision = Model_GetAABB(m_slime01, object.Position);
    }
  }

  m_undo_stack.clear();
  m_redo_stack.clear();
  removeGrassObjects(m_map_objects);
  m_grass_demo_cache_built = false;
  m_terrain_grass_compute_seeded = false;
  m_terrain_grass_seed_terrain_settings = MeshFieldRenderer::GetTerrainSettings();
  m_terrain_grass_seed_height_srv = nullptr;
  m_terrain_grass_seed_grass_data_srv = nullptr;
  m_terrain_grass_last_probe_time_seconds = -1000.0;
  m_terrain_grass_last_grass_data_signature = -1.0f;
  SafeReleaseMapResource(m_terrain_grass_grass_data_probe_texture);
  if (!m_terrain_grass_instances.Initialize(Direct3D_GetDevice(), Direct3D_GetContext()))
  {
    hal::dout << "MapController::Initialize(): compute grass instances disabled" << std::endl;
  }
}

void MapController::Finalize()
{
  m_terrain_grass_instances.Finalize();
  m_terrain_grass_compute_seeded = false;
  m_terrain_grass_seed_height_srv = nullptr;
  m_terrain_grass_seed_grass_data_srv = nullptr;
  m_terrain_grass_last_probe_time_seconds = -1000.0;
  m_terrain_grass_last_grass_data_signature = -1.0f;
  SafeReleaseMapResource(m_terrain_grass_grass_data_probe_texture);
}

void MapController::EnsureTerrainGrassComputeSeeds(const RenderFrameContext& frame_context)
{
  const TerrainSettings& terrain_settings = MeshFieldRenderer::GetTerrainSettings();
  const TerrainWaterFrameState& terrain_water = frame_context.resources.terrain_water;
  if (m_terrain_grass_seed_terrain_settings != terrain_settings)
  {
    m_terrain_grass_compute_seeded = false;
    m_terrain_grass_seed_terrain_settings = terrain_settings;
  }
  ID3D11ShaderResourceView* terrain_height_srv =
      terrain_water.terrain_height.isValid()
          ? terrain_water.terrain_height.shaderResourceView()
          : TerrainDataModel::HeightSRV();
  if (m_terrain_grass_seed_height_srv != terrain_height_srv)
  {
    m_terrain_grass_compute_seeded = false;
    m_terrain_grass_seed_height_srv = terrain_height_srv;
  }
  ID3D11ShaderResourceView* grass_data_srv =
      frame_context.resources.grass_data.isValid()
          ? frame_context.resources.grass_data.shaderResourceView()
          : nullptr;
  if (m_terrain_grass_seed_grass_data_srv != grass_data_srv)
  {
    m_terrain_grass_compute_seeded = false;
    m_terrain_grass_seed_grass_data_srv = grass_data_srv;
    m_terrain_grass_last_grass_data_signature = -1.0f;
    m_terrain_grass_last_probe_time_seconds = -1000.0;
  }

  const float world_min_bound_x = -kTerrainWorldHalfWidth + kTerrainGrassMargin;
  const float world_max_bound_x = kTerrainWorldHalfWidth - kTerrainGrassMargin;
  const float world_min_bound_z = -kTerrainWorldHalfDepth + kTerrainGrassMargin;
  const float world_max_bound_z = kTerrainWorldHalfDepth - kTerrainGrassMargin;

  if (m_terrain_grass_compute_seeded || !m_terrain_grass_instances.IsValid())
  {
    return;
  }

  const float min_x = std::clamp(
      kTerrainGrassAreaCenterX - kTerrainGrassAreaHalfExtentX,
      world_min_bound_x,
      world_max_bound_x);
  const float max_x = std::clamp(
      kTerrainGrassAreaCenterX + kTerrainGrassAreaHalfExtentX,
      world_min_bound_x,
      world_max_bound_x);
  const float min_z = std::clamp(
      kTerrainGrassAreaCenterZ - kTerrainGrassAreaHalfExtentZ,
      world_min_bound_z,
      world_max_bound_z);
  const float max_z = std::clamp(
      kTerrainGrassAreaCenterZ + kTerrainGrassAreaHalfExtentZ,
      world_min_bound_z,
      world_max_bound_z);
  const unsigned int cols = static_cast<unsigned int>((max_x - min_x) / kTerrainGrassSpacing) + 1u;
  const unsigned int rows = static_cast<unsigned int>((max_z - min_z) / kTerrainGrassSpacing) + 1u;

  m_terrain_grass_compute_seeded = m_terrain_grass_instances.ConfigureCoverage(
      terrain_height_srv,
      grass_data_srv,
      cols,
      rows,
      min_x,
      min_z,
      kTerrainGrassSpacing,
      terrain_settings);
}

bool MapController::ShouldRefreshTerrainGrassCoverage(const RenderFrameContext& frame_context)
{
  if (!m_terrain_grass_instances.HasSeeds())
  {
    return false;
  }

  if (frame_context.globals.time_seconds - m_terrain_grass_last_probe_time_seconds <
      kGrassCoverageProbeIntervalSeconds)
  {
    return false;
  }

  m_terrain_grass_last_probe_time_seconds = frame_context.globals.time_seconds;

  float grass_data_signature = 0.0f;
  const bool sampled_grass_data = SampleTextureSignature(
      frame_context.resources.grass_data.isValid()
          ? frame_context.resources.grass_data.shaderResourceView()
          : nullptr,
      m_terrain_grass_grass_data_probe_texture,
      0u,
      grass_data_signature);

  if (!sampled_grass_data)
  {
    return false;
  }

  const bool first_probe = m_terrain_grass_last_grass_data_signature < 0.0f;
  const bool grass_data_changed =
      std::fabs(
          grass_data_signature - m_terrain_grass_last_grass_data_signature) >=
      kGrassCoverageClassificationSignatureThreshold;

  m_terrain_grass_last_grass_data_signature = grass_data_signature;
  return first_probe || grass_data_changed;
}

void MapController::Draw(const RenderFrameContext& frame_context)
{
    const XMMATRIX view = XMLoadFloat4x4(&frame_context.globals.view_matrix);
    const XMMATRIX proj = XMLoadFloat4x4(&frame_context.globals.projection_matrix);
    XMMATRIX world_matrix{};
    std::unordered_map<int, std::vector<XMFLOAT4X4>> cube_batches;
    std::vector<XMFLOAT4X4> grass_patch_batch;
    std::vector<XMFLOAT4X4> grass_demo_batch;
    std::vector<XMFLOAT4X4> bird_demo_batch;

  XMFLOAT4X4 view_matrix{};
  XMFLOAT4X4 projection_matrix{};
  XMStoreFloat4x4(&view_matrix, view);
  XMStoreFloat4x4(&projection_matrix, proj);

  ViewFrustum view_frustum{};
  view_frustum.build(view_matrix, projection_matrix);
  const TerrainWaterFrameState& terrain_water = frame_context.resources.terrain_water;

  ShaderField_SetLightViewProj(Light_GetLightViewProjectionMatrix());
  ShaderField_SetViewMatrix(view);
  ShaderField_SetProjectionMatrix(proj);
  ShaderField_SetShadowMap(frame_context.resources.shadow_map);
  ShaderField_SetClimateMap(frame_context.resources.climate_field.shaderResourceView());
  ShaderField_SetTerrainSurfacePresentationEnabled(DebugMenu_IsTerrainSurfacePresentationEnabled());
  ShaderField_SetTerrainSurfaceDataMap(
      terrain_water.terrain_surface_data.isValid()
          ? terrain_water.terrain_surface_data.shaderResourceView()
          : nullptr);

  Shader3D_SetViewMatrix(view);
  Shader3D_SetProjMatrix(proj);
  Shader3D_Unlit_SetViewMatrix(view);
  Shader3D_Unlit_SetProjMatrix(proj);
  Shader3DInstanced_SetViewMatrix(view);
  Shader3DInstanced_SetProjMatrix(proj);
  ShaderGrassInstanced_SetViewMatrix(view);
  ShaderGrassInstanced_SetProjMatrix(proj);
  ShaderSprite3D_Cutout_SetViewMatrix(view);
  ShaderSprite3D_Cutout_SetProjMatrix(proj);
  ShaderSprite3D_CutoutInstanced_SetViewMatrix(view);
  ShaderSprite3D_CutoutInstanced_SetProjMatrix(proj);
  {
    const auto& noise_settings = DebugMenu_GetComputeNoiseSettings();
    const float wrapped_time_seconds =
        std::fmod(static_cast<float>(frame_context.globals.time_seconds), 1024.0f);
    ShaderSprite3D_Cutout_SetWindSettings(
        wrapped_time_seconds,
        {noise_settings.wind_direction_x, noise_settings.wind_direction_y},
        noise_settings.wind_strength,
        kClimateWorldMinX,
        kClimateWorldMaxX,
        kClimateWorldMinZ,
        kClimateWorldMaxZ);
    ShaderSprite3D_Cutout_SetWindField(frame_context.resources.wind_field.shaderResourceView());
    ShaderSprite3D_CutoutInstanced_SetWindSettings(
        wrapped_time_seconds,
        {noise_settings.wind_direction_x, noise_settings.wind_direction_y},
        noise_settings.wind_strength,
        kClimateWorldMinX,
        kClimateWorldMaxX,
        kClimateWorldMinZ,
        kClimateWorldMaxZ);
    ShaderSprite3D_CutoutInstanced_SetWindField(frame_context.resources.wind_field.shaderResourceView());
  }
  BillBoard_SetViewMatrix(frame_context.globals.view_matrix);

  for (MapObject& object : m_map_objects)
  {
    const int resolved_texture = ResolveMapTexture(object);

    Direct3D_SetDepthEnable(true);

    switch (object.KindId)
    {
    case FIELD:
      if (m_terrain_visible)
      {
        const XMMATRIX scale_matrix = XMMatrixScaling(5.0f, 5.0f, 5.0f);
        ShaderField_SetWorldMatrix(scale_matrix);
        MeshFieldRenderer::Draw(
          frame_context.resources.terrain_height.shaderResourceView(),
          frame_context.resources.terrain_normal.shaderResourceView());
      }
      break;
    case BLOCK:
      if (ShouldCullAabb(view_frustum, object.Collision))
      {
        break;
      }
      world_matrix = BuildObjectWorldMatrix(object);
      if (InstancingDebug_IsEnabled() &&
          (object.ShaderType == MapShaderType::Default || object.ShaderType == MapShaderType::Lit) &&
          (resolved_texture == m_cube_tex_id || resolved_texture < 0))
      {
        XMFLOAT4X4 stored_world{};
        XMStoreFloat4x4(&stored_world, world_matrix);
        cube_batches[resolved_texture >= 0 ? resolved_texture : m_cube_tex_id].push_back(stored_world);
      }
      else
      {
        const bool use_tiled_lit_draw =
            resolved_texture >= 0 &&
            (object.ShaderType == MapShaderType::Default || object.ShaderType == MapShaderType::Lit);

        if (use_tiled_lit_draw)
        {
          drawCubeTiled(resolved_texture, world_matrix);
        }
        else
        {
          Cube_DrawMaterial(resolved_texture >= 0 ? resolved_texture : m_cube_tex_id,
                            world_matrix,
                            {1.0f, 1.0f, 1.0f, 1.0f},
                            RenderState{DepthMode::ReadWrite},
                            resolveMaterialType(object.ShaderType));
        }
      }
      break;
    case GRASS:
      {
        if (!DebugMenu_IsGrassGpuEnabled())
        {
          break;
        }
        world_matrix = BuildObjectWorldMatrix(object);
        AppendInstanceMatrix(grass_patch_batch, world_matrix);
      }
      break;
    case SPHERE:
      if (ShouldCullAabb(view_frustum, object.Collision))
      {
        break;
      }
      world_matrix = BuildObjectWorldMatrix(object);
      Sphere_Draw(resolved_texture >= 0 ? resolved_texture : m_white_tex_id,
                  world_matrix,
                  resolveMaterialType(object.ShaderType));
      break;
    case CYLINDER:
      world_matrix = BuildObjectWorldMatrix(object);
      Cylinder_Draw(resolved_texture >= 0 ? resolved_texture : m_white_tex_id,
                    world_matrix,
                    resolveMaterialType(object.ShaderType));
      break;
    case CAPSULE:
      world_matrix = BuildObjectWorldMatrix(object);
      Capsule_Draw(resolved_texture >= 0 ? resolved_texture : m_white_tex_id,
                   world_matrix,
                   resolveMaterialType(object.ShaderType));
      break;
    case SLIME:
      world_matrix =
          XMMatrixScaling(object.Scale.x, object.Scale.y, object.Scale.z) *
          XMMatrixTranslation(object.Position.x, ResolveGroundedY(object), object.Position.z);
      switch (object.ShaderType)
      {
      case MapShaderType::Unlit:
        ModelUnlitDraw(m_slime01, world_matrix);
        break;
      case MapShaderType::Lit:
        ModelDraw(m_slime01, world_matrix);
        break;
      case MapShaderType::Toon:
      case MapShaderType::Default:
      default:
        ModelToonDraw(m_slime01, world_matrix, view, proj);
        break;
      }
      break;
    case ROCK:
      if (m_rock != nullptr)
      {
        world_matrix =
            XMMatrixScaling(object.Scale.x, object.Scale.y, object.Scale.z) *
            XMMatrixTranslation(object.Position.x, ResolveGroundedY(object), object.Position.z);
        switch (object.ShaderType)
        {
        case MapShaderType::Unlit:
          ModelUnlitDraw(m_rock, world_matrix);
          break;
        case MapShaderType::Toon:
          ModelToonDraw(m_rock, world_matrix, view, proj);
          break;
        case MapShaderType::Lit:
        case MapShaderType::Default:
        default:
          ModelDraw(m_rock, world_matrix);
          break;
        }
      }
      break;
    }
  }

  // Stress-test demo objects are disabled for grass/render profiling.
  // DrawInstancedGrassDemo(InstancingDebug_IsEnabled(), view_frustum, grass_demo_batch);
  // DrawInstancedBirdDemo(InstancingDebug_IsEnabled(), view_frustum, bird_demo_batch);

  for (auto& batch : cube_batches)
  {
    InstancingDebug_AddBatch(static_cast<int>(batch.second.size()));
    Cube_DrawInstanced(batch.first,
                       batch.second,
                       {0.7f, 0.7f, 0.7f, 1.0f},
                       RenderState{DepthMode::ReadWrite, BlendMode::Opaque, CullMode::Back});
  }

  if (kDrawGrass && DebugMenu_IsGrassGpuEnabled() && !grass_patch_batch.empty())
  {
    InstancingDebug_AddBatch(static_cast<int>(grass_patch_batch.size()));
    GrassPatch_DrawInstanced(
        m_cube_grass_tex_id,
        grass_patch_batch,
        {1.0f, 1.0f, 1.0f, 1.0f},
        nullptr,
        0.0f,
        0.0f,
        0.0f,
        RenderState{DepthMode::ReadWrite, BlendMode::Opaque, CullMode::None});
  }

  if (!grass_demo_batch.empty())
  {
    InstancingDebug_AddBatch(static_cast<int>(grass_demo_batch.size()));
    ModelDrawInstanced(m_slime01, grass_demo_batch);
  }

  if (!bird_demo_batch.empty())
  {
    InstancingDebug_AddBatch(static_cast<int>(bird_demo_batch.size()));
    Cube_DrawInstanced(m_white_tex_id,
                       bird_demo_batch,
                       {0.10f, 0.12f, 0.16f, 1.0f},
                       RenderState{DepthMode::ReadWrite, BlendMode::Opaque, CullMode::Back});
  }

  {
    GrassComputeStats grass_compute_stats{};
    if (frame_context.resources.compute_shared_registry != nullptr)
    {
      frame_context.resources.compute_shared_registry->Reset(ComputeSharedResourceId::GrassInstances);
      frame_context.resources.compute_shared_registry->Reset(ComputeSharedResourceId::GrassIndirectArgs);
    }

    if (!kDrawGrass)
    {
      grass_compute_stats.last_error = "Disabled for profiling";
    }
    else if (m_terrain_visible && kDrawTerrainGrassCompute)
    {
      if (!DebugMenu_IsGrassGpuEnabled())
      {
        grass_compute_stats.last_error = "Disabled by portfolio toggle";
      }
      else
      {
        EnsureTerrainGrassComputeSeeds(frame_context);

        grass_compute_stats.gpu_ready = m_terrain_grass_instances.HasSeeds();
        grass_compute_stats.grid_cols = m_terrain_grass_instances.GridCols();
        grass_compute_stats.grid_rows = m_terrain_grass_instances.GridRows();
        grass_compute_stats.seed_count = m_terrain_grass_instances.SeedCount();
        grass_compute_stats.max_instance_capacity = m_terrain_grass_instances.InstanceCount();
        grass_compute_stats.coverage_dirty = m_terrain_grass_instances.DispatchDirty();
        grass_compute_stats.last_error = m_terrain_grass_instances.LastError();

        bool drew_compute_patch = false;
        if (m_terrain_grass_instances.HasSeeds())
        {
          if (ShouldRefreshTerrainGrassCoverage(frame_context))
          {
            m_terrain_grass_instances.MarkCoverageDirty();
            m_terrain_grass_last_coverage_refresh_time_seconds = frame_context.globals.time_seconds;
          }
          if (m_terrain_grass_last_update_time_seconds != frame_context.globals.time_seconds)
          {
            m_terrain_grass_instances.Update(
                kDebugQuadScaleX,
                kDebugQuadScaleY,
                frame_context.globals.camera_position,
                view,
                proj,
                static_cast<float>(frame_context.globals.time_seconds));
            m_terrain_grass_last_update_time_seconds = frame_context.globals.time_seconds;
          }
          if (frame_context.resources.compute_shared_registry != nullptr)
          {
            frame_context.resources.compute_shared_registry->PublishStructuredBuffer(
                ComputeSharedResourceId::GrassInstances,
                "GrassInstances",
                m_terrain_grass_instances.InstanceBuffer(),
                m_terrain_grass_instances.InstanceSRV(),
                nullptr,
                m_terrain_grass_instances.InstanceCount(),
                m_terrain_grass_instances.InstanceStride());
            frame_context.resources.compute_shared_registry->PublishIndirectArgs(
                ComputeSharedResourceId::GrassIndirectArgs,
                "GrassIndirectArgs",
                m_terrain_grass_instances.ArgsBuffer());
          }
          Sprite3D_DrawCutoutInstancedIndirectBuffer(
              m_debug_billboard_tex_id,
              m_terrain_grass_instances.InstanceSRV(),
              m_terrain_grass_instances.ArgsBuffer(),
              {1.0f, 1.0f, 1.0f, 1.0f});
          drew_compute_patch = true;
        }

        grass_compute_stats.gpu_path_used = drew_compute_patch;
        grass_compute_stats.visible_instance_count = m_terrain_grass_instances.LastVisibleInstanceCount();
        grass_compute_stats.fallback_used = false;
      }
    }

    DebugMenu_SetGrassComputeStats(grass_compute_stats);
  }

  if (kDrawMeteorographPanelDebug && frame_context.resources.meteorograph_field.isValid())
  {
    const float panel_x = -22.0f;
    const float panel_z = 18.0f;
    const float ground_y = SampleTerrainHeightWorld(panel_x, panel_z);
    const float panel_width = 10.0f;
    const float panel_height = 5.5f;
    const float panel_yaw = 0.28f;
    const XMMATRIX panel_basis =
        XMMatrixRotationRollPitchYaw(0.0f, panel_yaw, 0.0f) *
        XMMatrixTranslation(panel_x, ground_y + 4.0f, panel_z);
    const XMMATRIX climate_panel_world =
        XMMatrixScaling(panel_width, panel_height, 0.18f) * panel_basis;

    Cube_DrawMaterialSRV(
        frame_context.resources.meteorograph_field.shaderResourceView(),
        climate_panel_world,
        {1.0f, 1.0f, 1.0f, 1.0f},
        RenderState{DepthMode::ReadWrite, BlendMode::Opaque, CullMode::None},
        MaterialType::Unlit);
  }

  ID3D11ShaderResourceView* null_srv = nullptr;
  Direct3D_GetContext()->PSSetShaderResources(2, 1, &null_srv);
}

bool MapController::GetWaterSurfaceDesc(WaterSurfaceDesc& out_desc) const
{
	out_desc = DebugMenu_GetWaterSurfaceSettings();
	out_desc.enabled = true;
	out_desc.center_x = 0.0f;
	out_desc.center_z = 0.0f;
	if (out_desc.height <= 0.0f)
	{
		out_desc.height = TerrainDataModel::ActiveWaterSurfaceHeight();
	}
	out_desc.size_x = kTerrainWorldHalfWidth * 2.0f;
	out_desc.size_z = kTerrainWorldHalfDepth * 2.0f;
	if (out_desc.base_color.w <= 0.0f)
	{
		out_desc.base_color = { 0.06f, 0.22f, 0.46f, 0.36f };
		out_desc.ripple_color = { 0.26f, 0.60f, 0.82f, 0.16f };
		out_desc.highlight_color = { 0.60f, 0.82f, 0.96f, 0.10f };
		out_desc.ripple_strength = 1.65f;
		out_desc.wind_influence = 1.85f;
		out_desc.edge_emphasis = 1.0f;
	}
	return true;
}

void MapController::RespawnFloatingDustParticle(
    FloatingDustParticle& particle,
    int particle_index,
    const XMFLOAT3& camera_position,
    const XMFLOAT3& camera_front,
    const XMFLOAT3& camera_right,
    const XMFLOAT3& camera_up) const
{
  const float id = static_cast<float>(particle_index) + 1.0f;
  const float side_offset = (Hash11(id * 1.173f) - 0.5f) * 20.0f;
  const float forward_offset = 7.0f + Hash11(id * 2.417f) * 28.0f;
  const float vertical_offset = -0.8f + Hash11(id * 3.731f) * 5.4f;
  const float phase = Hash11(id * 4.913f) * DirectX::XM_2PI;
  const float drift_speed = 0.95f + Hash11(id * 5.219f) * 1.85f;
  const float cross_speed = -0.32f + Hash11(id * 6.271f) * 0.64f;
  const float tint_seed = Hash11(id * 11.731f);
  const XMFLOAT4 tint = SampleDustPalette(tint_seed);

  const XMFLOAT3 world_wind = Normalize3({ 0.92f, 0.0f, 0.38f });
  particle.position = {
      camera_position.x + camera_front.x * forward_offset + camera_right.x * side_offset + camera_up.x * vertical_offset,
      camera_position.y + camera_front.y * forward_offset + camera_right.y * side_offset + camera_up.y * vertical_offset,
      camera_position.z + camera_front.z * forward_offset + camera_right.z * side_offset + camera_up.z * vertical_offset };
  particle.velocity = {
      world_wind.x * drift_speed + camera_right.x * cross_speed,
      0.03f + Hash11(id * 7.137f) * 0.12f,
      world_wind.z * drift_speed + camera_right.z * cross_speed };
  particle.size = 0.24f + Hash11(id * 8.411f) * 0.38f;
  particle.alpha = 0.26f + Hash11(id * 9.271f) * 0.24f;
  particle.phase = phase;
  particle.lifetime = 7.0f + Hash11(id * 10.193f) * 7.0f;
  particle.tint = tint;
}

void MapController::EnsureFloatingDustParticles(
    const XMFLOAT3& camera_position,
    const XMFLOAT3& camera_front,
    const XMFLOAT3& camera_right,
    const XMFLOAT3& camera_up)
{
  if (m_floating_dust_particles_initialized)
  {
    return;
  }

  m_floating_dust_particles.resize(kFloatingDustParticleCount);
  for (int i = 0; i < kFloatingDustParticleCount; ++i)
  {
    RespawnFloatingDustParticle(
        m_floating_dust_particles[static_cast<size_t>(i)],
        i,
        camera_position,
        camera_front,
        camera_right,
        camera_up);
  }
  m_floating_dust_particles_initialized = true;
}

void MapController::DrawTransparency(const RenderFrameContext& frame_context)
{
	(void)frame_context;
}

void MapController::DrawParticles(const RenderFrameContext& frame_context)
{
  if (!kDrawFloatingDustParticles)
  {
    return;
  }

  if (m_floating_light_tex_id < 0)
  {
    return;
  }

  const XMFLOAT3 camera_position = frame_context.globals.camera_position;
  const XMFLOAT3 camera_front = Camera_GetVector_Front();
  const XMMATRIX view = XMLoadFloat4x4(&frame_context.globals.view_matrix);
  const XMMATRIX inverse_view = XMMatrixInverse(nullptr, view);
  const XMFLOAT3 camera_right = {
      XMVectorGetX(inverse_view.r[0]),
      XMVectorGetY(inverse_view.r[0]),
      XMVectorGetZ(inverse_view.r[0]) };
  const XMFLOAT3 camera_up = {
      XMVectorGetX(inverse_view.r[1]),
      XMVectorGetY(inverse_view.r[1]),
      XMVectorGetZ(inverse_view.r[1]) };

  EnsureFloatingDustParticles(camera_position, camera_front, camera_right, camera_up);

  const float dt = static_cast<float>(std::clamp(frame_context.globals.elapsed_time, 0.0, 0.05));
  const float time_seconds = static_cast<float>(frame_context.globals.time_seconds);
  const auto& noise_settings = DebugMenu_GetComputeNoiseSettings();

  std::vector<XMFLOAT4X4> world_matrices;
  std::vector<XMFLOAT4> instance_colors;
  world_matrices.reserve(m_floating_dust_particles.size());
  instance_colors.reserve(m_floating_dust_particles.size());

  const XMVECTOR camera_position_v = XMLoadFloat3(&camera_position);
  const XMVECTOR camera_front_v = XMLoadFloat3(&camera_front);
  const XMVECTOR billboard_right_v = XMVector3Normalize(XMLoadFloat3(&camera_right));
  const XMVECTOR billboard_up_v = XMVector3Normalize(XMLoadFloat3(&camera_up));

  for (size_t i = 0; i < m_floating_dust_particles.size(); ++i)
  {
    auto& particle = m_floating_dust_particles[i];

    const XMFLOAT3 wind_sample =
        WindFieldCpu_SampleWorld(
            particle.position.x,
            particle.position.z,
            time_seconds,
            noise_settings);

    const XMFLOAT3 wind_dir = Normalize3({ wind_sample.x, 0.0f, wind_sample.y });
    const float wind_strength = std::clamp(wind_sample.z, 0.0f, 1.0f);

    particle.velocity.x = LerpScalar(particle.velocity.x, wind_dir.x * (1.8f + wind_strength * 2.6f), dt * 1.8f);
    particle.velocity.z = LerpScalar(particle.velocity.z, wind_dir.z * (1.8f + wind_strength * 2.6f), dt * 1.8f);
    particle.velocity.y = LerpScalar(
        particle.velocity.y,
        0.04f + std::sin(time_seconds * 0.9f + particle.phase) * 0.06f,
        dt * 1.2f);

    particle.position.x += particle.velocity.x * dt;
    particle.position.y += particle.velocity.y * dt;
    particle.position.z += particle.velocity.z * dt;
    particle.lifetime -= dt;

    const XMVECTOR particle_position_v = XMLoadFloat3(&particle.position);
    const XMVECTOR to_particle_v = particle_position_v - camera_position_v;
    const float forward_distance = XMVectorGetX(XMVector3Dot(to_particle_v, camera_front_v));
    const float distance_sq = XMVectorGetX(XMVector3LengthSq(to_particle_v));

    if (particle.lifetime <= 0.0f || forward_distance < 1.5f || distance_sq > (36.0f * 36.0f))
    {
      RespawnFloatingDustParticle(
          particle,
          static_cast<int>(i),
          camera_position,
          camera_front,
          camera_right,
          camera_up);
    }

    const float pulse = 0.82f + 0.28f * std::sin(time_seconds * 1.2f + particle.phase * 1.7f);
    const float alpha = particle.alpha * pulse;

    XMMATRIX world = XMMatrixIdentity();
    world.r[0] = billboard_right_v * particle.size;
    world.r[1] = billboard_up_v * particle.size;
    world.r[2] = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    world.r[3] = XMVectorSet(particle.position.x, particle.position.y, particle.position.z, 1.0f);

    XMFLOAT4X4 world_matrix{};
    XMStoreFloat4x4(&world_matrix, world);
    world_matrices.push_back(world_matrix);
    instance_colors.push_back({
        particle.tint.x,
        particle.tint.y,
        particle.tint.z,
        alpha });
  }

  if (world_matrices.empty())
  {
    return;
  }

  const XMMATRIX proj = XMLoadFloat4x4(&frame_context.globals.projection_matrix);
  ShaderSprite3D_TransparentInstanced_SetViewMatrix(view);
  ShaderSprite3D_TransparentInstanced_SetProjMatrix(proj);
  Sprite3D_DrawAdditiveInstancedColored(
      m_floating_light_tex_id,
      world_matrices,
      instance_colors,
      { 1.0f, 1.0f, 1.0f, 1.0f });
}

void MapController::DrawDepthPrePass()
{
  DrawShadow(nullptr);
}

void MapController::DrawShadow(const RenderFrameContext* frame_context)
{
  for (const MapObject& object : m_map_objects)
  {
    DrawObjectShadow(object);
  }

  if (m_terrain_visible &&
      frame_context != nullptr &&
      kDrawGrass &&
      kDrawTerrainGrassCompute &&
      DebugMenu_IsGrassGpuEnabled() &&
      m_debug_billboard_tex_id >= 0 &&
      m_terrain_grass_instances.HasSeeds())
  {
    const XMMATRIX view = XMLoadFloat4x4(&frame_context->globals.view_matrix);
    const XMMATRIX proj = XMLoadFloat4x4(&frame_context->globals.projection_matrix);
    if (ShouldRefreshTerrainGrassCoverage(*frame_context))
    {
      m_terrain_grass_instances.MarkCoverageDirty();
      m_terrain_grass_last_coverage_refresh_time_seconds = frame_context->globals.time_seconds;
    }
    if (m_terrain_grass_last_update_time_seconds != frame_context->globals.time_seconds)
    {
      m_terrain_grass_instances.Update(
          kDebugQuadScaleX,
          kDebugQuadScaleY,
          frame_context->globals.camera_position,
          view,
          proj,
          static_cast<float>(frame_context->globals.time_seconds));
      m_terrain_grass_last_update_time_seconds = frame_context->globals.time_seconds;
    }

    const auto& noise_settings = DebugMenu_GetComputeNoiseSettings();
    const float wrapped_time_seconds =
        std::fmod(static_cast<float>(frame_context->globals.time_seconds), 1024.0f);
    ShaderSprite3D_ShadowInstanced_SetLightViewProjection(Light_GetLightViewProjectionMatrix());
    ShaderSprite3D_ShadowInstanced_SetWindSettings(
        wrapped_time_seconds,
        {noise_settings.wind_direction_x, noise_settings.wind_direction_y},
        noise_settings.wind_strength,
        kClimateWorldMinX,
        kClimateWorldMaxX,
        kClimateWorldMinZ,
        kClimateWorldMaxZ);
    ShaderSprite3D_ShadowInstanced_SetWindField(frame_context->resources.wind_field.shaderResourceView());
    Sprite3D_DrawCutoutShadowInstancedIndirectBuffer(
        m_debug_billboard_tex_id,
        m_terrain_grass_instances.InstanceSRV(),
        m_terrain_grass_instances.ArgsBuffer());
  }
}

void MapController::AddBlock(const XMFLOAT3& position,
                             const XMFLOAT3& rotation,
                             const XMFLOAT3& scale)
{
  MapObject object{};
  object.KindId = BLOCK;
  object.Position = position;
  object.Collision.min = {position.x - 0.5f * scale.x,
                          position.y - 0.5f * scale.y,
                          position.z - 0.5f * scale.z};
  object.Collision.max = {position.x + 0.5f * scale.x,
                          position.y + 0.5f * scale.y,
                          position.z + 0.5f * scale.z};
  object.Rotation = rotation;
  object.Scale = scale;
  object.ShaderType = getDefaultShaderTypeForKind(object.KindId);
  m_map_objects.push_back(object);
}

void MapController::AddShape(int kindId,
                             const XMFLOAT3& position,
                             const XMFLOAT3& rotation,
                             const XMFLOAT3& scale)
{
  MapObject object{};
  object.KindId = kindId;
  object.Position = position;
  object.Collision.min = {position.x - 0.5f * scale.x,
                          position.y - 0.5f * scale.y,
                          position.z - 0.5f * scale.z};
  object.Collision.max = {position.x + 0.5f * scale.x,
                          position.y + 0.5f * scale.y,
                          position.z + 0.5f * scale.z};
  object.Rotation = rotation;
  object.Scale = scale;
  object.ShaderType = getDefaultShaderTypeForKind(object.KindId);
  m_map_objects.push_back(object);
}

void MapController::RemoveObject(int index)
{
  if (index >= 0 && index < static_cast<int>(m_map_objects.size()))
  {
    m_map_objects.erase(m_map_objects.begin() + index);
  }
}

int MapController::GetObjectsCount() const
{
  return static_cast<int>(m_map_objects.size());
}

MapObject* MapController::GetObject(int index)
{
  if (index >= 0 && index < static_cast<int>(m_map_objects.size()))
  {
    return &m_map_objects[index];
  }

  return nullptr;
}

void MapController::SaveToFile(const char* filename)
{
  FILE* file = nullptr;
  fopen_s(&file, filename, "w");
  if (file == nullptr)
  {
    return;
  }

  fprintf(file, "%d\n", static_cast<int>(m_map_objects.size()));
  for (const MapObject& object : m_map_objects)
  {
    const char* texture_path = object.TexturePath.empty() ? "-" : object.TexturePath.c_str();
    fprintf(file,
            "%d %d %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %s\n",
            object.KindId,
            static_cast<int>(object.ShaderType),
            object.Position.x,
            object.Position.y,
            object.Position.z,
            object.Rotation.x,
            object.Rotation.y,
            object.Rotation.z,
            object.Scale.x,
            object.Scale.y,
            object.Scale.z,
            object.Collision.min.x,
            object.Collision.min.y,
            object.Collision.min.z,
            texture_path);
  }

  fclose(file);
  removeGrassObjects(m_map_objects);
}

void MapController::LoadFromFile(const char* filename)
{
  if (isBackupMapPath(filename))
  {
    return;
  }

  FILE* file = nullptr;
  fopen_s(&file, filename, "r");
  if (file == nullptr)
  {
    return;
  }

  int count = 0;
  fscanf_s(file, "%d\n", &count);

  m_map_objects.clear();
  for (int index = 0; index < count; ++index)
  {
    MapObject object{};
    object.TextureId = -1;
    object.ShaderType = MapShaderType::Default;

    char line[512] = {};
    if (fgets(line, sizeof(line), file) == nullptr)
    {
      break;
    }

    std::istringstream line_stream(line);
    std::string texture_path;
    int shader_type_value = static_cast<int>(MapShaderType::Default);
    if (line_stream >> object.KindId >> shader_type_value >> object.Position.x >>
            object.Position.y >> object.Position.z >> object.Rotation.x >>
            object.Rotation.y >> object.Rotation.z >> object.Scale.x >>
            object.Scale.y >> object.Scale.z >> object.Collision.min.x >>
            object.Collision.min.y >> object.Collision.min.z >> texture_path)
    {
      object.ShaderType = static_cast<MapShaderType>(shader_type_value);
      if (texture_path == "-")
      {
        object.TexturePath.clear();
      }
      else if (texture_path.length() > 1 && texture_path[0] == '-')
      {
        object.TexturePath = texture_path.substr(1);
      }
      else
      {
        object.TexturePath = texture_path;
      }
    }
    else
    {
      line_stream.clear();
      line_stream.str(line);
      if (line_stream >> object.KindId >> object.Position.x >> object.Position.y >>
              object.Position.z >> object.Rotation.x >> object.Rotation.y >>
              object.Rotation.z >> object.Scale.x >> object.Scale.y >>
              object.Scale.z >> object.Collision.min.x >> object.Collision.min.y >>
              object.Collision.min.z >> texture_path)
      {
        object.ShaderType = getDefaultShaderTypeForKind(object.KindId);
        if (texture_path == "-")
        {
          object.TexturePath.clear();
        }
        else if (texture_path.length() > 1 && texture_path[0] == '-')
        {
          object.TexturePath = texture_path.substr(1);
        }
        else
        {
          object.TexturePath = texture_path;
        }
      }
      else
      {
        object.TexturePath.clear();
        object.ShaderType = getDefaultShaderTypeForKind(object.KindId);
      }
    }

    if (object.KindId == BLOCK || object.KindId == GRASS)
    {
      object.Collision.min = {object.Position.x - 0.5f * object.Scale.x,
                              object.Position.y - 0.5f * object.Scale.y,
                              object.Position.z - 0.5f * object.Scale.z};
      object.Collision.max = {object.Position.x + 0.5f * object.Scale.x,
                              object.Position.y + 0.5f * object.Scale.y,
                              object.Position.z + 0.5f * object.Scale.z};
    }

    m_map_objects.push_back(object);
  }

  fclose(file);
}

void MapController::PushUndoState()
{
  m_undo_stack.push_back(m_map_objects);
  if (static_cast<int>(m_undo_stack.size()) > kMaxUndo)
  {
    m_undo_stack.erase(m_undo_stack.begin());
  }

  m_redo_stack.clear();
}

void MapController::Undo()
{
  if (m_undo_stack.empty())
  {
    return;
  }

  m_redo_stack.push_back(m_map_objects);
  m_map_objects = m_undo_stack.back();
  m_undo_stack.pop_back();
}

void MapController::Redo()
{
  if (m_redo_stack.empty())
  {
    return;
  }

  m_undo_stack.push_back(m_map_objects);
  m_map_objects = m_redo_stack.back();
  m_redo_stack.pop_back();
}

void MapController::SetTerrainVisible(bool visible)
{
  m_terrain_visible = visible;
}

bool MapController::IsTerrainVisible() const
{
  return m_terrain_visible;
}
