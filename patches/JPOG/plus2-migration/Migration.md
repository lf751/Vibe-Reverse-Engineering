# JPOG on Remix Plus 2.0.0

Target: exact `remix-plus-2.0.0`, commit `268fb5b24`, not moving main.
Runtime integration worktree: `D:/Unity/feat-20261030-jpog-terrain`, branch
`feat/20261030-jpog-terrain`. Original runtime branch remains recoverable as
`backup/JPOGRemix-pre-plus2-20261030` at `ab48b452c`.
The original `D:/Unity/dxvk-remix_numos` checkout's `JPOGRemix` branch is now
`f07264ab0`. Feature port: `2093caa68`; history-preserving merge: `43d87ac61`;
build workflow follow-up: `df15cca1a`. The history join leaves the ported tree
unchanged and makes a normal push possible. No force push or public push occurred.
Three colliding ignored local files were backed up before the checkout advanced;
all submodules match their new release pins and the source checkout is clean.

Upstream migration references supplied by the user:
- https://github.com/RemixProjGroup/dxvk-remix/blob/main/docs/RemixApi-Migration.md
- https://github.com/RemixProjGroup/dxvk-remix/blob/main/docs/api-host-path-migration.md
- https://github.com/RemixProjGroup/dxvk-remix/blob/main/documentation/RemixApiChangelog.md

The checked-out release's `AGENTS.md` and workflow skills govern runtime work.
JPOG feeds intercepted D3D9 draws rather than creating its own Remix API scene.
The host-path guide's D3D11 device, retained-instance submission and GPU texture
import calls therefore do not require a new JPOG adapter. The 2.0 D3D9 frontend
handles its retained scene path; terrain smoothing must work in that path too.

## Port contract

Keep the new API-neutral scene backend, D3D9 frontend, NUMOS sky, shader variants,
matched bridge pair, and API 1.0.0 ABI. Port only the custom terrain functionality
and game adapter behaviour; do not restore the old renderer.

- Grass/sand/rock/dirt terrain presets and all existing authored thresholds.
- Off/raymarched/quadtree POM, signed strengths, remapping, and self-shadowing.
- Terrain smoothing, including regular-grid hash collision fixes.
- Live coverage masks for grass exclusion and dirt placement.
- Automatic terrain classification through the existing JPOG proxy.
- Weather reports through `rtx.weather.gameWeather` with preset bindings, replacing
  the removed `__weather.*` route in the game-specific ASI.

Runtime flags cannot use the old opaque material bit positions: all 16 bits are
allocated in 2.0. Terrain/world-opacity tags use spare per-surface flags. Core
visibility needs the world-opacity flag and secondary texture in its side table.
Coverage UV scale is authored configuration, default 1; JPOG config must set
`rtx.legacyMaterial.worldSpaceSecondaryOpacityScale = 0.0009765625`.

Retained geometry must use terrain smoothing as well as the old draw lane; live
coverage observations must survive cached frontend material translation.

## Other repositories

Toolkit integration completed by delegated worker on
`feature/20261030-upstream-toolkit`, merge commit
`aeb65baa5d093d7b2a5f3c270adb72969aa3c07a`. Upstream is `3d5071a6` (71 commits).
Build, formatting, lint, and all 26 terrain Kit tests passed. All 33 terrain
files are unchanged, as are schema 1 and mode values 0/1/2. No push.

Vibe upstream has no new commits (14 local commits ahead). The JPOG ASI and proxy
now build against the 1.0.0 headers via `REMIX_RUNTIME_SOURCE`. Weather reports use
`rtx.weather.gameWeather`; the old weather message route is removed.

## Current implementation

The data model/USD loader, SceneManager/context, per-surface/core flags,
world-space opacity, terrain evaluation and smoothing are ported to the new
backend. The pathtracer and shader entry/binding files preserve the 2.0 variants
(slim/lean/core/sparse/SCCR/SHARC/etc).

Submodules are now pinned to the release. RTXDI needed an explicit origin fetch
and `submodule update --no-fetch` to materialize the pinned commit. SHaRC's
fresh clone needed forced materialization; its source is unmodified.

All source conflicts are resolved. New options have UI controls and the options
reference has been regenerated. The first runtime compile exposed a Windows text
encoding issue in the shader compiler script and obsolete terrain material flags;
both are corrected. Confirmed terrain relief hits carry an explicit marker through
the float4 shared-coordinate payload so failed marches cannot cast relief shadows.

The matched x86 bridge client, x64 server and launcher build successfully. The
bridge wrapper now discovers VS 18 / Build Tools like the runtime wrapper and caps
compile concurrency at four. The JPOG proxy and ASI also build successfully.
Full runtime shader/C++ compilation and dedicated unit checks are complete.
Use runtime `.claude/skills/rtx-build/SKILL.md` (`scripts/build.ps1`, jobs 4), bridge
build skill, and `_Comp64UnitTest`.
Configure both `-Denable_d3d11=true` and `-Denable_dxgi=true`: the exact 2.0.0
release's shared `d3d9.def` exports those frontend entry points unconditionally,
although Meson defaults them off. The first link without those flags failed on
nine unresolved D3D11/DXGI exports. The full-module build enables both frontends.
The runtime wrapper now enables them automatically; manual unit setup uses
the explicit flags documented in the updated unit-test skill.
For the compressed release also pass `-Dcompress_embedded_shaders=true` and
`-Dbuild_tag=JPOG-Plus2-20261030` through the normal wrapper's `-MesonArgs`.

