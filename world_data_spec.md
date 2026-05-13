# World Data Spec

## Goal

Make the simulator follow a centralized world-data pipeline:

1. Compute generates or updates world-state fields.
2. Application publishes them through shared frame/state structures.
3. Terrain, grass, water, and later gameplay systems read the same data.

This matches the architectural direction of `AfterglowRender-main`, while keeping the simulator's current systems and names.

## Design Rule

`world data` is not a bag of global parameters.

It is a set of world-aligned fields where each texel/cell represents the state of one world position.

Examples:

- How high is this place?
- How steep is this place?
- How wet is this place?
- Is there standing water here?
- Is this area suitable for vegetation?
- How likely is grass to appear here?
- How strong is the wind here?

## Three-Layer Model

To keep the simulator converging toward an `Afterglow`-style layout without deleting useful modules too early, organize world data into three layers:

### 1. Raw Simulation Layer

These fields store source-of-truth physical or environmental state.

- `TerrainHeight`
- `SurfaceWater`
- `SurfaceWaterFlow`
- `SurfaceWaterFlowPreview`
- `WaterMask`
- `SoilMoisture`
- `ErosionDelta`
- `RainMap`
- `WindField`
- `ClimateField`
- `MeteorographField`

These should stay even if later surface or rendering interpretation changes.

### 2. Core Surface Layer

These fields are the shared interpretation layer that other systems should read instead of rebuilding terrain or water meaning ad hoc.

- `TerrainNormal`
- `WaterInteractionData`
- `TerrainSurfaceData`
- `TerrainVegetationSuitability`

This is the layer that should gradually become more stable and schema-driven.

`TerrainSurfaceData` is the current core terrain-surface field in this project.

### 3. Consumer-Derived Layer

These fields are derived for specific downstream systems and should remain lightweight consumer-facing outputs.

- `GrassData`

Future examples could include:

- traversal suitability
- footstep surface type
- AI movement cost
- shoreline foam mask

These fields are valid to keep as long as they are derived from the central layers rather than becoming new competing sources of truth.

## Retention Rule

When a future core surface field is introduced, existing modules do **not** need to disappear automatically.

Use this rule:

- keep raw simulation fields as source data
- keep core surface fields as shared interpretation
- keep consumer-derived fields when they save repeated work for multiple systems
- remove duplicate local logic and legacy fallback paths first

This means:

- `GrassData` should usually stay
- `WaterInteractionData` can also stay if it remains the shared water-interpretation layer
- old shader-side re-derivations are the first things that should disappear

## Current Core World Data

### 1. `TerrainHeight`

- Meaning: final terrain height after base terrain and erosion/deposition adjustments.
- Type: texture field.
- Current producer:
  [compute_final_terrain_height_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_final_terrain_height_texture.cpp)
- Current consumers:
  terrain render, water simulation, classification, wind, CPU terrain sampling.

### 2. `TerrainNormal`

- Meaning: world-space terrain normal and slope helper derived from final terrain height.
- Type: texture field.
- Current producer:
  [compute_terrain_normal_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_terrain_normal_texture.cpp)
- Current consumers:
  terrain vertex shader, terrain surface generation, grass distribution.

### 3. `TerrainSurfaceData`

- Meaning: shared terrain-surface interpretation, aligned with an Afterglow-style core surface field.
- Stored channels:
  `R = slope`
  `G = beach / shoreline`
  `B = humidity`
  `A = roughness`
- Type: texture field.
- Current producer:
  [compute_terrain_classification_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_terrain_classification_texture.cpp)
- Current consumers:
  terrain shader, grass-data generation, debug preview.

### 4. `TerrainVegetationSuitability`

- Meaning: broad vegetation suitability derived from terrain/ecology support.
- Type: texture field.
- Current producer:
  [compute_terrain_classification_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_terrain_classification_texture.cpp)
- Current consumers:
  grass-data generation.

### 5. `GrassData`

- Meaning: dedicated grass distribution field for instancing, separate from terrain shading.
- Stored channels:
  `R = grass possibility`
  `G = stable distribution hash`
  `B = grass scaling hint`
  `A = reserved`
- Type: texture field.
- Current producer:
  [compute_grass_data_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_grass_data_texture.cpp)
- Current consumers:
  grass seed/coverage compute, shared frame state, future debug/visualization.
- Notes:
  This is the main step toward the Afterglow-style split:
  terrain/ecology fields first
  vegetation distribution field second
  instancing consumes the distribution field instead of recomputing terrain rules.

### 6. `RainMap`

