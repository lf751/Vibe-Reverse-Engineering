# Terrain relief and self-shadowing

The Terrain Blend window exposes inward and outward world-unit depths and height
blackpoint/whitepoint for Grass, Sand, Rock, and Dirt. Master strength can exceed
one. View intersections use 4–128 maximum iterations; light intersections use
4–64. Self-shadow strength ranges from zero to one and can be disabled independently.

The runtime intersects one signed heightfield formed from the four layers. Material
placement stays anchored to the terrain hit, including the Dirt minimum elevation
and live grass exclusion mask. Height remapping affects relief, independently of
the raw height values used for material transitions.

Self-shadow rays use the same blended heightfield and its retained virtual hit.
Real mesh intersections remain unchanged. This does not alter terrain silhouettes,
collision, or the shadows the terrain mesh casts onto other geometry.

The staged `relief-Ground.usda` preserves the current authored inputs and inward
depths (0.2, 0.2, 1, 0.2). It adds outward depth 1 for Rock, selects 64 view iterations,
and enables full-strength self-shadowing with 32 light iterations. Height remap
defaults to the full range. The rock height texture uses almost the full range;
the decoded percentile measurements are in `relief-height-analysis.json`.

Toolkit 3.0.0 explicitly migrates old depth/quality fields when loading and removes
them when writing. Existing old presets keep self-shadowing disabled during
migration. The runtime consumes the new fields. Apply or export a migrated preset
before using it with the updated runtime.

Toolkit validation passed: release build, 24 sequential headless tests, formatting,
full lint, and whitespace checks. All runtime shaders, including 18 optional
lighting variants, and the corrected full release build passed. Logs use the
`relief-` prefix; the initial C++ build log records the corrected static-selector
error, while `relief-runtime-build-final.log` is the successful build.

## Live validation and deployment

The isolated game rendered signed parallax on the synthetic ramp/checker fixture.
Under sunlight crossing the ramps, `relief-shadow-on-crosslight.png` shows dark
recesses; `relief-shadow-off-crosslight.png` retains parallax with brighter recesses.
For the ground region x=1040..1240, y=400..620, mean RGB changes from
(101.5, 95.9, 72.6) with shadows to (131.1, 127.8, 102.1) without. This is a visual
smoke check, not a performance benchmark. Lighting aligned along the ramp ridges
produced much less occlusion, as expected.

The missing-height preset rendered a flat checker surface
(`relief-missingheight-off.png`). The real authored materials rendered with the
live Dirt coverage texture bound (`relief-real-materials.png`). The park save used
for this check does not establish the appearance on the user's mountain island.
There were no terrain-schema or texture-loading errors. The isolated daylight
override was released and the day/night cycle restored after validation.

The complete 191-file runtime package is deployed and SHA-256 verified in both the
isolated copy and `D:\Games\JPOGRTX\JPOG`. The actual Ground.usda uses Rock inward
1/outward 1, master strength 1, 64 view iterations, and full-strength self-shadowing
with 32 light iterations. Other authored blend/material inputs remain as before.
Root integration, configuration, bridge, and saves were retained. Deployment
manifests and timestamped backups are recorded in `relief-deployment-*.json`.
