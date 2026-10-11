# Terrain POM displacement

The Terrain Blend plugin adds a master displacement toggle, strength, and quality,
plus independent Grass, Sand, Rock, and Dirt depths. It uses the existing height
texture on each layer. White is the top of the apparent surface and black is
recessed by the selected depth. Depth is measured in game world units, independently
of texture repeat scale. A missing height texture or depth zero leaves a layer
unchanged. The master strength ranges from zero to one.

Quality selects maximum 8/16/32 raymarch steps (Low/Medium/High). Viewing angle
controls the actual count, with at least four steps. Explicit gradient sampling
filters height with distance; interpolated intersections reduce stepping, and the
effect fades at extreme grazing angles. Placement height samples remain anchored
to the original surface, preserving the authored blends and Dirt exclusion gate.

This is apparent texture depth. It does not change the terrain mesh, silhouette,
ray-hit positions, collision, or height-field self-shadows. Normal maps are sampled
at the parallax coordinates. Performance depends on active layers and projections;
the quality selector provides a bounded sampling budget, not a frame-rate guarantee.

Optional schema-one fields are `displacementEnabled` (bool, default false),
`displacementStrength` (float, default 1), `displacementQuality` (token, default
medium), and `displacementDepths` (float4, default 0.1 for all four layers). All
depths must be finite and nonnegative. The existing three-layer repeat attribute
and Dirt attributes are unchanged. Older presets keep displacement disabled.

Terrain POM has dedicated compile-time variants. The runtime selects them only
for an active terrain preset with displacement enabled and positive strength.
Ordinary material displacement settings are independent. Shader variants cover
primary and indirect paths, ray query and ray trace, NRC, WBOIT, NEE Cache, and SHARC.

Toolkit release build, formatting, lint, and 23 sequential headless tests passed.
The actual UI workflow edits all four depths, enable, strength, and quality, rejects
invalid strength, and preserves settings through project load and portable export.
The runtime release build passed including all 280 new POM variants. Logs are
`pom-toolkit-*.log`, `pom-runtime-build.log`, and `pom-runtime-install.log`.

`pom-smoke/` contains a generated checker albedo and repeated ramp height DDS pair
for controlled on/off rendering validation. Its exaggerated Grass depth of 2 is
for validation only. The staged game preset uses conservative depths
(0.05, 0.1, 0.2, 0.1) and Medium quality, preserving current authored terrain inputs.

## Live validation and deployment

The isolated park save rendered the synthetic height/checker pair with displacement
off (`pom-off.png`) and on (`pom-on.png`). The on view shows the ramp relief.
The paused fixed-view captures `pom-on-paused.png` and `pom-strength-zero.png`
show the same terrain reverting to flat sampling when master strength is zero.
Low quality rendered via hot reload (`pom-low.png`); its reduced sampling budget
shows additional aliasing with the exaggerated depth-2 discontinuous test ramp.
Enabled POM with all depths zero rendered flat (`pom-depth-zero.png`).

The actual four authored materials and conservative depths also rendered in the
isolated copy (`pom-real-materials.png`), preserving grass, sand, and Dirt placement.
No invalid-terrain or texture-loading errors were found in its current runtime log.
This is functional rendering validation, not a performance benchmark or exhaustive
steep-cliff/grazing/multiple-island assessment. Height-field self-shadowing and mesh
displacement are outside this implementation.

The full 191-file release package is deployed and SHA-256 verified in both the
isolated copy and `D:\Games\JPOGRTX\JPOG`. The actual Ground.usda has POM enabled,
strength 1, Medium quality, and depths (0.05, 0.1, 0.2, 0.1). Its previous blend
parameters, Dirt settings, and authored textures are preserved. Root proxy/ASI,
configuration, bridge, and saves were retained. See `pom-deployment-JPOG.json` and
`pom-deployment-test-game.json` for file hashes and backup locations.

The isolated copy's day/night cycle was temporarily held and restored afterward.
The preview copy remains running with the actual materials; close it and launch the
normal installation to work against the deployed Ground.usda directly.