- Meaning: rainfall amount and rain hints derived from climate and wind.
- Type: texture field.
- Current producer:
  [compute_rain_map_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_rain_map_texture.cpp)
- Current consumers:
  surface water, soil moisture.

### 7. `SurfaceWater`

- Meaning: raw hydrology state on terrain cells.
- Stored channels:
  `R = water amount`
  `G = glow/preview helper`
  `B = standing water`
  `A = preview alpha/helper`
- Type: texture field.
- Current producer:
  [compute_surface_water_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_surface_water_texture.cpp)
- Current consumers:
  visible water, water surface height, water render, soil moisture, erosion.

### 8. `SurfaceWaterFlow`

- Meaning: directional outflow / flow distribution for each terrain cell.
- Type: texture field.
- Current producer:
  [compute_surface_water_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_surface_water_texture.cpp)
- Current consumers:
  visible water, erosion, water render preview.

### 9. `SurfaceWaterFlowPreview`

- Meaning: debug/render helper for water-flow preview.
- Type: texture field.
- Current producer:
  [compute_surface_water_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_surface_water_texture.cpp)
- Current consumers:
  water render, debug UI.

### 10. `VisibleWater`

- Meaning: render-oriented split between pooled water and runoff-like thin water.
- Stored channels:
  `R = visible water`
  `G = runoff hint`
  `B = pooled water preview`
  `A = preview alpha`
- Type: texture field.
- Current producer:
  [compute_visible_water_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_visible_water_texture.cpp)
- Current consumers:
  water mask, post-process, debug UI.

### 11. `WaterMask`

- Meaning: immediate wet / water-contact mask at the surface.
- Stored channels:
  `R = water contact`
  `G = shoreline band`
  `B = pooled-water support`
  `A = render coverage`
- Type: texture field.
- Current producer:
  [compute_water_mask_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_water_mask_texture.cpp)
- Current consumers:
  soil moisture, terrain surface generation, water render overlays.
- Notes:
  `WaterMask` is closer to hydrology output than ecology output.

### 12. `WaterInteractionData`

- Meaning: consumer-facing water/surface interaction field derived from hydrology state.
- Stored channels:
  `R = surface interaction`
  `G = shoreline influence`
  `B = pooled-water interaction`
  `A = retained wetness`
- Type: texture field.
- Current producer:
  `compute_water_interaction_texture.cpp`
- Current consumers:
  terrain surface generation today, future water/terrain/gameplay interpretation.
- Notes:
  This is the water-side equivalent of `GrassData`:
  hydrology fields first
  consumer-facing interpretation field second
  terrain/gameplay systems consume the interpretation field instead of recomputing from raw water state.

### 13. `SoilMoisture`

- Meaning: retained ground wetness after rain, seepage, and standing water.
- Type: texture field.
- Current producer:
  [compute_soil_moisture_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_soil_moisture_texture.cpp)
- Current consumers:
  terrain surface generation, future gameplay movement penalties, vegetation logic.

### 14. `ErosionDelta`

- Meaning: erosion / deposition accumulation and transport energy.
- Stored channels:
  `R = erosion accumulation`
  `G = deposition accumulation`
  `B = signed preview`
  `A = transport energy`
- Type: texture field.
- Current producer:
  [compute_erosion_delta_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_erosion_delta_texture.cpp)
- Current consumers:
  final terrain height, terrain surface generation, future terrain gameplay feedback.

### 15. `WindField`

- Meaning: local wind direction and strength.
- Type: texture field.
- Current producer:
  [compute_wind_field_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_wind_field_texture.cpp)
- Current consumers:
  grass, water, floating light points, UI preview, meteorograph.

### 16. `ClimateField`

- Meaning: broad climate state used as a shared environment layer.
- Type: texture field.
- Current producer:
  [compute_climate_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_climate_texture.cpp)
- Current consumers:
  rain map, terrain surface generation, terrain shader, meteorograph.

### 17. `MeteorographField`

- Meaning: localized weather/environment interpretation layer derived from climate and wind.
- Type: texture field.
- Current producer:
  [compute_meteorograph_texture.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_meteorograph_texture.cpp)
- Current consumers:
  debug UI, future gameplay/environment interactions.

## Central Publishing Path

Current publication path:

1. Compute tasks update textures in [application.cpp](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/app/application.cpp).
2. Shared world data is assembled in [terrain_water_state.h](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/scene/terrain_water_state.h).
3. Data is published into [compute_shared_resource_registry.h](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/compute/compute_shared_resource_registry.h).
4. Frame consumers read it through [render_frame_context.h](/C:/Users/a3877/OneDrive/桌面/就職活動/simulator/src/render/render_frame_context.h).

