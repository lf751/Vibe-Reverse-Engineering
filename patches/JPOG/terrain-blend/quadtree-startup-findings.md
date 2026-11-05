# Quadtree-only terrain and startup shader cache

Validated 2026-11-05 in the isolated game. Installed game and shared mod assets were not modified.

## Result

Terrain Blend retains optional displacement, with Quadtree POM as its only traversal algorithm. Off stays wire value 0 and Quadtree stays 2. Runtime and Toolkit read old value 1 as Quadtree; missing modes default to Quadtree. Import preserves authored depths, blending, shadow and normal controls. Toolkit exports value 2 and no longer offers raymarched authoring.

The old implementation selected shader variants from frame arguments populated by the first terrain geometry. Initial shader prewarming runs before asynchronous mod loading, so those arguments initially disabled terrain. Loading a preset did not change the option generation counter and did not request another prewarm pass. Some G-buffer helpers also explicitly requested non-terrain kernels, and terrain self-shadow variants were missing from RTXDI/ReSTIR GI prewarming.

Shader selection now derives its terrain feature set from the loaded preset without requiring bound geometry or textures. The initializer detects terrain feature changes as well as option changes and queues the corresponding variants. Startup completion observes asset publication before refreshing those requirements. All requests use the existing pipeline compilation queue, cache and wait screen.

## Size measurements

| Artifact | Before | After | Saving |
| --- | ---: | ---: | ---: |
| Terrain SPIR-V (436 shaders) | 258593168 bytes | 252960952 bytes | 5632216 bytes / 5.37 MiB / 2.18% |
| Runtime DLL, compressed shaders | 335324160 bytes | 331246080 bytes | 4078080 bytes / 3.89 MiB / 1.22% |

No variant files were added or removed: both traversal algorithms previously shared each terrain variant. This removes traversal code inside those shaders. Ordinary Remix displacement algorithms remain available.

## Validation

- Release runtime build and installation passed. Build tag: `JPOG-TerrainQuadtree-20261105`.
- Runtime SHA256: `6C92E1AE47073641FAEA1F2341456D3A2FCD0F1F4E15FDEF5171EE3752D129C6`.
- Runtime source commit: `6ff4da3e973b1d741bb3abf0022fe6f3a12d8cc1`, merged into local `JPOGRemix`.
- Dedicated runtime suite: 31 passed, 0 failed, including terrain polynomial intersections, surface packing, preset reload and shader-feature availability before geometry.
- Toolkit: core 16, widget 3, window 8 tests passed; formatting and full lint passed. Wire mapping, legacy import and portable export are exercised through Kit.
- Toolkit commit: `e2644a5daf6f7c5e87a588bbca28d19dd07a1a1b`.
- Runtime shader binaries were reused in the dedicated test build after verifying equivalent compiler options. All 974 SPIR-V hashes matched; raw test headers were regenerated using the repository's encoder, retaining the test build's raw-header configuration. Dedicated C++ test binaries were rebuilt and no tests were skipped.
- All 233 shipping/integration files verified in the isolated deployment, followed by restoration of its local patched `nvngx_dlssnr.dll` (SHA256 `CDFA6C1EC784867182164C7836F324F087B5C9F374D0C9FE46F54999FEABF3ED`).
- Seven selected installed integration/configuration files verified unchanged.

## Live cold-local-cache run

The test used a new `DXVK_STATE_CACHE_PATH` directory. Global driver caches were retained. Runtime unit compilation overlapped this run, so its timing is not a normal-load benchmark.

- 23:35:17.583: 127 initial pipelines queued, local cache missing.
- 23:35:17.782: existing terrain mode 1 read as Quadtree, without modifying the source USD.
- 23:35:20.293: 40 terrain pipelines queued before the first 3D frame; wait screen shown and input held.
- 23:37:56.817: startup finished, 167 pipelines total; 103 cache hits, 64 compilations; wait screen closed.
- 23:38:10.816: first 3D frame; live grass exclusion bound for Dirt.
- 23:38:20.829: first ten seconds of 3D: 0 on-demand compiles, 0 background compiles, 0 ms of frame waits for pipelines.
- User response to the screen/terrain check: “Seems like it's all fine”.

Native computer-use capture was unavailable (native pipe missing), so UI activation/closure logs and the user's observation provide the UI evidence. Existing missing vegetation DDS warnings remained; no terrain shader compilation errors were observed.

The isolated game was left open. No installed-game or desktop-release deployment was performed for this change.

## Recovery and evidence

- Runtime source backup: `backups/JPOG/2026-11-05_230310_terrain-quadtree-startup`.
- Toolkit source backup: `backups/JPOG/2026-11-05_230235_quadtree-toolkit`.
- Isolated runtime recovery: `backups/JPOG/2026-11-05_233355_terrain-quadtree-isolated-runtime`.
- Isolated configuration/DLL recovery: `backups/JPOG/2026-11-05_233355_terrain-quadtree-isolated-local`.
- Logs, size comparison, deployment hashes and validation JSON: `test-artifacts/JPOG/terrain-quadtree-startup/`.
- Toolkit test logs: `test-artifacts/JPOG/terrain-blend/quadtree-only-toolkit/`.

Earlier POM findings remain in `POM.md` and `TerrainPomModes.md`, explicitly marked as historical where superseded.
