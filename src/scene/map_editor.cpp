#include "map_editor.h"
#include "camera.h"
#include "collision.h"
#include "cube.h"
#include "cylinder.h"
#include "direct3d.h"
#include "grid.h"
#include "imgui/imgui.h"
#include "map.h"
#include "meshfield.h"
#include "mouse.h"
#include "sphere.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <windows.h>

using namespace DirectX;

static bool g_hasHit = false;
static XMFLOAT3 g_hitPos = {0.0f, 0.0f, 0.0f};
static XMFLOAT3 g_currentRotation = {0.0f, 0.0f, 0.0f};
static int g_prevMouseX = 0;
static int g_prevMouseY = 0;

enum EditorMode { MODE_BUILD, MODE_SELECT };
static EditorMode g_CurrentMode = MODE_BUILD;

static int g_SelectedIndex = -1;
static std::vector<int> g_SelectedIndices; // Multi-select list.
static std::vector<MapObject> g_Clipboard; // Clipboard objects.
static int g_currentShape = 1;             // Default to BLOCK (1)

// Box selection state.
static bool g_DragBoxActive = false;
static int g_BoxStartX = 0, g_BoxStartY = 0;

// Build drag-to-fill state.
static bool g_IsBuildDragging = false;
static DirectX::XMFLOAT3 g_BuildDragStart = {0, 0, 0};

// Build mode texture.
static std::string g_BuildTexturePath;
static int g_BuildShaderType = static_cast<int>(MapShaderType::Lit);

// Shared texture scan list under resource/texture/.
static std::vector<std::string> g_TexFiles;
static bool g_TexScanned = false;
static void ScanTextures() {
  g_TexFiles.clear();
  WIN32_FIND_DATAA fd;
  HANDLE hFind = FindFirstFileA("resource/texture/*.*", &fd);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        std::string fname(fd.cFileName);
        std::string ext;
        auto dot = fname.rfind('.');
        if (dot != std::string::npos)
          ext = fname.substr(dot);
        for (auto &c : ext)
          c = (char)tolower((unsigned char)c);
        if (ext == ".png" || ext == ".jpg" || ext == ".bmp")
          g_TexFiles.push_back(std::string("resource/texture/") + fname);
      }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
  }
  std::sort(g_TexFiles.begin(), g_TexFiles.end());
  g_TexScanned = true;
}

MapEditorController::MapEditorController(MapController& map_controller)
    : m_map_controller(map_controller) {}

enum GizmoTool {
  TOOL_MOVE,  // W
  TOOL_SCALE, // E
  TOOL_ROTATE // R
};
static GizmoTool g_ActiveTool = TOOL_MOVE;

enum DragState {
  DRAG_NONE,
  DRAG_TRANS_X,
  DRAG_TRANS_Y,
  DRAG_TRANS_Z,
  DRAG_SCALE_X,
  DRAG_SCALE_Y,
  DRAG_SCALE_Z,
  DRAG_SCALE_UNIFORM,
  DRAG_ROTATE_Y,
  DRAG_CENTER
};
static DragState g_DragState = DRAG_NONE;
static XMFLOAT3 g_DragStartPos = {0, 0, 0};
static XMFLOAT3 g_DragStartScale = {1, 1, 1};
static XMFLOAT3 g_DragStartRot = {0.0f, 0.0f, 0.0f};

