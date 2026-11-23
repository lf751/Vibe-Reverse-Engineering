# Static replacement geometry ownership regression

Investigation date: 2026-11-03. Runtime source: eea44970e, including upstream dev
5c44db98a. The user reproduced the slowdown with all smoothing removed and states
the same content worked in 9-19, with the regression present in stock 2.0.0.

## Observed failure

Preserved logs/configuration:
`test-artifacts/JPOG/plus2-regression-investigation/2026-11-03_114019_replacement-churn/`.

- GPU fence recovery waits: 1740, 2482 and 4086 ms in the latest run.
- Individual stalled frames issue 2714, 2800 and 1078 acceleration-structure builds.
- Frame876 VRAM demand15203 MiB against15214 MiB budget. DXVK used14433 MiB;
  acceleration structures3249 MiB, material textures169 MiB. Those categories are
  components of the total, not additional allocations.
- The frame900 cumulative sample reports18997 builds and14872 refits over98 frames.
  That sample follows the memory spike; its current geometry/memory counters must
  not be presented as the peak geometry census.
- SceneHistory reports owner handles20000000000000d0 and20000000000000d3 jumping
  50.8016/10.8061 units with extents18.6062/5.22618, both classified posed.
- On-demand shader compilations:0. Startup graphics preset:High. The diagnostics
  therefore do not support shader compilation or replacement texture residency
  as the dominant explanation for these particular stalls.

## Concrete source defect

`src/frontends/d3d9/d3d9_frontend.cpp:969` computes description-level `deforming`
from the original game draw: bones, dynamic source, captured vertices, or an update.
The description stores that value at1035.

`src/dxvk/rtx_render/rtx_retained_scene.cpp:480-513` expands a USD mesh replacement
into new RasterGeometry and marks the submesh part `meshReplacement=true`. During
the direct build, the description-level deformation state and original skinning
state are not specialized to that replacement geometry.

At974-985, the geometry key receives `owner=entry.handle` when the original draw
has bones or the description is deforming. The BLAS is also marked deforming from
the description flag. Consequently a static replacement for a dynamic original
gets private geometry per object instead of sharing its immutable mesh. Original
bone hashes also enter the reuse comparison at1207-1209, and original skinning
affects placed bounds/jump testing. A jump rebuild resets geometry buffers through
SceneManager::onSceneObjectUpdated(dropHistory=true).

This ownership condition is present in the base Remix Plus v2 commit268fb5b24.
The retained-scene implementation does not exist in the tested 9-19 reference
commit717b94bc9. The mismatch does not depend on smooth normals or the custom
terrain material.

## Actual replacement content

The installed mod directory is a symlink to
`C:/Users/User/Desktop/Files/JPOGRTX`. The referenced vegetation assets were
inspected read-only using Toolkit's existing USD libraries. All12 assets have
static points and no authored per-mesh joint indices. For example:

| Asset | Vertices | Triangles |
|---|---:|---:|
| Palm.usd | 91160 | 145032 |
| DatePalm_Med.usd | 107597 | 162731 |
| Salacca_05.usd | 215775 | 289634 |

The full measurements and mesh paths are in the evidence directory's
`vegetation-assets.json`; `audit_vegetation.py` reproduces the read-only audit.

## Proposed correction and validation boundary

Geometry ownership, pose history and deformation must describe the current
replacement mesh, not unconditionally inherit the original draw's deformation.
Static USD replacement submeshes should share their buffers/BLAS. Genuine skinned
replacement geometry and includeOriginal submeshes must retain their required
per-instance pose ownership. Mixed replacement/original parts must restore their
own state independently; clearing a parent description flag globally would be wrong.

This is a demonstrated source-level ownership mismatch, strongly consistent with
the observed memory/build storm. The logs do not yet identify which replacement
asset each failing owner handle represents, or measure the fix's GPU impact.
Do not claim the sole runtime cause or a verified performance fix before a controlled
same-view A/B. Add regression coverage for shared static replacements of dynamic
originals, genuine skinned replacements, and includeOriginal/mixed submeshes, then
compare geometry sharing, builds/refits and peak memory on the same save.

No runtime source, installed configuration, mod asset, save, or game binary was
changed during this investigation. No game/GPU test was launched.

## Candidate implementation and validation