Terrain shader compilation and the full shared-module C++ build have passed.
All 29 unit checks pass: 27 passed in the complete run and the two documentation
checks passed after repairing stale references. This includes hot reload, POM
math, smooth normals, actual surface/core packing, scene-input, SCCR shell/march
and the compression codec. The explicit `unit_tests` build now includes codec
fixtures; an early `--no-rebuild` codec run exposed that missing build dependency.
The options generator now matches runtime formatting without changing any of
the 1558 option names, types, defaults or ranges. Graph goldens were missing the
existing 2.0 GameValueReadBool/Number components; their six files and index rows
are regenerated from the unchanged runtime definitions.

The proxy registers its device wrapper through the new bridge's optional
`BridgeClient_SetOuterDevice` export, retains its D3D9 owner, and clears borrowed
pointers before release. Four cases against the actual proxy DLL and synthetic
chain modules passed, covering export presence/absence and resource-only inner
device lifetimes. See `proxy/COMIdentity.md` for the borrowed-pointer contract.
These tests do not substitute for the real bridge/game check.

All twelve legacy weather presets have converted profiles under `patches/JPOG/weather`.
Four profiles bind the game's five weather IDs; the remaining presets remain
available manually. `weather-migration.json` preserves both mapped values and
legacy values without a supported 2.0 equivalent. Cloud thickness converts from
kilometres to metres according to NUMOS's importer. Fog/precipitation appearance
requires visual retuning; this conversion does not claim identical weather.
`generate_profile.py --config <original-pre-2.0-rtx.conf> --runtime <new-runtime-checkout> --output <staging-dir>`
reproduces the conversion. Legacy defaults are read from Git revision `ab48b452c`,
independently of the old checkout's current branch. The original source config
is in the weather backup. A fresh conversion reproduced all tracked profiles
and the new global config byte for byte.
`config-option-audit.json` records six retained global values with no matching
2.0 option: four removed SHaRC controls, the old Psycho17 contrast control and
moon brightness. They are preserved rather than guessed into controls with
different semantics; the new runtime ignores those keys. Night lighting and
tonemapping therefore also require visual review.

## Validation and delivery

The compressed release build and final incremental check both pass. The final
DLL is 327,315,968 bytes (about 312 MiB), versus 484,896,256 bytes for the raw
test build. All 955 emitted shader arrays independently decode to the final
SPIR-V byte for byte: 468,327,728 raw shader bytes become 310,748,544 embedded
bytes. The DLL contains build tag `JPOG-Plus2-20261030` and commit `df15cca1`,
and does not contain the local runtime source path.

The canonical `terrain-blend/deploy-share.ps1` now accepts explicit target and
runtime source roots. It deploys the complete runtime, matched bridge pair,
rebuilt proxy/ASI and one-time target-owned weather conversion. Existing new
weather profiles remain authored on repeat deployment. Target bridge settings
are retained and packaged DLSSNR is never replaced by another game's DLL.
Build/package freshness, recovery backups and destination hashes are checked.

The isolated game at `test-artifacts/JPOG/terrain-blend/test-game` received 238
verified files. Its 2752 pre-existing files were accounted for; unmapped files
retain their hashes. Twelve profiles are under `rtx-remix/weather`. Backup:
`backups/JPOG/2026-10-30_220550_sharing-release`. Manifest:
`test-artifacts/JPOG/plus2-migration/isolated-deployment/deployment.json`.
The installed game and Desktop sharing folder have not been deployed.

The user approved launching the verified isolated package. The corrected build
passes the same-save scene load and stays running without logged runtime errors.
Off, Raymarched POM and Quadtree POM hot reload remain stable; the original
preset is restored byte for byte. The user confirmed correct terrain appearance,
smooth-normal updates after a height edit, and dirt/grass exclusion after a path
extension. Visual checks are user-confirmed because automated window capture
was unavailable. Weather profiles load and the live mask rebinds; exhaustive
weather-transition and day/night appearance testing was not performed.
No public pushes are authorized.

The first approved scene load crashed in CPU normal generation. See
[SmoothNormalsCrash.md](SmoothNormalsCrash.md) for the dump-backed diagnosis and
the buffer-domain correction in `f07264ab0`. The corrected clean release and a
fresh full 29/29 unit run pass. The isolated package now contains the fix, with
225 runtime/integration files verified and all unmapped files preserved. Native
weather profiles were retained on this repeat deployment. Manifest:
`test-artifacts/JPOG/plus2-migration/isolated-smooth-fix-deployment/deployment.json`.
The original staged package failed its live test and is superseded by this
corrected, tested package. The installed game and Desktop release folder remain
on their prior builds; this migration deploys only the isolated test copy.

Logs, patches, and reports: `test-artifacts/JPOG/plus2-migration/`.
Recovery source snapshots: `backups/JPOG/*plus2*`.
