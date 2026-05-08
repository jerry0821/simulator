// ----------------------------------------------------
// マップの管理 [map.h]
// ====================================================
// Created by: Jerry
// Date: 2025-11-10
// ----------------------------------------------------
#ifndef MAP_H
#define MAP_H

#include "collision.h"
#include "compute_grass_instances.h"
#include "meshfield.h"
#include "octree.h"
#include "render_water_surface.h"
#include <DirectXMath.h>
#include <string>
#include <vector>

struct RenderFrameContext;

enum ObjectKind {
  FIELD,
  BLOCK,
  GRASS,
  SLIME,
  ROCK,
  SPHERE,
  CYLINDER,
  CAPSULE,
};

enum class MapShaderType {
  Default = 0,
  Lit,
  Toon,
  Unlit,
};

struct MapObject {
  int KindId;
  DirectX::XMFLOAT3 Position;
  Collision::AABB Collision;
  DirectX::XMFLOAT3 Rotation; // XYZ Euler angles in radians
  DirectX::XMFLOAT3 Scale;
  MapShaderType ShaderType = MapShaderType::Default;
  std::string TexturePath; // 貼圖路徑 ("" = 預設)
  int TextureId = -1;      // Runtime texture ID
};

class MapController {
public:
  void Initialize();
  void Finalize();

  void Draw(const RenderFrameContext& frame_context);
  void DrawDepthPrePass();
  bool GetWaterSurfaceDesc(WaterSurfaceDesc& out_desc) const;
  void DrawTransparency(const RenderFrameContext& frame_context);
  void DrawParticles(const RenderFrameContext& frame_context);
  void DrawShadow(const RenderFrameContext* frame_context = nullptr);

  void AddBlock(const DirectX::XMFLOAT3 &position,
                const DirectX::XMFLOAT3 &rotation,
                const DirectX::XMFLOAT3 &scale = {1.0f, 1.0f, 1.0f});
  void AddShape(int kindId, const DirectX::XMFLOAT3 &position,
                const DirectX::XMFLOAT3 &rotation,
                const DirectX::XMFLOAT3 &scale = {1.0f, 1.0f, 1.0f});
  void RemoveObject(int index);

  int GetObjectsCount() const;
  MapObject *GetObject(int index);

  void SaveToFile(const char *filename);
  void LoadFromFile(const char *filename);

  void PushUndoState();
  void Undo();
  void Redo();

  void SetTerrainVisible(bool visible);
  bool IsTerrainVisible() const;

private:
  struct DemoInstanceData {
    DirectX::XMFLOAT4X4 world_matrix{};
    Collision::AABB bounds{};
  };

  struct FloatingDustParticle {
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 velocity{};
    float size = 0.35f;
    float alpha = 0.18f;
    float phase = 0.0f;
    float lifetime = 0.0f;
    DirectX::XMFLOAT4 tint{1.0f, 1.0f, 1.0f, 1.0f};
  };

  int ResolveMapTexture(MapObject &obj);
  float ResolveGroundedY(const MapObject& object) const;
  bool ShouldCullAabb(const class ViewFrustum& view_frustum,
                      const Collision::AABB& aabb) const;
  void AppendInstanceMatrix(std::vector<DirectX::XMFLOAT4X4> &instances,
                            const DirectX::XMMATRIX &world_matrix) const;
  std::vector<DemoInstanceData> BuildGrassDemoData(bool stress_mode) const;
  Octree BuildGrassDemoOctree(const std::vector<DemoInstanceData>& entries) const;
  void EnsureGrassDemoCache();
  void DrawInstancedGrassDemo(bool use_instancing,
                              const class ViewFrustum& view_frustum,
                              std::vector<DirectX::XMFLOAT4X4> &batch);
  void DrawInstancedBirdDemo(bool use_instancing,
                             const class ViewFrustum& view_frustum,
                             std::vector<DirectX::XMFLOAT4X4> &batch) const;
  void EnsureTerrainGrassComputeSeeds(const RenderFrameContext& frame_context);
  bool ShouldRefreshTerrainGrassCoverage(const RenderFrameContext& frame_context);
  DirectX::XMMATRIX BuildObjectWorldMatrix(const MapObject& object) const;
  void DrawObjectShadow(const MapObject& object) const;
  void EnsureFloatingDustParticles(
      const DirectX::XMFLOAT3& camera_position,
      const DirectX::XMFLOAT3& camera_front,
      const DirectX::XMFLOAT3& camera_right,
      const DirectX::XMFLOAT3& camera_up);
  void RespawnFloatingDustParticle(
      FloatingDustParticle& particle,
      int particle_index,
      const DirectX::XMFLOAT3& camera_position,
      const DirectX::XMFLOAT3& camera_front,
      const DirectX::XMFLOAT3& camera_right,
      const DirectX::XMFLOAT3& camera_up) const;

  std::vector<MapObject> m_map_objects;
  bool m_terrain_visible = true;
  std::vector<std::vector<MapObject>> m_undo_stack;
  std::vector<std::vector<MapObject>> m_redo_stack;

  int m_cube_tex_id = -1;
  int m_debug_billboard_tex_id = -1;
  int m_cube_grass_tex_id = -1;
  int m_height_map_tex_id = -1;
  int m_floating_light_tex_id = -1;
  int m_white_tex_id = -1;
  struct MODEL *m_grass_model = nullptr;
  struct MODEL *m_slime01 = nullptr;
  struct MODEL *m_rock = nullptr;

  std::vector<DemoInstanceData> m_grass_demo_normal;
  std::vector<DemoInstanceData> m_grass_demo_stress;
  Octree m_grass_demo_normal_octree;
  Octree m_grass_demo_stress_octree;
  bool m_grass_demo_cache_built = false;
  ComputeGrassInstances m_terrain_grass_instances{};
  bool m_terrain_grass_compute_seeded = false;
  TerrainSettings m_terrain_grass_seed_terrain_settings{};
  ID3D11ShaderResourceView* m_terrain_grass_seed_height_srv = nullptr;
  ID3D11ShaderResourceView* m_terrain_grass_seed_vegetation_suitability_srv = nullptr;
  ID3D11Texture2D* m_terrain_grass_vegetation_suitability_probe_texture = nullptr;
  double m_terrain_grass_last_update_time_seconds = -1.0;
  double m_terrain_grass_last_coverage_refresh_time_seconds = -1000.0;
  double m_terrain_grass_last_probe_time_seconds = -1000.0;
  float m_terrain_grass_last_vegetation_suitability_signature = -1.0f;
  std::vector<FloatingDustParticle> m_floating_dust_particles;
  bool m_floating_dust_particles_initialized = false;
};

#endif // MAP_H