Approved by user on2026-11-03. Candidate branch fix/20261103-static-replacement-sharing,
commitsf9c8c781a andf6fa0d850. SceneDraw now has an explicit replacement geometry binding
that discards the source pose/shader/ephemeral flags only for unweighted replacement
meshes. ExternalDrawState classifies deformation per prepared submesh. The retained
build restores each expanded part from its parent snapshot; original parts and
weighted replacements keep their pose ownership. Parent pose snapshots are constructed
only for expanded parts. The legacy replacement lane uses the same binding contract.
No graphics setting or new runtime option implements the correction.

Runtime release build:PASS. Dedicated _Comp64UnitTest suite:31 passed,0 failed,
including new static replacement, pose hash removal, mixed original/replacement,
weighted replacement, missing pose and explicit API deformation checks.

The isolated copy initially had only terrain-blend-smoke and outdated configuration.
For a representative test it now uses the installed mod through a junction to
C:/Users/User/Desktop/Files/JPOGRTX and matching configuration/weather/save copies.
2373 native data files matched. simjp.exe and TTerrainInterface.dll initially differed;
the originals were backed up and the isolated copies now match all14 checked native
binaries. Diagnostic logging is more frequent only in the isolated copy. Backup paths
are recorded in test-artifacts/JPOG/replacement-sharing/isolated-configuration.json
and fixed-isolated-deployment/deployment.json.

Final isolated runtime SHA256:
E6923F02CD7AC22786B66E9824DB33061000CBC85EF651C2DD20BF04BA3E83F6.
All233 shipping files were hash-verified. The baseline runtime20C376D8... is preserved
as a complete package under test-artifacts/JPOG/replacement-sharing/baseline-runtime.
Installed game integration/configuration hashes were verified unchanged. Neither the
installed runtime nor JPOGRemix trunk has been advanced to this experimental fix.

The isolated game was opened under the user's test approval. Same-save tree-heavy
camera-motion gameplay, memory and rebuild comparison remain pending user interaction.
Do not call the performance regression fixed based on build/unit results alone.

## Gameplay result

The user exercised the same save and replaced-tree view in the isolated candidate
and reported: "seems fixed, the only issue is a seperate flickering which we can
work on later". Final binary log identifies f6fa0d85 and build tag
JPOG-StaticReplacement-20261103, with High/DLSS Auto/Ray Reconstruction unchanged.

Preserved fixed-gameplay logs and summary are under
`test-artifacts/JPOG/replacement-sharing/fixed-gameplay/`:
- Peak driver usage2987 MiB, versus15590 MiB in the earlier failing installed run.
- Peak demand2487 MiB, versus15203 MiB previously.
- Peak DXVK allocated2335 MiB; peak DXVK used1805 MiB.
- GPU-stall warnings0, versus3 previously.
- Geometry-sharing counter reaches32 shared geometry entries.
- Some original geometry still builds/refits and32 scene-history reset lines remain;
  this does not establish that object-identity churn or the separate flicker is fixed.

This is a positive user-controlled same-content/configuration comparison, not an
identical-frame benchmark. The expensive static-replacement ownership regression is
validated by the memory reduction and user observation. Keep the unrelated flicker
as a separate later task, as explicitly requested. The installed game remains unchanged.

After validation, JPOGRemix source was fast-forwarded to f78ace762 (the f6fa0d850 code plus the result documentation). The merged temporary fix branch was removed; the existing build worktree remains detached at that commit so its artifacts are preserved. The tested binary still correctly reports its built-code revision f6fa0d85. Nothing was pushed. The installed runtime remains the unchanged dev baseline20C376D8... .

## Installed deployment — 2026-11-03

At the user's request, the tested fix was deployed to D:/Games/JPOGRTX/JPOG.
All233 shipping files were verified against the package. The installed runtime
hash is E6923F02CD7AC22786B66E9824DB33061000CBC85EF651C2DD20BF04BA3E83F6.
Nine existing configuration/root integration files retain their pre-deployment
hashes. Recovery backup: D:\Unity\Vibe-Reverse-Engineering_JPOG_BetaTools\backups\JPOG\2026-11-03_125944_replacement-sharing-installed.
Manifest: test-artifacts/JPOG/replacement-sharing/installed-deployment/deployment.json.
The installed game was not launched by this deployment. The remaining flicker
is still outside this fix and has not been claimed resolved.
