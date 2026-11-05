# Terrain blend displacement modes

The Toolkit terrain editor exposes Off and Quadtree POM in its
Displacement mode dropdown. Keep Enable terrain displacement checked to use
Quadtree POM. The existing inward/outward strengths, height ranges,
iteration controls, and self-shadow settings remain available.

The exported integer attribute is `remix:terrainBlend:displacementMode`:

| Value | Mode |
| --- | --- |
| 0 | Off |
| 2 | Quadtree POM |

Presets without the attribute default to Quadtree. Legacy value 1 is migrated to
Quadtree on read without changing its other controls or the source USD. Toolkit
writes value 2 on export. Other invalid values are rejected. The runtime maps Off
to disabled displacement before pass selection, so self-shadowing is also off.

The terrain quadtree traverses the maximum-height mipmaps of all active height
textures and triplanar projections. Their bounds are combined with the same
fixed blend weights used by the existing signed height evaluation. It skips
cells above the height bound and descends where an intersection is possible.
At leaf level it solves the bilinear height polynomial, splitting it at height
remap clamp boundaries to test interior peaks. It retains the placement LOD for
matching self-shadow weights and compensates the sampler bias for integer height
mips. Traversal-budget exhaustion does not fabricate a displaced hit.
Use Toolkit-ingested height maps with MAX mip filtering, as required by Remix's
regular quadtree POM. Normal averaged mipmaps do not provide conservative bounds.

Terrain variants, including self-shadow lighting variants, join Remix's existing
startup shader compilation queue and progress screen when the mod preset loads.
They do not wait for the first terrain draw. The runtime shader contains no
raymarched terrain POM traversal; ordinary material displacement is independent.

## Earlier three-mode validation (historical)

Toolkit authoring checks passed: 15 core, 3 widget, and 8 window Kit tests, plus
formatting and full lint. The corrected release build and install passed, and
four dedicated runtime regressions passed: hot reload, smooth normals, embedded
shader decoding, and terrain polynomial intersection. The complete 191-file
runtime package was deployed and hash-verified in the isolated game and
`D:/Games/JPOGRTX/JPOG`. The user loaded the mountain save and confirmed normal
rendering in the corrected isolated build. Live switching through all three
modes completed without shader errors; the isolated preset was left on Quadtree.
The main installation retains its authored preset (older presets default to
Raymarched POM). The desktop release was not replaced. Build and live-test evidence is recorded in
`test-artifacts/JPOG/terrain-blend/pom-modes-*`.

For smoke-fixture comparisons, `set-pom-test-mode.ps1 -Mode 0` or `-Mode 2`
updates the isolated smoke preset and makes a backup first.
