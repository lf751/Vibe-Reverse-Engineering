# JPOG procedural terrain materials

The authoring extension and custom runtime use one USD terrain preset for all terrain
draws identified by the existing JPOG terrain hook. No generated terrain texture must
be selected in the Toolkit.

## Author a preset

1. Launch `D:\Unity\toolkit-remix\_build\windows-x86_64\release\lightspeed.app.trex.full.bat`.
2. Open **Window > Terrain Blend**.
3. Enter grass, sand/dirt, and rock albedo, normal, roughness, and height textures.
   Use absolute paths to DDS assets already converted by Remix ingestion. Empty
   texture slots use the layer's constants.
4. Set world units per repeat, sand/grass elevation thresholds, rock slope angles,
   and height-map blend strength/width.
5. **Apply to Project** stores an undoable preset in the current editable layer.
   Save the project to persist it. **Export Terrain Mod** creates a new independent
   mod directory containing `mod.usda` and its textures. Export does not require
   an open project.

An exported mod is installed through the normal `rtx-remix/mods` workflow. It needs
the terrain-enabled custom runtime built from `D:\Unity\dxvk-remix_numos`.
The installed JPOG runtime is a separate deployment from source compilation.
Use a matching custom bridge build as well: root `d3d9_remix.dll` and
`.trex/NvRemixBridge.exe` must expose the terrain texture-tagging API.

## Supported blend

- Grass and sand/dirt blend by terrain elevation.
- Rock replaces those layers on slopes.
- Rock placement can use slope, elevation, or both, with independent transition ranges.
- Flip elevation direction supports JPOG's inverted vertical coordinate.
- Material height maps refine transitions without enabling POM displacement.
- World-space triplanar mapping covers horizontal terrain and cliffs.
- Albedo, normals, and roughness blend into an opaque dielectric surface.
- A preset is automatically assigned to tagged terrain across generated islands.
- Mod priority and USD hot reload select and update the preset.

Lengths and elevations are in game world units. The runtime's configured `rtx.zUp`
sets the vertical axis. Toolkit preview does not execute the procedural terrain
shader; assess the blend in the custom runtime.

For JPOG, enable **Flip elevation direction** to make elevation increase toward
physically higher ground. Recalibrate existing negative thresholds in the flipped
coordinate; the option negates the world height for both grass/sand and rock.
Choose **Elevation** rock placement for a third upper layer, **Slope** for cliffs,
or **Both** for high ground and cliffs. Both combines the two soft weights.
Try a broad slope range such as 25–65 degrees and height strength zero while tuning
placement. Existing presets omit these new optional fields and retain their original
unflipped elevation and slope-only rock behavior.

## Validation assets

`smoke/mod.usda` is a texture-free preset with distinctive grass, sand, and rock
colors, intended to isolate automatic assignment before testing imported textures.
Build/test logs are stored alongside this guide. The runtime's complete schema
and integration documentation is `D:\Unity\dxvk-remix_numos\documentation\TerrainBlend.md`.
Toolkit usage is documented in `D:\Unity\toolkit-remix\docs\howto\learning-materials.md`.

## Live grass coverage sand

Grass exclusion sand strength (0 to 1) transfers grass weight to sand using the
game's live coverage alpha. Sand border width controls the filter radius in world
units. Strength zero disables this addition; existing presets default to zero.
The mask includes paths, buildings, shores, and other grass exclusions. It does not
identify why grass was excluded. Existing rock placement retains its weight.
The game's secondary grass texture is discovered through the configured
`rtx.legacyMaterial.worldSpaceSecondaryOpacityTextures` primary hashes and sampled
at world XZ / 1024, matching the existing JPOG grass opacity correction.
No static control texture needs to be exported or manually selected.

Live read-only acquisition confirmed a 1024-square A1R5G5B5 texture. Adding a path
changed 580 alpha texels from white to black in the same texture. The updated runtime
bound this texture in the isolated game; disabling coverage returned building
exclusions to grass, and enabling it restored sand. The user confirmed the effect
appeared to work. The release build, hot-reload unit test, and all 20 Toolkit checks
passed. Both the isolated copy and actual test game received all 191 runtime package
files with SHA-256 verification. The actual Ground.usda uses strength 0.7 and width 2;
its materials and existing elevation/rock settings were preserved.

See `coverage-deployment-manifest.json` for backup and retained-file hashes, and
`coverage-disabled.png` / `coverage-live-final.png` for rendering evidence. Day/night
cycling was held in the isolated test only and restored afterward. Broader testing
of distant grass availability, multiple islands, reflections, large border widths,
and sampling performance remains. Smooth strata/paint/detail-map integration is
separate from this grass exclusion mask and has not been implemented.

## Verified local result

Toolkit build, formatting, lint, and 15 headless Kit checks passed. The final runtime
release build and dedicated hot-reload unit test passed. `test-game/` contains an
isolated runnable game copy with the complete runtime and matching custom bridge.
Those smoke tests used the isolated copy and preserved the original installation.

The live smoke test verified automatic tagging, constant-color terrain replacement,
USD color hot reload, and disabling the preset to restore vanilla ground. See
`game-cyan-reload.png`, `game-preset-disabled.png`, and the test game's logs.
The normal smoke preset has been restored. Imported maps, cliffs, multiple islands,
and performance still need visual validation with the intended terrain assets.

The placement revision passed 19 Kit checks, the full runtime build, and the
dedicated hot-reload unit test. Live checks verified elevation reversal, elevation
rock on flat terrain, both-mode rock, and broader combined transitions. The test
copy's day/night cycle was temporarily held for inspection and then restored.
The updated complete runtime is deployed in `D:\Games\JPOGRTX\JPOG` with all 191
package files hash-verified. Its integration, settings, terrain preset, and saves
were preserved. Deployment details are in `placement-deployment-manifest.json`.