void MapEditorController::Update() {
  Mouse_State ms{};
  Mouse_GetState(&ms);

  static bool prev_left = false;
  bool curr_left = ms.leftButton;

  static bool prev_right = false;
  bool curr_right = ms.rightButton;

  // Right click and drag to freely rotate block (Only in build mode or when
  // selected)
  bool altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
  if (curr_right && prev_right && g_CurrentMode == MODE_BUILD && !altHeld) {
    int dx = ms.x - g_prevMouseX;
    g_currentRotation.y += dx * 0.01f; // Drag sensitivity
  }

  // Press 'R' in build mode to reset rotation
  if (g_CurrentMode == MODE_BUILD && (GetAsyncKeyState('R') & 0x8000)) {
    g_currentRotation = {0.0f, 0.0f, 0.0f};
  }
  ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
  ImGui::Begin("Map Editor");

  int modeInt = (int)g_CurrentMode;
  ImGui::RadioButton("Build Mode", &modeInt, MODE_BUILD);
  ImGui::SameLine();
  ImGui::RadioButton("Select Mode", &modeInt, MODE_SELECT);
  g_CurrentMode = (EditorMode)modeInt;

  ImGui::Separator();

  static bool isFlatMode = false;
  if (ImGui::Checkbox("Flat Terrain Mode", &isFlatMode)) {
    MeshFieldRenderer::SetFlatMode(isFlatMode);
  }

  bool terrainVis = m_map_controller.IsTerrainVisible();
  if (ImGui::Checkbox("Show Terrain", &terrainVis)) {
    m_map_controller.SetTerrainVisible(terrainVis);
  }

  ImGui::Separator();
  if (ImGui::Button("Save Map")) {
    m_map_controller.SaveToFile("resource/map_save.txt");
  }
  ImGui::SameLine();
  if (ImGui::Button("Load Map")) {
    m_map_controller.LoadFromFile("resource/map_save.txt");
    g_SelectedIndex = -1;
  }

  if (g_CurrentMode == MODE_BUILD) {
    ImGui::Text("Hold Right Click & Drag to Rotate");
    ImGui::Text("Current Rotation: %.2f degrees",
                XMConvertToDegrees(g_currentRotation.y));

    ImGui::Separator();
    const char *shapeNames[] = {"Field", "Cube",   "Grass",    "Slime",
                                "Rock",  "Sphere", "Cylinder", "Capsule"};
    ImGui::Combo("Shape", &g_currentShape, shapeNames,
                 IM_ARRAYSIZE(shapeNames));
    const char *shaderNames[] = {"Default", "Lit", "Toon", "Unlit"};
    ImGui::Combo("Shader", &g_BuildShaderType, shaderNames, IM_ARRAYSIZE(shaderNames));

    // Build mode texture picker.
    ImGui::Separator();
    ImGui::Text("Build Texture:");
    if (!g_TexScanned || ImGui::Button("Refresh##build"))
      ScanTextures();
    auto _sl = g_BuildTexturePath.rfind('/');
    std::string _disp =
        g_BuildTexturePath.empty()
            ? "(default)"
            : g_BuildTexturePath.substr(_sl != std::string::npos ? _sl + 1 : 0);
    ImGui::TextColored({0.6f, 1.0f, 0.6f, 1.0f}, "%s", _disp.c_str());
    if (ImGui::BeginListBox("##BuildTex", ImVec2(-1, 100))) {
      if (ImGui::Selectable("(default)", g_BuildTexturePath.empty()))
        g_BuildTexturePath = "";
      for (const auto &tf : g_TexFiles) {
        auto sl = tf.rfind('/');
        std::string disp = (sl != std::string::npos) ? tf.substr(sl + 1) : tf;
        bool sel = (g_BuildTexturePath == tf);
        if (ImGui::Selectable(disp.c_str(), sel))
          g_BuildTexturePath = tf;
        if (sel)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndListBox();
    }
  } else {
    const char *toolNames[] = {"Move (W)", "Scale (E)", "Rotate (R)"};
    ImGui::Text("Active Tool: %s", toolNames[g_ActiveTool]);
  }
  ImGui::End();

  // ========= Properties Window (separate) =========
  ImGui::Begin("Properties");
  if (g_SelectedIndex != -1) {
    MapObject *selInfo = m_map_controller.GetObject(g_SelectedIndex);
    if (selInfo) {
      const char *kindNames[] = {"Field", "Block",  "Grass",    "Slime",
                                 "Rock",  "Sphere", "Cylinder", "Capsule"};
      ImGui::Text("Type: %s  (Index: %d)", kindNames[selInfo->KindId],
                  g_SelectedIndex);

      ImGui::DragFloat3("Position", &selInfo->Position.x, 0.05f);

      XMFLOAT3 rotDeg = {XMConvertToDegrees(selInfo->Rotation.x),
                         XMConvertToDegrees(selInfo->Rotation.y),
                         XMConvertToDegrees(selInfo->Rotation.z)};
      if (ImGui::DragFloat3("Rotation", &rotDeg.x, 1.0f, -360.0f, 360.0f,
                            "%.1f")) {
        selInfo->Rotation.x = XMConvertToRadians(rotDeg.x);
        selInfo->Rotation.y = XMConvertToRadians(rotDeg.y);
        selInfo->Rotation.z = XMConvertToRadians(rotDeg.z);
      }

      ImGui::DragFloat3("Scale", &selInfo->Scale.x, 0.05f, 0.1f, 50.0f);
      const char *shaderNames[] = {"Default", "Lit", "Toon", "Unlit"};
      int shaderType = static_cast<int>(selInfo->ShaderType);
      if (ImGui::Combo("Shader", &shaderType, shaderNames, IM_ARRAYSIZE(shaderNames))) {
        selInfo->ShaderType = static_cast<MapShaderType>(shaderType);
      }

      // --- Texture Picker ---
      ImGui::Separator();
      ImGui::Text("Texture:");

      // Reuse the scanned texture list.
      if (!g_TexScanned || ImGui::Button("Refresh Textures"))
        ScanTextures();

      // Show current texture
      std::string curTex =
          selInfo->TexturePath.empty() ? "(default)" : selInfo->TexturePath;
      ImGui::TextColored({0.6f, 1.0f, 0.6f, 1.0f}, "Current: %s",
                         curTex.c_str());

      // Listbox to pick a texture
      if (ImGui::BeginListBox("##TexList", ImVec2(-1, 120))) {
        // First item: reset to default
        bool isDefault = selInfo->TexturePath.empty();
        if (ImGui::Selectable("(default)", isDefault)) {
          selInfo->TexturePath = "";
          selInfo->TextureId = -1;
        }
        for (int ti = 0; ti < (int)g_TexFiles.size(); ++ti) {
          bool isSel = (selInfo->TexturePath == g_TexFiles[ti]);
          // Show only filename for readability
          auto slash = g_TexFiles[ti].rfind('/');
          std::string display = (slash != std::string::npos)
                                    ? g_TexFiles[ti].substr(slash + 1)
                                    : g_TexFiles[ti];
          if (ImGui::Selectable(display.c_str(), isSel)) {
            selInfo->TexturePath = g_TexFiles[ti];
            selInfo->TextureId = -1; // Force reload
          }
          if (isSel)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
      }

      // Apply to ALL selected if multi-select
      if (!g_SelectedIndices.empty() &&
          ImGui::Button("Apply to All Selected")) {
        for (int midx : g_SelectedIndices) {
          MapObject *mo = m_map_controller.GetObject(midx);
          if (mo) {
            mo->TexturePath = selInfo->TexturePath;
            mo->TextureId = -1;
            mo->ShaderType = selInfo->ShaderType;
          }
        }
      }
    }
  } else {
    ImGui::TextDisabled("No object selected.");
  }
  ImGui::End();

  // ========= Object List Window (separate) =========
  ImGui::Begin("Object List");
  int objCount = m_map_controller.GetObjectsCount();
  ImGui::Text("Total: %d", objCount);
  ImGui::BeginChild("ObjList", ImVec2(0, 300), true);
  for (int i = 0; i < objCount; ++i) {
    MapObject *obj = m_map_controller.GetObject(i);
    if (!obj)
      continue;
    const char *kindNames[] = {"Field", "Block",  "Grass",    "Slime",
                               "Rock",  "Sphere", "Cylinder", "Capsule"};
    char label[64];
    snprintf(label, sizeof(label), "[%d] %s (%.1f, %.1f, %.1f)", i,
             kindNames[obj->KindId], obj->Position.x, obj->Position.y,
             obj->Position.z);
    bool isSelected =
        (g_SelectedIndex == i) ||
        std::find(g_SelectedIndices.begin(), g_SelectedIndices.end(), i) !=
            g_SelectedIndices.end();
    if (ImGui::Selectable(label, isSelected)) {
      g_SelectedIndices.clear();
      g_SelectedIndices.push_back(i);
      g_SelectedIndex = i;
      g_CurrentMode = MODE_SELECT; // Auto-switch to Select Mode
    }
  }
  ImGui::EndChild();
  ImGui::End();

  // W/E/R key switching (only in Select mode & not typing in ImGui)
  if (!ImGui::GetIO().WantCaptureKeyboard) {
    if (g_CurrentMode == MODE_SELECT) {
      if (GetAsyncKeyState('W') & 0x8000)
        g_ActiveTool = TOOL_MOVE;
      if (GetAsyncKeyState('E') & 0x8000)
        g_ActiveTool = TOOL_SCALE;
      if (GetAsyncKeyState('R') & 0x8000)
        g_ActiveTool = TOOL_ROTATE;
    }

    // Delete selected object(s)
    static bool prevDelete = false;
    bool currDelete = (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0;
    if (currDelete && !prevDelete) {
      if (!g_SelectedIndices.empty()) {
        m_map_controller.PushUndoState();
        // Remove in reverse order to keep indices stable.
        std::sort(g_SelectedIndices.begin(), g_SelectedIndices.end(),
                  std::greater<int>());
        for (int idx : g_SelectedIndices)
          m_map_controller.RemoveObject(idx);
        g_SelectedIndices.clear();
        g_SelectedIndex = -1;
        g_DragState = DRAG_NONE;
      } else if (g_SelectedIndex != -1) {
        m_map_controller.PushUndoState();
        m_map_controller.RemoveObject(g_SelectedIndex);
        g_SelectedIndex = -1;
        g_DragState = DRAG_NONE;
      }
    }
    prevDelete = currDelete;

    // Ctrl+S to save
    static bool prevSave = false;
    bool currSave = (GetAsyncKeyState('S') & 0x8000) != 0 &&
                    (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (currSave && !prevSave) {
      m_map_controller.SaveToFile("resource/map_save.txt");
    }
    prevSave = currSave;

    // Ctrl+C to copy
    static bool prevCopy = false;
    bool currCopy = (GetAsyncKeyState('C') & 0x8000) != 0 &&
                    (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (currCopy && !prevCopy) {
      g_Clipboard.clear();
      // Copy the single selection if multi-select is empty.
      if (!g_SelectedIndices.empty()) {
        for (int idx : g_SelectedIndices) {
          MapObject *o = m_map_controller.GetObject(idx);
          if (o)
            g_Clipboard.push_back(*o);
        }
      } else if (g_SelectedIndex != -1) {
        MapObject *o = m_map_controller.GetObject(g_SelectedIndex);
        if (o)
          g_Clipboard.push_back(*o);
      }
    }
    prevCopy = currCopy;

    // Ctrl+V to paste
    static bool prevPaste = false;
    bool currPaste = (GetAsyncKeyState('V') & 0x8000) != 0 &&
                     (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (currPaste && !prevPaste && !g_Clipboard.empty()) {
      m_map_controller.PushUndoState();
      g_SelectedIndices.clear();
      int newBase = m_map_controller.GetObjectsCount();
      for (const auto &obj : g_Clipboard) {
        MapObject newObj = obj;
        newObj.Position.x += 1.0f; // Small offset to avoid overlap.
        m_map_controller.AddShape(newObj.KindId, newObj.Position, newObj.Rotation,
                     newObj.Scale);
        g_SelectedIndices.push_back(newBase++);
      }
      g_SelectedIndex =
          g_SelectedIndices.empty() ? -1 : g_SelectedIndices.back();
      g_CurrentMode = MODE_SELECT;
    }
    prevPaste = currPaste;

    // Ctrl+Z to undo
    static bool prevUndo = false;
    bool currUndo = (GetAsyncKeyState('Z') & 0x8000) != 0 &&
                    (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (currUndo && !prevUndo) {
      m_map_controller.Undo();
      g_SelectedIndex = -1;
      g_SelectedIndices.clear();
      g_DragState = DRAG_NONE;
    }
    prevUndo = currUndo;
  } // end WantCaptureKeyboard

  XMFLOAT4X4 mtxView = Camera_GetMatrix();

  XMFLOAT3 test_near = Direct3D_ScreenToWorld(ms.x, ms.y, 0.0f, mtxView,
                                              Camera_GetPerspectiveMatrix());
  XMFLOAT3 test_far = Direct3D_ScreenToWorld(ms.x, ms.y, 1.0f, mtxView,
                                             Camera_GetPerspectiveMatrix());

  XMVECTOR vtest = XMLoadFloat3(&test_far) - XMLoadFloat3(&test_near);
  vtest = XMVector3Normalize(vtest);

  XMFLOAT3 dir;
  XMStoreFloat3(&dir, vtest);

  g_hasHit = false;

  float closestDist = 100000.0f;
  XMFLOAT3 bestHitPos = {0.0f, 0.0f, 0.0f};
  bool hitAnyBlock = false;

  if (g_CurrentMode == MODE_BUILD) {
    // 1. Ray-OBB check against all existing blocks
    const int buildObjectCount = m_map_controller.GetObjectsCount();
    int hitBlockIndex = -1;
    if (!ImGui::GetIO().WantCaptureMouse &&
        !altHeld) { // Skip when Alt held (camera)
      for (int i = 0; i < buildObjectCount; ++i) {
        MapObject *obj = m_map_controller.GetObject(i);
        if (obj && obj->KindId != FIELD) { // Check all placeable objects
          XMMATRIX objWorld =
              XMMatrixScaling(obj->Scale.x, obj->Scale.y, obj->Scale.z) *
              XMMatrixRotationRollPitchYaw(obj->Rotation.x, obj->Rotation.y,
                                           obj->Rotation.z) *
              XMMatrixTranslationFromVector(XMLoadFloat3(&obj->Position));
          Collision::RayHit hit;
          if (obj->KindId == SPHERE) {
            hit = Collision::IntersectRaySphere(test_near, dir, 0.5f, objWorld);
          } else if (obj->KindId == CYLINDER) {
            hit = Collision::IntersectRayCylinder(test_near, dir, 0.5f, 0.5f,
                                                 objWorld);
          } else if (obj->KindId == CAPSULE) {
            hit = Collision::IntersectRayCapsule(test_near, dir, 0.5f, 0.5f,
                                                objWorld);
          } else {
            Collision::AABB localAabb = Cube_GetAABB({0.0f, 0.0f, 0.0f});
            hit =
                Collision::IntersectRayOBB(test_near, dir, localAabb, objWorld);
          }
          if (hit.isHit && hit.distance < closestDist) {
            closestDist = hit.distance;
            hitBlockIndex = i;

            // Place the block adjacent to the collided face
            bestHitPos = {obj->Position.x + hit.normal.x * 1.0f,
                          obj->Position.y + hit.normal.y * 1.0f,
                          obj->Position.z + hit.normal.z * 1.0f};
            hitAnyBlock = true;
          }
        }
      }
      g_SelectedIndex = hitBlockIndex; // Track which block the cursor is over

      // 2. Fallback to Ray-Plane intersection if no blocks clicked
      if (!hitAnyBlock && dir.y != 0.0f) {
        float t = -test_near.y / dir.y;
        if (t > 0.0f) {
          bestHitPos = {test_near.x + dir.x * t, 0.5f, test_near.z + dir.z * t};
          hitAnyBlock = true;
        }
      }

      if (hitAnyBlock) {
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
          bestHitPos.x = floorf(bestHitPos.x) + 0.5f;
          bestHitPos.z = floorf(bestHitPos.z) + 0.5f;
        }

        if (!curr_right) { // Lock position while holding right click to rotate
          g_hitPos = bestHitPos;
        }
        g_hasHit = true;

        // No inline drawing here, editor draw handles the preview.

        if (curr_left && !prev_left) {
          if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
            g_IsBuildDragging = true;
            g_BuildDragStart = g_hitPos;
          } else {
            m_map_controller.PushUndoState();
            m_map_controller.AddShape(g_currentShape, g_hitPos, g_currentRotation,
                         {1.0f, 1.0f, 1.0f});
            {
              int lastIdx = m_map_controller.GetObjectsCount() - 1;
              MapObject *newObj = m_map_controller.GetObject(lastIdx);
              if (newObj) {
                newObj->ShaderType = static_cast<MapShaderType>(g_BuildShaderType);
              }
            }
            if (!g_BuildTexturePath.empty()) {
              int lastIdx = m_map_controller.GetObjectsCount() - 1;
              MapObject *newObj = m_map_controller.GetObject(lastIdx);
              if (newObj) {
                newObj->TexturePath = g_BuildTexturePath;
                newObj->TextureId = -1;
                newObj->ShaderType = static_cast<MapShaderType>(g_BuildShaderType);
              }
            }
          }
        } else if (!curr_left && prev_left && g_IsBuildDragging) {
          g_IsBuildDragging = false;
          m_map_controller.PushUndoState();

          // Determine grid bounds based on drag start and end (current hit)
          float minX = std::min(g_BuildDragStart.x, g_hitPos.x);
          float maxX = std::max(g_BuildDragStart.x, g_hitPos.x);
          float minZ = std::min(g_BuildDragStart.z, g_hitPos.z);
          float maxZ = std::max(g_BuildDragStart.z, g_hitPos.z);
          float ypos =
              g_BuildDragStart.y; // Keep the same height as the start point

          for (float x = minX; x <= maxX; x += 1.0f) {
            for (float z = minZ; z <= maxZ; z += 1.0f) {
              m_map_controller.AddShape(g_currentShape, {x, ypos, z}, g_currentRotation,
                           {1.0f, 1.0f, 1.0f});
              {
                int lastIdx = m_map_controller.GetObjectsCount() - 1;
                MapObject *newObj = m_map_controller.GetObject(lastIdx);
                if (newObj) {
                  newObj->ShaderType = static_cast<MapShaderType>(g_BuildShaderType);
                }
              }
              if (!g_BuildTexturePath.empty()) {
                int lastIdx = m_map_controller.GetObjectsCount() - 1;
                MapObject *newObj = m_map_controller.GetObject(lastIdx);
                if (newObj) {
                  newObj->TexturePath = g_BuildTexturePath;
                  newObj->TextureId = -1;
                  newObj->ShaderType = static_cast<MapShaderType>(g_BuildShaderType);
                }
              }
            }
          }
        }
      }
    }
  } else if (g_CurrentMode == MODE_SELECT) {
    // SELECT MODE LOGIC
    if (!ImGui::GetIO().WantCaptureMouse && !altHeld) { // Skip when Alt held
      if (g_DragState == DRAG_NONE) {
        if (curr_left && !prev_left) {
          int clickedIndex = -1;
          float closestObjDist = 100000.0f;

          if (g_SelectedIndex != -1) {
            MapObject *selObj = m_map_controller.GetObject(g_SelectedIndex);
            if (selObj) {
              XMMATRIX selWorldRot = XMMatrixRotationRollPitchYaw(
                                         selObj->Rotation.x, selObj->Rotation.y,
                                         selObj->Rotation.z) *
                                     XMMatrixTranslationFromVector(
                                         XMLoadFloat3(&selObj->Position));

              // Center handle (bigger hitbox)
              Collision::AABB handleCenter = {{-0.2f, -0.2f, -0.2f}, {0.2f, 0.2f, 0.2f}};
              // Axis handles (match cone positions based on scale)
              float hLenX = 0.7f * selObj->Scale.x;
              float hLenY = 0.7f * selObj->Scale.y;
              float hLenZ = 0.7f * selObj->Scale.z;
              Collision::AABB handleAxisX = {{hLenX - 0.15f, -0.15f, -0.15f},
                                  {hLenX + 0.35f, 0.15f, 0.15f}};
              Collision::AABB handleAxisY = {{-0.15f, hLenY - 0.15f, -0.15f},
                                  {0.15f, hLenY + 0.35f, 0.15f}};
              Collision::AABB handleAxisZ = {{-0.15f, -0.15f, hLenZ - 0.15f},
                                  {0.15f, 0.15f, hLenZ + 0.35f}};

              // Check center first
              Collision::RayHit hitC = Collision::IntersectRayOBB(
                  test_near, dir, handleCenter, selWorldRot);
              if (hitC.isHit && hitC.distance < closestObjDist) {
                closestObjDist = hitC.distance;
                g_DragState = DRAG_CENTER;
              }

              // Check axis handles (override center if closer)
              Collision::RayHit hitX = Collision::IntersectRayOBB(test_near, dir,
                                                      handleAxisX, selWorldRot);
              if (hitX.isHit && hitX.distance < closestObjDist) {
                closestObjDist = hitX.distance;
                if (g_ActiveTool == TOOL_MOVE)
                  g_DragState = DRAG_TRANS_X;
                else if (g_ActiveTool == TOOL_SCALE)
                  g_DragState = DRAG_SCALE_X;
                else
                  g_DragState = DRAG_ROTATE_Y;
              }
              Collision::RayHit hitY = Collision::IntersectRayOBB(test_near, dir,
                                                      handleAxisY, selWorldRot);
              if (hitY.isHit && hitY.distance < closestObjDist) {
                closestObjDist = hitY.distance;
                if (g_ActiveTool == TOOL_MOVE)
                  g_DragState = DRAG_TRANS_Y;
                else if (g_ActiveTool == TOOL_SCALE)
                  g_DragState = DRAG_SCALE_Y;
                else
                  g_DragState = DRAG_ROTATE_Y;
              }
              Collision::RayHit hitZ = Collision::IntersectRayOBB(test_near, dir,
                                                      handleAxisZ, selWorldRot);
              if (hitZ.isHit && hitZ.distance < closestObjDist) {
                closestObjDist = hitZ.distance;
                if (g_ActiveTool == TOOL_MOVE)
                  g_DragState = DRAG_TRANS_Z;
                else if (g_ActiveTool == TOOL_SCALE)
                  g_DragState = DRAG_SCALE_Z;
                else
                  g_DragState = DRAG_ROTATE_Y;
              }

              if (g_DragState != DRAG_NONE) {
                m_map_controller.PushUndoState();
                g_DragStartPos = selObj->Position;
                g_DragStartScale = selObj->Scale;
                g_DragStartRot = selObj->Rotation;
                clickedIndex = g_SelectedIndex;
              }
            }
          }

          // If no gizmo handle clicked, check for block selection
          if (clickedIndex == -1) {
            const int selectObjectCount = m_map_controller.GetObjectsCount();
            for (int i = 0; i < selectObjectCount; ++i) {
              MapObject *obj = m_map_controller.GetObject(i);
              if (obj && obj->KindId != FIELD) {
                XMMATRIX objWorld =
                    XMMatrixScaling(obj->Scale.x, obj->Scale.y, obj->Scale.z) *
                    XMMatrixRotationRollPitchYaw(
                        obj->Rotation.x, obj->Rotation.y, obj->Rotation.z) *
                    XMMatrixTranslationFromVector(XMLoadFloat3(&obj->Position));
                Collision::RayHit hit;
                if (obj->KindId == SPHERE) {
                  hit = Collision::IntersectRaySphere(test_near, dir, 0.5f,
                                                     objWorld);
                } else if (obj->KindId == CYLINDER) {
                  hit = Collision::IntersectRayCylinder(test_near, dir, 0.5f,
                                                       0.5f, objWorld);
                } else if (obj->KindId == CAPSULE) {
                  hit = Collision::IntersectRayCapsule(test_near, dir, 0.5f,
                                                      0.5f, objWorld);
                } else {
                  Collision::AABB localAabb = Cube_GetAABB({0.0f, 0.0f, 0.0f});
                  hit = Collision::IntersectRayOBB(test_near, dir, localAabb,
                                                  objWorld);
                }
                if (hit.isHit && hit.distance < closestObjDist) {
                  closestObjDist = hit.distance;
                  clickedIndex = i;
                }
              }
            }
            // Shift+Click toggles multi-select.
            // Shift+Click toggles multi-select.
            bool shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            if (clickedIndex != -1) {
              if (shiftHeld) {
                // Toggle in multi-select
                auto it = std::find(g_SelectedIndices.begin(),
                                    g_SelectedIndices.end(), clickedIndex);
                if (it != g_SelectedIndices.end()) {
                  g_SelectedIndices.erase(it);
                  g_SelectedIndex =
                      g_SelectedIndices.empty() ? -1 : g_SelectedIndices.back();
                } else {
                  g_SelectedIndices.push_back(clickedIndex);
                  g_SelectedIndex = clickedIndex;
                }
              } else {
                g_SelectedIndices.clear();
                g_SelectedIndices.push_back(clickedIndex);
                g_SelectedIndex = clickedIndex;
              }
            } else if (!shiftHeld) {
              // Start box selection when clicking empty space.
              // Start box selection when clicking empty space.
              g_DragBoxActive = true;
              g_BoxStartX = ms.x;
              g_BoxStartY = ms.y;
              g_SelectedIndices.clear();
              g_SelectedIndex = -1;
            }
          }
        }
      } else {
        // While dragging...
        if (g_SelectedIndex != -1) {
          MapObject *selObj = m_map_controller.GetObject(g_SelectedIndex);
          if (selObj) {
            int dx = ms.x - g_prevMouseX;
            int dy = ms.y - g_prevMouseY;
            float sensitivity = 0.01f;

            if (g_DragState == DRAG_SCALE_X) {
              selObj->Scale.x =
                  std::max(0.1f, selObj->Scale.x + dx * sensitivity);
            } else if (g_DragState == DRAG_SCALE_Y) {
              selObj->Scale.y =
                  std::max(0.1f, selObj->Scale.y - dy * sensitivity);
            } else if (g_DragState == DRAG_SCALE_Z) {
              selObj->Scale.z =
                  std::max(0.1f, selObj->Scale.z + dx * sensitivity);
            } else if (g_DragState == DRAG_SCALE_UNIFORM ||
                       (g_DragState == DRAG_CENTER &&
                        g_ActiveTool == TOOL_SCALE)) {
              float uniScale = dx * sensitivity - dy * sensitivity;
              selObj->Scale.x = std::max(0.1f, selObj->Scale.x + uniScale);
              selObj->Scale.y = std::max(0.1f, selObj->Scale.y + uniScale);
              selObj->Scale.z = std::max(0.1f, selObj->Scale.z + uniScale);
            } else if (g_DragState == DRAG_TRANS_X) {
              XMFLOAT3 xDir(dx * sensitivity * 2.0f, 0, 0);
              XMVECTOR delta = XMVector3TransformNormal(
                  XMLoadFloat3(&xDir),
                  XMMatrixRotationRollPitchYaw(selObj->Rotation.x,
                                               selObj->Rotation.y,
                                               selObj->Rotation.z));
              XMFLOAT3 d;
              XMStoreFloat3(&d, delta);
              selObj->Position.x += d.x;
              selObj->Position.z += d.z;
            } else if (g_DragState == DRAG_TRANS_Y) {
              selObj->Position.y -= dy * sensitivity * 2.0f;
            } else if (g_DragState == DRAG_TRANS_Z) {
              XMFLOAT3 zDir(0, 0, dx * sensitivity * 2.0f);
              XMVECTOR delta = XMVector3TransformNormal(
                  XMLoadFloat3(&zDir),
                  XMMatrixRotationRollPitchYaw(selObj->Rotation.x,
                                               selObj->Rotation.y,
                                               selObj->Rotation.z));
              XMFLOAT3 d;
              XMStoreFloat3(&d, delta);
              selObj->Position.x += d.x;
              selObj->Position.z += d.z;
            } else if (g_DragState == DRAG_ROTATE_Y ||
                       (g_DragState == DRAG_CENTER &&
                        g_ActiveTool == TOOL_ROTATE)) {
              selObj->Rotation.y += dx * 0.01f;
            } else if (g_DragState == DRAG_CENTER &&
                       g_ActiveTool == TOOL_MOVE) {
              // Move on XZ plane
              XMFLOAT3 moveDir(dx * sensitivity * 2.0f, 0, 0);
              XMVECTOR delta = XMVector3TransformNormal(
                  XMLoadFloat3(&moveDir),
                  XMMatrixRotationRollPitchYaw(selObj->Rotation.x,
                                               selObj->Rotation.y,
                                               selObj->Rotation.z));
              XMFLOAT3 d;
              XMStoreFloat3(&d, delta);
              selObj->Position.x += d.x;
              selObj->Position.z += d.z;
              selObj->Position.y -= dy * sensitivity * 2.0f;
            }
          }
        }
      }
      if (!curr_left && prev_left) {
        if (g_DragBoxActive) {
          // Finish box selection by projecting visible objects.
          int x0 = std::min(g_BoxStartX, ms.x);
          int x1 = std::max(g_BoxStartX, ms.x);
          int y0 = std::min(g_BoxStartY, ms.y);
          int y1 = std::max(g_BoxStartY, ms.y);
          if (x1 - x0 > 4 || y1 - y0 > 4) { // Require a minimum drag area.
            XMFLOAT4X4 mtxV = Camera_GetMatrix();
            XMFLOAT4X4 mtxP = Camera_GetPerspectiveMatrix();
            int count = m_map_controller.GetObjectsCount();
            RECT viewport;
            GetClientRect(GetForegroundWindow(), &viewport);
            float vw = (float)(viewport.right - viewport.left);
            float vh = (float)(viewport.bottom - viewport.top);
            for (int i = 0; i < count; ++i) {
              MapObject *obj = m_map_controller.GetObject(i);
              if (!obj || obj->KindId == FIELD)
                continue;
              XMVECTOR worldPos = XMLoadFloat3(&obj->Position);
              XMVECTOR clip = XMVector3TransformCoord(
                  worldPos, XMLoadFloat4x4(&mtxV) * XMLoadFloat4x4(&mtxP));
              XMFLOAT3 ndc;
              XMStoreFloat3(&ndc, clip);
              int sx = (int)((ndc.x * 0.5f + 0.5f) * vw);
              int sy = (int)((-ndc.y * 0.5f + 0.5f) * vh);
              if (ndc.z > 0.0f && sx >= x0 && sx <= x1 && sy >= y0 &&
                  sy <= y1) {
                if (std::find(g_SelectedIndices.begin(),
                              g_SelectedIndices.end(),
                              i) == g_SelectedIndices.end())
                  g_SelectedIndices.push_back(i);
              }
            }
            g_SelectedIndex =
                g_SelectedIndices.empty() ? -1 : g_SelectedIndices.back();
            if (!g_SelectedIndices.empty())
              g_CurrentMode = MODE_SELECT;
          }
          g_DragBoxActive = false;
        }
        g_DragState = DRAG_NONE; // Reset drag state on mouse release
      }
    }
  }

  prev_left = curr_left;
  prev_right = curr_right;

  g_prevMouseX = ms.x;
  g_prevMouseY = ms.y;
}

void MapEditorController::Draw() {
  Grid_Draw();

  if (g_CurrentMode == MODE_BUILD) {
    if (g_hasHit) {
      Collision::AABB local_aabb = Cube_GetAABB({0.0f, 0.0f, 0.0f});

      if (g_IsBuildDragging) {
        float minX = std::min(g_BuildDragStart.x, g_hitPos.x);
        float maxX = std::max(g_BuildDragStart.x, g_hitPos.x);
        float minZ = std::min(g_BuildDragStart.z, g_hitPos.z);
        float maxZ = std::max(g_BuildDragStart.z, g_hitPos.z);
        float ypos = g_BuildDragStart.y;
        for (float x = minX; x <= maxX; x += 1.0f) {
          for (float z = minZ; z <= maxZ; z += 1.0f) {
            XMFLOAT3 pos = {x, ypos, z};
            XMMATRIX previewWorld =
                XMMatrixRotationRollPitchYaw(g_currentRotation.x,
                                             g_currentRotation.y,
                                             g_currentRotation.z) *
                XMMatrixTranslationFromVector(XMLoadFloat3(&pos));
            if (g_currentShape == SPHERE)
              Collision::Debug::DrawSphereMtx(0.5f, previewWorld,
                                           {1.0f, 1.0f, 0.0f, 1.0f});
            else if (g_currentShape == CYLINDER || g_currentShape == CAPSULE)
              Collision::Debug::DrawCapsuleMtx(0.5f, 0.5f, previewWorld,
                                            {1.0f, 1.0f, 0.0f, 1.0f});
            else
              Collision::Debug::DrawOBB(local_aabb, previewWorld,
                                     {1.0f, 1.0f, 0.0f, 1.0f});
          }
        }
      } else {
        XMMATRIX previewWorld =
            XMMatrixRotationRollPitchYaw(
                g_currentRotation.x, g_currentRotation.y, g_currentRotation.z) *
            XMMatrixTranslationFromVector(XMLoadFloat3(&g_hitPos));
        if (g_currentShape == SPHERE) {
          Collision::Debug::DrawSphereMtx(0.5f, previewWorld,
                                       {1.0f, 1.0f, 0.0f, 1.0f});
        } else if (g_currentShape == CYLINDER || g_currentShape == CAPSULE) {
          Collision::Debug::DrawCapsuleMtx(0.5f, 0.5f, previewWorld,
                                        {1.0f, 1.0f, 0.0f, 1.0f});
        } else {
          Collision::Debug::DrawOBB(local_aabb, previewWorld,
                                 {1.0f, 1.0f, 0.0f, 1.0f});
        }
      }
    }
  }

  // ========= Draw Gizmo for Selected Block (in BOTH modes) =========
  if (g_SelectedIndex != -1 && g_CurrentMode == MODE_SELECT) {
    MapObject *selObj = m_map_controller.GetObject(g_SelectedIndex);
    if (selObj) {
      // Disable depth so ALL gizmo elements draw on top of blocks
      Direct3D_SetDepthEnable(false);

      // Yellow outline around selected block (with scale)
      Collision::AABB localAabb = Cube_GetAABB({0.0f, 0.0f, 0.0f});
      XMMATRIX selWorldFull =
          XMMatrixScaling(selObj->Scale.x, selObj->Scale.y, selObj->Scale.z) *
          XMMatrixRotationRollPitchYaw(selObj->Rotation.x, selObj->Rotation.y,
                                       selObj->Rotation.z) *
          XMMatrixTranslationFromVector(XMLoadFloat3(&selObj->Position));

      if (selObj->KindId == SPHERE) {
        Collision::Debug::DrawSphereMtx(0.5f, selWorldFull,
                                     {1.0f, 1.0f, 0.0f, 1.0f});
      } else if (selObj->KindId == CYLINDER || selObj->KindId == CAPSULE) {
        Collision::Debug::DrawCapsuleMtx(0.5f, 0.5f, selWorldFull,
                                      {1.0f, 1.0f, 0.0f, 1.0f});
      } else {
        Collision::Debug::DrawOBB(localAabb, selWorldFull,
                               {1.0f, 1.0f, 0.0f, 1.0f});
      }

      // Gizmo: rotation + translation only (no scale) for fixed thickness
      XMMATRIX selWorldNoScale =
          XMMatrixRotationRollPitchYaw(selObj->Rotation.x, selObj->Rotation.y,
                                       selObj->Rotation.z) *
          XMMatrixTranslationFromVector(XMLoadFloat3(&selObj->Position));

      // Stem lengths scale with object, thickness stays fixed
      float lenX = 0.7f * selObj->Scale.x;
      float lenY = 0.7f * selObj->Scale.y;
      float lenZ = 0.7f * selObj->Scale.z;
      Collision::AABB stemX = {{0.0f, -0.04f, -0.04f}, {lenX, 0.04f, 0.04f}};
      Collision::AABB stemY = {{-0.04f, 0.0f, -0.04f}, {0.04f, lenY, 0.04f}};
      Collision::AABB stemZ = {{-0.04f, -0.04f, 0.0f}, {0.04f, 0.04f, lenZ}};

      Collision::Debug::DrawOBB(stemX, selWorldNoScale, {1.0f, 0.0f, 0.0f, 1.0f});
      Collision::Debug::DrawOBB(stemY, selWorldNoScale, {0.0f, 1.0f, 0.0f, 1.0f});
      Collision::Debug::DrawOBB(stemZ, selWorldNoScale, {0.0f, 0.0f, 1.0f, 1.0f});

      // Center point (fixed size)
      Collision::AABB handleCenter = {{-0.03f, -0.03f, -0.03f}, {0.03f, 0.03f, 0.03f}};
      Collision::Debug::DrawOBB(handleCenter, selWorldNoScale,
                             {1.0f, 1.0f, 1.0f, 1.0f});

      // Cones at tips (fixed size, positioned at stem ends)
      Collision::Debug::DrawConeOBB(XMFLOAT3(lenX, 0, 0),
                                 XMFLOAT3(lenX + 0.3f, 0, 0), 0.12f,
                                 selWorldNoScale, {1.0f, 0.0f, 0.0f, 1.0f});
      Collision::Debug::DrawConeOBB(XMFLOAT3(0, lenY, 0),
                                 XMFLOAT3(0, lenY + 0.3f, 0), 0.12f,
                                 selWorldNoScale, {0.0f, 1.0f, 0.0f, 1.0f});
      Collision::Debug::DrawConeOBB(XMFLOAT3(0, 0, lenZ),
                                 XMFLOAT3(0, 0, lenZ + 0.3f), 0.12f,
                                 selWorldNoScale, {0.0f, 0.0f, 1.0f, 1.0f});

      Direct3D_SetDepthEnable(true);
    }
  }

  // ========= Draw outlines for ALL multi-selected objects (excluding primary)
  // =========
  if (g_CurrentMode == MODE_SELECT && g_SelectedIndices.size() > 1) {
    Direct3D_SetDepthEnable(false);
    for (int idx : g_SelectedIndices) {
      if (idx == g_SelectedIndex)
        continue; // primary already drawn with gizmo
      MapObject *obj = m_map_controller.GetObject(idx);
      if (!obj)
        continue;
      XMMATRIX w = XMMatrixScaling(obj->Scale.x, obj->Scale.y, obj->Scale.z) *
                   XMMatrixRotationRollPitchYaw(
                       obj->Rotation.x, obj->Rotation.y, obj->Rotation.z) *
                   XMMatrixTranslationFromVector(XMLoadFloat3(&obj->Position));
      if (obj->KindId == SPHERE) {
        Collision::Debug::DrawSphereMtx(0.5f, w, {1.0f, 1.0f, 0.0f, 1.0f});
      } else if (obj->KindId == CYLINDER || obj->KindId == CAPSULE) {
        Collision::Debug::DrawCapsuleMtx(0.5f, 0.5f, w, {1.0f, 1.0f, 0.0f, 1.0f});
      } else {
        Collision::AABB localAABB = Cube_GetAABB({0.0f, 0.0f, 0.0f});
        Collision::Debug::DrawOBB(localAABB, w, {1.0f, 1.0f, 0.0f, 1.0f});
      }
    }
    Direct3D_SetDepthEnable(true);
  }

  // ========= Box-Select Rectangle Overlay (ImGui) =========
  if (g_DragBoxActive) {
    Mouse_State ms2{};
    Mouse_GetState(&ms2);
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    dl->AddRectFilled(ImVec2((float)g_BoxStartX, (float)g_BoxStartY),
                      ImVec2((float)ms2.x, (float)ms2.y),
                      IM_COL32(100, 160, 255, 40));
    dl->AddRect(ImVec2((float)g_BoxStartX, (float)g_BoxStartY),
                ImVec2((float)ms2.x, (float)ms2.y),
                IM_COL32(100, 160, 255, 220), 0.0f, 0, 1.5f);
  }
}
