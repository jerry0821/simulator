#pragma once

class MapController;

class MapEditorController
{
public:
  explicit MapEditorController(MapController& map_controller);

  void Update();
  void Draw();

private:
  MapController& m_map_controller;
};
