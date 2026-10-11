# Dirt material layer

Dirt replaces the previous grass-exclusion-to-sand controls. The Toolkit exposes
four material slots: Grass, Sand, Rock, and Dirt. Each has its own material inputs
and repeat scale; Dirt repeats use the separate `dirtUnitsPerRepeat` scalar while
the original three retain `unitsPerRepeat` float3.

Dirt reads the game's live grass exclusion texture. After grass/sand/rock placement
and material-height blending, Dirt takes only a portion of the remaining grass.
Sand and rock contributions are preserved. Grass/Dirt height maps refine their own
transition without changing the other layers.

- **Dirt minimum height**: no Dirt contribution at or below this elevation.
- **Dirt height fade width**: positive range above that minimum to reach full coverage.
- **Dirt strength**: zero to one; zero disables Dirt.
- **Dirt border width**: positive mask-filter radius in game world units.
- **Dirt units per repeat**: independent positive texture scale.

Minimum height uses the same vertical axis and Flip elevation direction as the
other terrain layers. JPOG uses inverted Y, so keep Flip elevation enabled when
using physically increasing heights.

Older three-layer presets remain valid with Dirt disabled. The Toolkit explicitly
migrates old coverageSand settings on load, cloning Sand into Dirt and preserving
its repeat scale, then writes only Dirt controls. The runtime has one Dirt path
and does not evaluate the removed coverage-to-sand settings.

The staged migration of the user's Ground.usda retains current sand/grass/rock
materials and settings. Dirt starts as an independent copy of Sand, with strength
0.6, border width 1.2, repeat 10, minimum height 1, and elevation fade width 1.
Assign the desired Dirt textures in the fourth slot. This starting material is
copied rather than inventing an unapproved texture choice.

The mask also excludes shores and other non-grass areas; it does not distinguish
paths from buildings. Dirt is inactive when the current frame does not supply the
grass secondary texture. Existing multiple-island/distant-grass/performance
limitations remain.

Deployment manifests and timestamped backups are beside the existing terrain
blend artifacts. Build and schema tests are separate from in-game visual checks.

## Validation on October 9, 2026

- Runtime release compile and install succeeded, including terrain shader variants.
- Dedicated unit-build hot-reload test passed (1/1).
- Toolkit release build passed; sequential headless core/window/widget checks passed
  14/5/3, including fourth-slot color, tiling, minimum/fade, reload, export, and old
  Sand tiling migration. Full formatting and lint passed.
- Isolated game loaded an existing park save. Magenta Dirt followed live grass
  exclusions around paths and buildings (`dirt-enabled.png`). Raising minimum
  height from -1000 to 1000 removed Dirt (`dirt-height-gated.png`) through USD hot reload.
  Restoring Dirt while enabling the original 0-to-1 flipped elevation blend retained
  sand along the water while Dirt covered eligible grass (`dirt-sand-preserved.png`).
  The isolated test's day/night cycle was temporarily held and restored afterward.
- Complete 191-file runtime packages were deployed and SHA-256 verified in the
  isolated copy and normal `D:\Games\JPOGRTX\JPOG` installation. Root integration
  DLLs, ASI, configuration, bridge, and saves were retained. The normal installation
  received the migrated Ground.usda with independent Dirt copied from Sand.