This is already the right central path and should stay the main route.

## Required Consumer Rule

Systems should prefer reading published world data instead of recalculating from scratch.

### Good

- Terrain shader reads `TerrainHeight`, `TerrainNormal`, `TerrainSurfaceData`.
- Grass placement reads `TerrainHeight`, `GrassData`, `WindField`.
- Water render reads `SurfaceWater`, `WaterMask`, `SurfaceWaterFlow`, `WindField`.
- Character movement reads `TerrainNormal`, `WaterMask`, `SoilMoisture`, `VisibleWater`.

### Avoid

- Terrain shader recalculating normals from height when a central normal field exists.
- Grass logic estimating slope separately from a different rule than terrain surface data.
- Grass instancing treating `TerrainSurfaceData` as the final instancing output instead of reading `GrassData`.
- Gameplay using ad-hoc terrain checks that disagree with render/simulation data.

## Surface Schema

Use these meanings consistently:

- `TerrainHeight`: final terrain elevation field.
- `TerrainNormal`: world normal field, `A/W` can carry `normal_y` if useful.
- `WaterMask`: immediate water-contact / visibly wet surface mask.
  `R = water contact`
  `G = shoreline band`
  `B = pooled-water support`
  `A = render coverage`
- `WaterInteractionData`: consumer-facing water interpretation field.
  `R = surface interaction`
  `G = shoreline influence`
  `B = pooled-water interaction`
  `A = retained wetness`
- `SoilMoisture`: retained ground wetness over time.
- `ErosionDelta`: terrain change tendency / erosion energy.
- `TerrainSurfaceData`:
  `R = slope`
  `G = beach / shoreline`
  `B = humidity`
  `A = roughness`
- `TerrainVegetationSuitability`: broad vegetation support field.
- `GrassData`:
  `R = grass possibility`
  `G = stable hash / density variation`
  `B = grass scaling hint`
  `A = reserved`

Do not overload one field with too many unrelated meanings once gameplay starts depending on it.

## Responsibility Split

To avoid systems drifting apart again, use this split:

- `WaterMask` answers:
  Is this spot currently wet or touched by visible/surface water?
- `WaterInteractionData` answers:
  Given current hydrology and retained moisture, how should other systems interpret water at this spot?
- `SoilMoisture` answers:
  How much water has the ground retained over time?
- `TerrainSurfaceData` answers:
  How should this place be interpreted as a shared terrain surface for rendering, vegetation, and future gameplay?
- `TerrainVegetationSuitability` answers:
  Is this spot broadly suitable for vegetation growth?
- `GrassData` answers:
  Given all of the above, how likely is grass to appear here, how should it vary, and how big should it be?

This keeps:

- hydrology fields as physics-like state
- water interaction data as consumer-facing hydrology interpretation
- classification fields as terrain/ecology interpretation
- grass data as vegetation distribution output

## Current Gap vs Afterglow

Compared with `AfterglowRender-main`, the simulator still needs:

1. More systems to read the same central data instead of partially recomputing.
2. Character/gameplay systems to consume world data.
3. Clearer separation between:
   physics-like fields
   presentation/material fields
   gameplay interpretation fields
4. More consumers to use `GrassData` directly instead of relying on older intermediate assumptions.
5. More consumers to read `WaterInteractionData` directly instead of rebuilding water interpretation from `VisibleWater`, `WaterMask`, and `SoilMoisture` ad hoc.

## Next Recommended Steps

### Step 2

Finish the grass-data transition:

- let grass instancing fully consume `GrassData`
- move grass scale variation to read `GrassData.B`
- keep `TerrainSurfaceData` as source surface data, not final instancing output

### Step 3

Make gameplay read world data:

- movement speed penalty from `WaterMask` / `VisibleWater`
- slope limit from `TerrainNormal`
- traversal hazard from `SurfaceWater` depth

### Step 4

Let grass, terrain, water, and character all share the same slope/wetness interpretation:

- remove duplicate slope logic where possible
- centralize cliff / wet / vegetation thresholds
- avoid hydrology and ecology fields silently overlapping in meaning

### Step 5

Keep `TerrainSurfaceData` stable and treat it as the core terrain surface field:

- `R = slope`
- `G = shoreline/beach`
- `B = humidity/wetness`
- `A = roughness`

This is the main bridge toward Afterglow's layout. Build on it instead of reintroducing ad-hoc terrain interpretation in consumers.

## Summary

The simulator should not be rebuilt from zero.

Instead, continue converging toward:

`compute generates world data -> application publishes world data -> render/gameplay systems read shared world data`

That is the target architecture.
