## Particles rework: TSpriteShader FFP, true vertex colors, flare effect lights & water normal ripples — 2026-11-26

### Root Causes Diagnosed
1. **Lost vertex colors unless tagged `rtx.uiTextures`:** `TSpriteShaderD3D.dll` draws all 3D particles (`425128AF7045BE2A` flares, `9CFB0CC8D7894296` rain, `A030C96CE856579C` water rings, `F03FD5E9279C71FE` water splashes/fountains, `AB056E8048DB8091` dust/smoke/sparks) using D3D8 `SHADERDECL` (`v0=FLOAT3, v1=D3DCOLOR, v2=FLOAT2`, stride 24) and `TSpriteShaderD3D.vsh`. `d3d8to9` maps D3D8 reg 1/2 to `BLENDWEIGHT`/`BLENDINDICES` instead of `COLOR0`/`TEXCOORD0`, so Remix never received `color0Buffer`.
2. **Broken additive override in proxy:** `d3d9_device.c`'s old `PARTICLE_VERTEX_SHADER_HASH` workaround only fired for `rtx.particleTextures` and forced `D3DRS_DESTBLEND = 2` (`D3DBLEND_ONE` additive), breaking translucent particles (`AB056E8048DB8091`, `F03FD5E9279C71FE`, `A030C96CE856579C`, `9CFB0CC8D7894296`).
3. **Baked-lighting RGB normalization on particles:** `rtx_instance_manager.cpp` left `isVertexColorBakedLighting = true` on `InstanceCategories::Particle`, causing `opaque_surface_material_interaction.slangh` to divide vertex RGB by `max(r,g,b)` and wash out brown dust / dark smoke / rain tints.
4. **Shared-material `RtxParticleSystem` collisions:** `fetchParticleSystem` hashed only `getHash()` (texture hash), merging additive sparks and translucent dust sharing `smoke2.dds` (`AB056E8048DB8091`); without `color0Buffer`, emitter vertex colors were also unavailable to `particle_system_spawn.comp.slang`.

### Changes Implemented (deployed to `test-game` only)
- **`patches/JPOG/asi/jpog_view_fix.c`:** Hooked `TSpriteShaderHAL::Flush` (`+0x2660`) and `Render` (`+0x2A30`) in `TSpriteShaderD3D.dll`. On the main 3D camera pass (`SyncShaderMainCameraContext == 1`), switches to the shader's built-in `FVF 0x142` (`XYZ | DIFFUSE | TEX1`) FFP path with `g_mvWorldTransformActive = 8` (MV→World decomposition without static-world snapping). On 2D UI contexts, restores the original VS so blocked identity `D3DTS_VIEW` never transforms screen-space HUD.
- **`patches/JPOG/proxy/d3d9_device.c`:** Removed the obsolete `PARTICLE_VERTEX_SHADER_HASH` / `particleDeclaration` / forced `D3DRS_DESTBLEND = 2` workaround. Updated `proxy/tests/lifecycle_test.c` (passes).
- **`D:\Unity\dxvk-remix_numos`:**
  - Disabled `isVertexColorBakedLighting` normalization for `alphaState.isParticle` (`rtx_instance_manager.cpp`).
  - Added tunable NUMOS sky-ambient for non-emissive particles (`rtx.particles.skyAmbientScale`, default `0.35`, in `resolve.slangh` + `rtx_particle_system.h/.cpp` + `rtx_context.cpp`).
  - Mixed legacy blend state into `RtxParticleSystemManager::fetchParticleSystem` hash so shared textures with different blend modes stay distinct while inheriting emitter `color0Buffer` vertex colors.
  - Upgraded `rtx.lightConverter` (`SceneManager::createEffectLight`) with dev menu toggles under **Lighting → Effect Light**: `rtx.effectLightEnable`, `rtx.effectLightPerQuad` (per-quad lights for batched flare draws), and `rtx.effectLightUseVertexColor` (per-flare vertex color + alpha tinting).
  - Added `rtx.waterRippleTextures` (`0xA030C96CE856579C`), `WaterRippleArgs` in `raytrace_args.h`, `SceneManager::collectWaterRipples` (wired into both dynamic and `RetainedScene` `kServiceWaterRipple`), and concentric wave normal perturbation in `translucent_surface_material_interaction.slangh` on `isAnimatedWater` surfaces, with dev menu controls under **Translucent Material → Animated Water**.
- **`test-game/rtx.conf`:** Removed `425128AF7045BE2A`, `9CFB0CC8D7894296`, `F03FD5E9279C71FE` from `rtx.uiTextures`; tagged them (+ `AB056E8048DB8091`) in `rtx.particleTextures`; tagged `425128AF7045BE2A` in `rtx.lightConverter`; tagged `A030C96CE856579C` in `rtx.waterRippleTextures`.
- **Backup:** `backups/JPOG/2026-11-26_1115_particles-rework/`.
- **Test-game deployed SHA-256s:**
  - `d3d9.dll`: `F442C48A67CCC937EF6D7475613987CA6FE49857F36A9708DBDFB6AB9782C749`
  - `jpog_view_fix.asi`: `9CE435E51CB71A7D4242B41C932052A1BF2D929A7689E1768AF7E714D8DF41A8`
  - `.trex\d3d9.dll`: `5A69C58276917045F4918D97EC912DE3B1DB9D55FC0891CCF79DFDA6F01CF33D`

## Tree-swap fix, smooth wind sway, preview UI, water freeze, OMM fix & rollout — 2026-11-25

### Deployment Targets & Rules
1. **Dev test game:** `D:\Unity\Vibe-Reverse-Engineering_JPOG_BetaTools\test-artifacts\JPOG\terrain-mask-rework\test-game\` (live `mod.usda`)
2. **Main game:** `D:\Games\JPOGRTX\JPOG\` (live `mod.usda` symlinked to `C:\Users\User\Desktop\Files\JPOGRTX`)
3. **Release package:** `C:\Users\User\Desktop\JPOGMOD\` (flattened `rtx-remix\mods\JPOGRTX\mod.usd` + `mod.pkg`)
4. **Isolated test game:** `D:\Games\JPOGIsolated\` (flattened `rtx-remix\mods\JPOGRTX\mod.usd` + `mod.pkg`)

- **Do NOT copy or modify `rtx.conf`, `user.conf`, or `Data\Constant.ini`** across targets — the user manages those manually.
- **Leave `Grass.usda` alone** (the user explicitly prefers leaving the grass setup as-is despite the 27-bit primitive-count log warning).
- **Verified deployed artifact SHA-256s (all 4 targets in sync):**
  - `d3d9.dll`: `02691BC0DF034B141E9E47CD4427FE2E8E9E26C012EBA279581B5F5269915506`
  - `jpog_view_fix.asi`: `4B7A7E5430D66E95278D80373D07377A9DDFD3F101B7CC239DBE0C1F5A532430`
  - `.trex\d3d9.dll`: `DFFBE7F0B279C9CED2E0F0E8311B3DD697EFC748E72ACEDE8F8B801EE5B52942`
  - `mod.usd` (`JPOGMOD` & `JPOGIsolated`): `5C8D3822E85D1828E3715EFDD6E9E64282FD1F715EA86016B48047FB21FDF588`

### Proxy & ASI Changes (`patches/JPOG/proxy/d3d9_device.c`, `patches/JPOG/asi/jpog_view_fix.c`)
- **`TSkinShaderD3D` FFP + 1-bone static VB + expanded dynamic VB:** Patched `TSkinShaderHAL::IsCapableHighEndSkinning` (`+0x2120`) and `IsCapableScattering` (`+0x2190`) to return `0`, hooked `Flush` (`+0x1B60`) and `Render` (`+0x2570`), cleared `enableRenderEnvMap` (`+0x6C`), and expanded the dynamic CPU-skinning VB creation size (`+0x334B`) from 96,000 B (2,400 verts) to 2.5 MiB (65,536 verts) so large multi-bone meshes cannot overflow Remix's shadow buffer. 1-bone meshes keep their native static VB; the proxy binds a POS/NORM/UV `skinDeclaration` on `FVF 0x1D2` (stride 40) to omit blend weights/indices at `+24/+28` so they are never read as vertex colors.
- **Terrain cache pool expansion (`TTerrainInterface.dll`):** Patched `TTerrainInterface::Create`'s 5 cache caps (`+0x8A11`, `+0x8A16`, `+0x8A1B`, `+0x8A2A`, `+0x8A34`: 400/400/400/350/900 → 4096) synchronously in `DllMain` so extended `TerrainDrawDistance` values do not exhaust terrain tile caches.
- **Low-end (reflection-off) water flipbook freeze (`TTerrainShaderD3D.dll`):** `TTerrainShaderHAL::RenderWater` (`0x10007AF0`) cycled a 32-frame flipbook via `[0x1014C2C4]` (`t_water0000_1555.dds`..`t_water0031_1555.dds`); frame 2 (`t_water0002_1555.dds`) exact-matches `0xEFEC3F35D7660711`. Patched `RVA 0x8A32` (`mov edx, [0x1014C2C4]` → `mov edx, 2; nop`) so low-end water keeps its translucent Remix override bound every frame while `rtx.animatedWaterTextures` drives wave animation. High-end reflection water is untouched.
- **Building 3D preview UI fix (`g_mvWorldTransformActive` bitflags):** `SyncShaderMainCameraContext` returns `0` for auxiliary `TRenderContext`s (e.g. building preview UI) and mirrored reflection passes (`det <= 0.5`). Clearing `g_mvWorldTransformActive = 0` broke auxiliary contexts because `D3DTS_VIEW` stays bound to the main camera globally, flinging preview meshes off-screen and skipping `skinDeclaration`. Split `g_mvWorldTransformActive` into bitflags:
  - Bit 0 (`1`): `TSysShaderD3D` — always decomposes `World = MV * inv(View)`
  - Bit 1 (`2`): `TSkinShaderD3D` — always decomposes `World = MV * inv(View)` and binds `skinDeclaration`
  - Bit 2 (`4`): main camera pass only — enables `stabilize_static_world_matrix`
- **World-matrix stabilization & smooth idle tree wind sway (`d3d9_device.c`):** `stabilize_static_world_matrix` snaps `MV * inv(View)` float32 round-trip jitter (`POS_EPS = 0.005f`) back to each object's canonical world matrix so Remix's exact-position matcher hits. Tightened `STATIC_WORLD_ROT_EPS` from `0.002f` (which stair-stepped gentle idle tree sway while fast dino collisions escaped it) to `0.00005f` (~0.003°, still ~50× above `[-1,1]` rotation round-trip noise). Added `STATIC_WORLD_SWAY_EPS = 0.05f`: when translation matches a cached anchor within `POS_EPS` but rotation moves (swaying tree), only root translation `w[12..14]` snaps while rotation passes through unquantized every frame (and updates the swaying entry in place). Passed proxy `lifecycle_test`.

### Remix Runtime Changes (`D:\Unity\dxvk-remix_numos`, 3 files modified)
1. **Skip hidden-instance OMM baking (`src/dxvk/rtx_render/rtx_opacity_micromap_manager.cpp`):** Added `|| instance.isHidden()` to the OMM skip check. Previously, hidden vanilla grass (`0xBF84A6E3285BBB20`, `worldSpaceSecondaryOpacityTextures`) baked 33.5M-microtriangle L8 OMMs (~306 MiB VRAM / ~630 MiB OMM budget). OMM usage dropped from ~306 MiB to ~30–42 MiB.
2. **O(1) exact-position matching & cross-species slot-steal fix (`src/frontends/translator/instance_matcher.cpp`):**
   - Extended `m_exactSlots` exact-position lookup to all finite-position draws (not just `kContent`), preventing `> kWindow (32)` draw-order shifts from causing slot misses and recreation storms during camera rotation/panning.
   - Added `|| distance > query.otherMaterialDistanceSqr` to the `query.shapeFirst && !sameSignature` check (line 162, matching line 157's `!sameMaterial` guard). Previously, multi-bone trees CPU-skinned into `TSkinShaderD3D`'s shared dynamic VB shared a `coarseKey` (e.g. `BostonFern_Med` 196v, `Salacca_05` 130v, `Dryopteris_01` 76v all on `mat_BE9EEBA99A93A8C8`), allowing one species to steal a `recent` slot of a different species up to `rtx.uniqueObjectDistance` (20 units) away.
3. **Coarse-key `inputsKeyWith` soundness fix (`src/frontends/translator/draw_key.h`):** Included `(primitiveCount, numVertices)` for all `usesCoarseKey()` draws — and `(streams[i].buffer.id, offset, indices.id, baseVertex, startIndex, minVertexIndex)` when `!(flags & kContent)` — inside `inputsKeyWith()`. Previously, when a coarse-keyed draw stole another species' slot without `kContent`, `ChangeTest::test()` saw identical `inputsNoWorld` and emitted `batch->moves` instead of `CommitMode::Update`, literally teleporting e.g. `Salacca_05` onto `BostonFern_Med`'s position until the 2.5 MiB dynamic VB ring discarded. Verified with `[TREESWAP]` logging: `handleSwaps` dropped from 60+/frame to **0**, `posSwaps = 0`, steady-state BLAS builds = **0/frame** (42 fps at 2560×1440 Ultra), `test_d3d9_frontend_lifecycle` passed.

### Engine Notes & Pitfalls (Do Not Retry)
- **Do NOT raise `rtx.numFramesToKeepBLAS` (e.g. `600`):** In `D3D9v2` direct mode each terrain tile owns an unshared `BlasEntry`; keeping unlinked BLASes alive piles up past Remix's 32,768 buffer-cache limit (`rejected: scene=...`) and makes terrain disappear.
- **Tree `_hi` / `_lo` models (`simjp.exe` `0x47BB80`, `0x65BC10`):** Load-time detail-level selection only (`0=_lo`, `1=_med`, `2=_hi`), not dynamic distance LOD. `ATreeManager::PopulateSubCell` (`0x5DFCB0`) seeds a local RNG deterministically from `(subcellX << 16) | subcellZ`, so JPOG does not randomize tree species on cache misses.
- **Map-edge false camera cuts:** Fast RTS camera scrolling moves ~17–33 world units/frame; `rtx.uniqueObjectDistance <= 20` can trigger consecutive `RtCamera::isCameraCut()` resets at the map edge (50 avoids it; managed manually by user in `rtx.conf`).
- **Build & tooling paths:** MSVC `vcvars64.bat`/`vcvars32.bat` lives at `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\`; Remix build dirs are `D:\Unity\dxvk-remix_numos\_Comp64Release` and `_Comp64UnitTest`; Blender 4.0 Python (`C:\Program Files\Blender Foundation\Blender 4.0\4.0\python\bin\python.exe`) provides `pxr` USD bindings.

## Terrain/tree flicker fix, bridge stall fix, LOD lock & TSysShader FFP — 2026-11-24

- **Root cause of black/missing terrain tiles and flickering trees during camera movement (Remix NUMOS runtime):** `DirectState::handover` in `dxvk-remix_numos/src/frontends/d3d9/d3d9_frontend.cpp` overwrote `objects[handle]` on `CommitMode::Update` for an aged-out handle (`hiddenUnseen = true`, `RetainedScene::Entry::visible = false`), clearing `hiddenUnseen` back to `false` before `flush()` could unhide it. Fixed by calling `scene.setVisible(handle, true)` before `scene.buildInstance(...)` when `hiddenUnseen` is set and preserving `frameMergedPass`, `releasing`, `agedOut`, and `hiddenUnseen` across the handover entry replacement.
- **Bridge round-trip & ring stall fix (`patches/JPOG/proxy/d3d9_device.c`, `bridge.conf`):** Replaced per-frame `GetTextureHash` cache expiry with a reset-only `g_textureHashEpoch` plus mutation-driven invalidation (`TexGetSurfaceLevel_Hook`, `TexLockRect_Hook`, `SurfLockRect_Hook`, `UpdateSurface`, `UpdateTexture`, `StretchRect`, `ColorFill`, `SetRenderTarget`, `Reset`), and raised `clientCmdQueueSize = 16384`. Game-thread CPU dropped from ~57 ms to ~2.1–3.8 ms/frame; blocking `GetTextureHash` round trips dropped from 77.6/frame (54.4 ms) to 0.0/frame steady-state; command-ring full stalls dropped to 0.0 ms.
- **Terrain LOD 0 lock (`patches/JPOG/asi/jpog_view_fix.c`):** Hooked `TTerrainSection::ComputeLOD` (`TTerrainInterface.dll+0x7470`, counter `sm_ui32Counter` at `+0x123C4`) to keep all terrain sections at LOD 0 instead of splitting/merging into 4 stitch quadrants during camera movement. Welded terrain chunks dropped 617 → 197 and per-frame BLAS builds dropped ~80% (117–158 → 21–29/frame). Paired with `rtx.uniqueObjectDistance = 15` and `rtx.antiCulling.object.enable = True`.
- **`TSysShaderD3D` native FFP (`patches/JPOG/asi/jpog_view_fix.c`, `patches/JPOG/proxy/d3d9_device.c`):** Patched `TSysShaderHAL::IsCapableHighEnd` (`+0x27D0`) and `IsCapableLightScattering` (`+0x2830`) to return `0`, and hooked `Flush` (`+0x19B0`) and `Render` (`+0x22D0`) to enforce the built-in `FVF 0x142` path (identical 24-byte `XYZ + DIFFUSE + UV0` VB/IB so USD `GeometryAssetHash` replacements stay intact) while setting exported `g_mvWorldTransformActive = 1`. While active, `WD_SetTransform(D3DTS_WORLD, mv)` decomposes `World = MV * inv(View)` using `double` accumulation and skips fallback identity resets on draw. Zero-cost FFP repeat-skips jumped from ~142 to 530–759 draws/frame.
- **1-draw-per-frame legacy grass skip (`patches/JPOG/asi/jpog_view_fix.c`, `patches/JPOG/proxy/d3d9_device.c`):** Hooked `TGrassShaderHAL::Flush` (`TTerrainShaderD3D.dll+0x11F50`) and `Render` (`+0x12D70`) via exported `g_grassFlushState`. The proxy lets draws proceed until the first draw carrying a tagged `rtx.legacyMaterial.worldSpaceSecondaryOpacityTextures` stage-0 texture (`0xBF84A6E3285BBB20`) plus non-NULL stage-1 coverage texture executes (keeping Remix 3D `TerrainGrass` `hasCoverage=1` live every frame while hidden by `hideVanillaGrass = 1`), then skips all remaining ~200–500 legacy grass patches. Steady-state render-thread CPU dropped to **8.1 ms/frame**, game-thread CPU to **1.5 ms/frame**, bridge round trips to **0.0 ms**, and BLAS builds to **1.1/frame**.
- **Opaque vegetation USD materials (`JPOGRTX/vegetation.usda`, `mod.usda`):** Set `inputs:alpha_test_type = 7`, `inputs:blend_enabled = 0`, `inputs:use_legacy_alpha_state = 0` across all replaced vegetation shader overrides (including 4 previously un-overridden ingested plant refs).

## Release proxy/ASI ownership and optimization pass — 2026-09-23

This section is the current release policy and supersedes the older Q1/Q2 and sky experiments retained below as historical analysis.

- Terrain quality is game-owned. The ASI does not patch `IsHighEndTerrain`, `IsMediumTerrain`, the Q1/Q2 render entries, or the terrain shader getters.
- Sky rendering is game/Remix-owned. The proxy no longer exports or checks `g_skipWorldOverride`, does not suppress world reconstruction for sky draws, and does not inject a camera-centred sky world matrix.
- The terrain render hook at `TTerrainShaderD3D.dll+0x5790` remains solely to scope automatic `rtx.terrainTextures` tagging. It does not select or alter a terrain quality path.
- The ASI retains only three release responsibilities: main-camera View/Projection forwarding, Remix weather preset sync, and terrain texture-tag scope.
- The proxy retains general matrix reconstruction plus tagged particle conversion. Its release build uses MSVC `/O2 /Oi /Gy` and linker `/OPT:REF /OPT:ICF`; `/GL` is intentionally excluded because MSVC rejects whole-program optimization with the proxy's no-CRT `memcpy`/`memset` helpers. The ASI uses `/GL /LTCG` in addition to those release optimizations.
- Dead release baggage removed: the unused ASI copy of the last View matrix, unused math headers, stale sky/Q1 discovery comments, unused transform-helper locals, and an obsolete linker map that still listed the removed sky export.

## Proxy hot-path cache optimization — 2026-09-23

- `set_world_if_changed()` now skips an exact duplicate WORLD matrix instead of submitting a redundant `SetTransform` call to Remix. The cache is updated only after a successful real-device call.
- Game-originated WORLD writes synchronize the same cache after successful forwarding, preventing a synthetic transform from being incorrectly skipped after the game changes device state.
- The WORLD cache remains conservatively invalidated at `Reset` and each `Present`, so deduplication is frame-local.
- Texture-hash lookup now checks the same pointer-derived slot used for insertion instead of scanning all 64 direct-mapped entries. Collision behavior is unchanged.
- Removed write-only `cachedWorld`, `hasCachedWorld`, and `viewCapturedThisFrame` fields and their per-draw copies.
- No terrain-quality or sky behavior changed.

## Discarded proxy water and selection/highlight experiments — 2026-09-23

- Confirmed the active proxy and ASI already contained no water-specific transform, shader, camera, or draw-routing code.
- Removed the abandoned selection/highlight decal-tag parsing and conversion branch; tagged particle conversion remains independent and active.
- Remix-owned `rtx.animatedWaterTextures` and `rtx.decalTextures` settings remain in project and deployed configuration; this cleanup is limited to patch code and inactive captures.
- Removed the inactive water and selection trace captures from the active project tree. Historical backups remain isolated under `backups/` and do not affect builds or runtime.

## TerrainQuality=2 static path + terrain break triage (simjp.exe) — 2026-03-24

## LIVE traced terrain/system SVCF callers -> behavior mapping (TTerrainShaderD3D + TSysShaderD3D) — 2026-03-24

### Summary
From the live caller clusters, terrain and system paths are separable by module+caller fingerprint. Terrain SVCF callers (`0x0F29230B`, `0x0F2923EB`) are tightly grouped in `TTerrainShaderD3D.dll` and behave like one terrain constant-emit routine with two SVCF sites. System callers (`0x0BE72918`..`0x0BE72BF3`) form one contiguous SVCF batch routine in `TSysShaderD3D.dll` that writes many register bands. The minimal reversible proxy fix is a one-draw latch set by terrain SVCF caller fingerprint and consumed at wrapped DIP caller `0x6D05E75D`.

### Key Addresses
| Address | Description |
|---|---|
| 0x0F280000 | `TTerrainShaderD3D.dll` base |
| 0x0F29230B | Terrain SVCF caller A (RVA `0x1230B`) |
| 0x0F2923EB | Terrain SVCF caller B (RVA `0x123EB`) |
| 0x0BE70000 | `TSysShaderD3D.dll` base |
| 0x0BE72918 | System SVCF cluster start (RVA `0x2918`) |
| 0x0BE72BF3 | System SVCF cluster end (RVA `0x2BF3`) |
| 0x6D040000 | `dxwrapper.dll` base |
| 0x6D05E75D | Wrapped DIP caller (RVA `0x1E75D`) |

### Recovered containing function(s) (caller-cluster reconstruction)
1. Terrain constants setup function
   - Window: `0x0F292300`–`0x0F292400` (contains both terrain callers)
   - Provisional signature: `@ 0x0F292300 void __thiscall TerrainShader_EmitVSConstants(void* this, IDirect3DDevice9* dev)`

2. System constants batch function
   - Window: `0x0BE72900`–`0x0BE72C00` (contains all 15 system callers)
   - Provisional signature: `@ 0x0BE72900 void __thiscall SysShader_EmitVSConstantsBatch(void* this, IDirect3DDevice9* dev)`

3. Wrapped DIP dispatch site
   - Window: `0x6D05E740`–`0x6D05E780` (contains observed DIP caller)
   - Provisional signature: `@ 0x6D05E740 HRESULT __stdcall Dxwrapper_DrawIndexedPrimitive_Wrap(...)`

### c17-c20 vs other register ranges
- `c17-c20` is the model-view block (already tracked in proxy via `VS_REG_MV_START=17`, `VS_REG_MV_END=21`).
- Terrain-specific sequence is best represented as a pair in terrain module:
  - one call writing `StartRegister=17`, `Vector4fCount=4` (c17-c20), and
  - companion matrix-range write (commonly `StartRegister=0`, `Vector4fCount=4`).
- System caller cluster should be treated as mixed-band uploads and excluded from terrain-only gating.

### Recommended minimal, reversible proxy condition (terrain quality-2 path)
Place producer in wrapped `SetVertexShaderConstantF`, consumer in wrapped DIP.

```c
// Per-device/per-thread draw-lifetime flag
bool terrainQ2ArmedForNextDraw = false;

// Wrapped_SetVertexShaderConstantF
uintptr_t ra = (uintptr_t)_ReturnAddress();
bool terrainCaller = (ra == 0x0F29230Bu) || (ra == 0x0F2923EBu);
bool mvWrite = (StartRegister == 17u && Vector4fCount == 4u);   // c17-c20
bool pairWrite = mvWrite || (StartRegister == 0u && Vector4fCount == 4u);

if (terrainCaller && pairWrite) {
  terrainQ2ArmedForNextDraw = true;
}

// Wrapped_DrawIndexedPrimitive
uintptr_t dipRa = (uintptr_t)_ReturnAddress();
if (terrainQ2ArmedForNextDraw && dipRa == 0x6D05E75Du) {
  ApplyTerrainQ2OnlyWorkaround();
}
terrainQ2ArmedForNextDraw = false;
```

### Single next fix recommendation
Implement the one-draw terrain latch above and route only that path to your selected terrain workaround (passthrough or transform override). This is the narrowest change that targets terrain caller fingerprints without touching system-shader draws.

### Rollback instructions
1. Disable guard with one config flag (e.g., `TerrainQ2CallerGuard=0`).
2. Or remove just:
   - terrain caller arm block in wrapped `SetVertexShaderConstantF`, and
   - `terrainQ2ArmedForNextDraw && dipRa == 0x6D05E75D` check in wrapped DIP.
3. Rebuild/redeploy proxy DLL.

### Summary
Requested end-to-end static analysis could not be executed in this run because terminal-capable tool access is unavailable in this session (no ability to run `python -m retools.*`, `bootstrap`, `sigdb pull`, or `decompiler --types`). As a result, exact `simjp.exe` addresses for `Options.ini/TerrainQuality` parse/consume path are not yet resolved from the binary in this pass. Confirmed proxy-side symbols and routing hooks are documented below, plus a strict command sequence to complete the binary trace immediately when terminal access is available.

### Bootstrap status
- `NOT EXECUTED` in this session (tooling constraint: no command execution surface).
- Required first commands (run from repo root):
  1. `python verify_install.py`
  2. `if not exist retools\data\signatures.db python -m retools.sigdb pull`
  3. `python -m retools.bootstrap "D:\Games\JPOGRTX\JPOG\simjp.exe" --project JPOG`
  4. `python -m retools.search "D:\Games\JPOGRTX\JPOG\simjp.exe" strings -f "TerrainQuality,Options.ini,terrain" --xrefs`

### Confirmed symbols/hooks (proxy layer)
These are already present in the JPOG proxy codebase and are high-value intervention points once TerrainQuality=2 callsites are known:

| Symbol / Site | Description |
|---|---|
| `g_cfgWorldOverrideOnDP` | Runtime override toggle used by wrapped draw path |
| `g_cfgWorldOverrideOnDIP` | Runtime override toggle used by wrapped draw path |
| `g_cfgForceIdentityView` | Runtime transform forcing toggle |
| `g_cfgRejectIdentityView` | Runtime transform rejection toggle |
| `SLOT_DrawPrimitive = 81` | D3D9 device vtable draw slot |
| `SLOT_DrawIndexedPrimitive = 82` | D3D9 device vtable indexed draw slot |
| `SLOT_SetVertexShader = 92` | VS bind hook point |
| `SLOT_SetVertexShaderConstantF = 94` | VS constants capture hook point |
| `VS_REG_MV_START = 17` / `VS_REG_MV_END = 21` | Captured model-view register span in current JPOG proxy |

### What is still unresolved (requires retools execution)
1. `Options.ini` parser function and xrefs to `TerrainQuality` storage.
2. Exact branch condition for value `2` (cmp/je/jne chain and address).
3. Call chain from option consume site -> terrain render dispatch -> shader/vertex declaration setup -> sampler/texture state setup.
4. Shader constant usage specific to quality=2 terrain path (register ranges, update sites, and call frequency).

### Deterministic completion plan (exact command sequence)
After bootstrap creates/populates `patches/JPOG/kb.h`, run:

1. **Find parse/consume sites**
   - `python -m retools.search "D:\Games\JPOGRTX\JPOG\simjp.exe" strings -f "TerrainQuality,Options.ini" --xrefs`
   - For each xref function: `python -m retools.decompiler "D:\Games\JPOGRTX\JPOG\simjp.exe" <addr> --types patches/JPOG/kb.h`

2. **Resolve quality=2 branch**
   - `python -m retools.search "D:\Games\JPOGRTX\JPOG\simjp.exe" insn "cmp *,0x2" --near "TerrainQuality" --range 0x600`
   - `python -m retools.xrefs "D:\Games\JPOGRTX\JPOG\simjp.exe" <branch_target_addr> -t call`

3. **Map terrain render path**
   - `python -m retools.search "D:\Games\JPOGRTX\JPOG\simjp.exe" imports -d d3d9`
   - `python -m retools.search "D:\Games\JPOGRTX\JPOG\simjp.exe" insn "call *[*,0x148]"` (DIP vtable dispatch patterns)
   - `python -m retools.search "D:\Games\JPOGRTX\JPOG\simjp.exe" insn "call *[*,0x170]"` (SetVertexShader)
   - `python -m retools.search "D:\Games\JPOGRTX\JPOG\simjp.exe" insn "call *[*,0x178]"` (SetVertexShaderConstantF)
   - Decompile hits with `--types patches/JPOG/kb.h` and build callgraph up/down.

4. **Shader/decl/state evidence for quality=2 path**
   - Identify where terrain path sets declaration/FVF + texture stage/sampler state in the quality=2 branch.
   - Add function/global signatures to `patches/JPOG/kb.h` (`@`/`$` entries) immediately when resolved.

### Candidate fixes (minimal + reversible)
These are prepared as constrained options; exact target addresses in `simjp.exe` are pending the unresolved static branch mapping above.

#### Candidate A — FFP-compatible bypass for terrain draws only (proxy-layer)
- **Where:** Wrapped draw path around `DrawIndexedPrimitive`/`DrawPrimitive` routing in `patches/JPOG/proxy` (symbols above).
- **Mechanism:** Gate by terrain signature (specific shader/declaration/state fingerprint from resolved path), then force safe FFP fallback for those draws only.
- **Risk:** Medium (possible false positives affecting non-terrain draws if fingerprint too broad).
- **Rollback:** Disable feature flag in `proxy.ini` or revert one guarded branch in proxy and rebuild.

#### Candidate B — selective passthrough for problematic shader path
- **Where:** Proxy branch that currently nulls/replaces shader behavior for matched draws.
- **Mechanism:** If quality=2 terrain shader signature is detected, bypass conversion and pass through original shader path unchanged.
- **Risk:** Low-Medium (terrain may render but with reduced Remix control for those draws).
- **Rollback:** Remove one conditional passthrough guard.

#### Candidate C — config/constant clamp (INI-level)
- **Where:** `Options.ini` / runtime option consumption for terrain detail quality.
- **Mechanism:** Force `TerrainQuality` to `1` when problematic map/profile is loaded, or clamp quality-dependent constant to non-breaking value.
- **Risk:** Low, but quality downgrade side effect.
- **Rollback:** Restore original INI value or remove clamp hook.

### Best first fix to try
**Candidate B (selective passthrough)** first, because it is most reversible and least invasive while gathering evidence. It can be enabled only for the confirmed quality=2 terrain shader/declaration fingerprint once static mapping is complete.

### Live validation commands (to run after static addresses are filled)
1. Attach:
   - `python -m livetools attach simjp.exe`
2. Confirm TerrainQuality consume function is hit when toggling option:
   - `python -m livetools trace <terrain_quality_consume_fn_addr> --count 20 --read`
3. Confirm quality=2 branch target execution:
   - `python -m livetools trace <quality2_branch_block_addr> --count 20 --read`
4. Confirm terrain draw site hit-rate:
   - `python -m livetools collect <terrain_dip_callsite_addr> --duration 5000`
5. Patch test (temporary):
   - `python -m livetools mem write <conditional_jump_addr> <patched_bytes>`
6. Expected observations:
   - Quality toggle causes different hit counts at branch block.
   - Terrain-only draw callsite frequency changes with camera over terrain.
   - Temporary patch either restores terrain rendering at quality=2 or eliminates specific corruption mode.

### KB update status
No new `simjp.exe` function/global entries added to `patches/JPOG/kb.h` in this pass because binary decompilation and xref extraction were not executable in-session.
## Q2 terrain invisible: centroid double-positioning bug — 2026-03-25

### Summary

Root cause identified and fixed: `applyCentroidOrMvpPath` tried the centroid VB-sample
lookup BEFORE the MVP decomposition. For Q2 terrain (world-space vertices), the centroid
is a world coordinate like (500, 50, 700). The proxy interpreted this as a model transform
and set `WORLD = translation(500, 50, 700)`, effectively double-positioning the geometry
(vertices are already at that world position). The MVP decomposition (which would correctly
produce `WORLD = Identity`) was never reached because the centroid path returned early.

### Root Cause Chain

1. TTerrainShaderD3D.dll calls SVCF(c0-c3) setting MVP — this clears `gameWorldSet=0`
2. Terrain does NOT write c17-c20 (MV), so `mvDirty` stays 0 from previous draw
3. `applyMvPath`: `mvDirty=0` + `PreserveWorldOnMvStall=1` → returns without setting WORLD
4. `applyCentroidOrMvpPath`: `gameWorldSet=0` → proceeds
5. Centroid lookup finds terrain VB with first vertex at e.g. (500, 50, 700)
6. `dist2 > CENTROID_MIN_DIST2` (2500) → centroid fires, sets WORLD = translate(500,50,700)
7. MVP decomposition never reached — centroid returns early
8. In Remix: vertex capture deprojection produces world-space positions, but `objectToWorld`
   is the centroid translation instead of Identity → terrain placed at 2x its correct position

### Fix Applied

Swapped the order in `applyCentroidOrMvpPath`: MVP decomposition runs first when
`hasMVP` is set (VS-based draws with shader constants). Centroid is only used as a
fallback for FFP draws without shader constants.

For Q2 terrain: MVP * inv(P) * inv(V) = P*V * inv(P) * inv(V) = Identity → correct.

### Key Addresses
| Address | Description |
|---------|-------------|
| d3d9_device.c:946 | `applyCentroidOrMvpPath` — reordered MVP-first |
| RVA 0x1230B, 0x123EB | TTerrainShaderD3D.dll SVCF call sites |
| CENTROID_MIN_DIST2 | 2500.0f threshold that terrain world coords always exceed |

### Impact on Other Objects

- VS-based draws (`hasMVP=1`): now use MVP decomposition (exact) instead of centroid (heuristic)
- FFP draws (`hasMVP=0`): still use centroid as before (no change)
- Game-WORLD draws (`gameWorldSet=1`): still short-circuit (no change)
- This should improve all VS-based object placement, not just terrain

### Suggested Live Verification

Launch the game with TerrainQuality=2. The terrain should now be visible in Remix.
Check that other VS-based objects (dinosaurs, buildings) are still placed correctly.

## TTerrainShaderD3D.dll Quality Branching Analysis — Q1 terrain + Q2 sky feasibility

### Summary
TTerrainShaderD3D.dll has built-in multi-quality rendering paths in both `Flush()` and per-tile `Render()`. The quality is controlled by two boolean flags in the TTerrainShaderHAL object: `IsHighEndTerrain` (offset 0x2B8) and `IsMediumTerrain` (offset 0x2BA). In `Flush()`, `RenderSky()` is called **before** the `IsHighEndTerrain` check — sky rendering is unconditional. Terrain rendering state (custom VS/PS) and per-tile draw calls are gated by these quality flags. Forcing `IsHighEndTerrain=false` while keeping the DLL loaded gives Q2 sky + Q0/Q1 terrain fallback paths in the DLL itself.

### Key Discovery: Three Quality Levels in TTerrainShaderD3D.dll

The DLL supports three terrain rendering modes, matching the engine's quality system:

| Level | Enable vtable | Check vtable | Flag offset | Vertex decl size |
|-------|-------------|-------------|-------------|-----------------|
| Q0 Low | 0xE0 | 0xD4 | 0x2B9 | 0x14 |
| Q1 Medium | 0xDC | 0xD0 | 0x2BA | 0x1C |
| Q2 High | 0xD8 | 0xCC | 0x2B8 | 0x04 |

### Flush() Control Flow

`
Flush() {
    device = this->vtable[0x134]();
    if (GetFlushShaderSorted()) goto common_path;          // skip if sorted
    renderList = this->vtable[0x138]();
    if (renderList->count == 0) goto common_path;          // skip if empty

    RenderSky();                                // 0x10009C90 — ALWAYS runs

    if (IsHighEndTerrain()) {                   // vtable[0xCC] at 0x10004562
        // Q2 terrain state setup:
        // - Set SHADERDECL vertex declaration
        // - Fog parameters, lighting coefficients
        // - Sky-to-terrain blending constants
    }

common_path:                                    // 0x10004469
    VS = GetVertexShaderHandle();               // returns this->field_0x70
    SetVertexShader(device, VS);                // [edx+0x130] at 0x10004474

    if (*0x1004AF10 == 0) {                     // rendering mode flag
        SetRenderState(0x1B, 1);                // fog on
        SetRenderState(0x13, 5);
        SetRenderState(0x14, 6);
        PS = GetPixelShaderHandle();            // returns this->field_0x7C
        SetPixelShader(device, PS);             // [edx+0x160] at 0x100044BE
    } else {
        SetRenderState(0x1B, 0);                // fog off
        SetRenderState(0x13, 2);
        SetRenderState(0x14, 1);
        PS = this->field_0x80;
        SetPixelShader(device, PS);             // [edx+0x160] at 0x100044F7
    }

    TOrderTable::Render();                      // dispatches to per-tile Render()
    SetPixelShader(device, NULL);               // [eax+0x160] at 0x1000451B
    // reset texture stages, return
}
`

**Critical observations:**
1. `RenderSky()` at 0x10004559 runs BEFORE any terrain quality check
2. Terrain VS/PS (`SetVertexShader`/`SetPixelShader`) are set in the **common path** at 0x10004474/0x100044BE — they run for ALL quality levels
3. `TOrderTable::Render()` always executes — per-tile Render() is always called

### Per-Tile Render() Quality Branching (0x10005790)

`
Render(TRenderPacket* packet) {
    if (IsDying()) return;
    // Get vertex pool, index pool from packet

    if (IsHighEndTerrain()) {           // vtable[0xCC] at 0x10005845
        // Q2 HIGH: use vertex decl size 0x4
        // custom texture bindings, fog, shader constants
        // DrawIndexedPrimitive(TRIANGLELIST)
    }
    else if (IsMediumTerrain()) {       // vtable[0xD0] at 0x10005858
        // Q1 MEDIUM: use vertex decl size 0x1C
        // texture stage setup, simpler bindings
        // DrawIndexedPrimitive(TRIANGLELIST)
    }
    else {
        // Q0 LOW: use vertex decl size 0x14
        // simplest texture stage setup
        // DrawIndexedPrimitive(TRIANGLELIST)
    }
}
`

**All three paths end with DrawIndexedPrimitive.** The Q0/Q1 paths set up their own vertex declarations and texture stages, but the terrain VS/PS from Flush are still active.

### Key Addresses

| Address (DLL VA) | RVA | Description |
|----------|-----|-------------|
| 0x10010D50 | 0x10D50 | `IsHighEndTerrain()` — returns byte at this+0x2B8 |
| 0x10010D40 | 0x10D40 | `IsMediumTerrain()` — returns byte at this+0x2BA |
| 0x10010D30 | 0x10D30 | `IsLowTerrain()` — returns byte at this+0x2B9 |
| 0x10010CF0 | 0x10CF0 | `EnableHighEndTerrain(bool)` — checks capability, sets 0x2B8 |
| 0x10010CB0 | 0x10CB0 | `EnableMediumTerrain(bool)` — checks capability, sets 0x2BA |
| 0x10010C70 | 0x10C70 | `EnableLowTerrain(bool)` — checks capability, sets 0x2B9 |
| 0x10009C90 | 0x9C90 | `RenderSky()` — sky dome rendering (called before terrain check) |
| 0x10005790 | 0x5790 | `Render()` — per-tile terrain rendering (3 quality paths) |
| 0x10004430 | 0x4430 | `Flush()` — frame dispatch (sky + terrain + order table) |
| 0x10010FE0 | 0x10FE0 | `GetVertexShaderHandle()` — returns this->field_0x70 |
| 0x10010FD0 | 0x10FD0 | `GetPixelShaderHandle()` — returns this->field_0x7C |
| 0x10004562 | - | IsHighEndTerrain check in Flush (`call [eax+0xCC]`) |
| 0x10004474 | - | SetVertexShader in Flush (`call [edx+0x130]`) — common path |
| 0x100044BE | - | SetPixelShader in Flush path A (`call [edx+0x160]`) |
| 0x100044F7 | - | SetPixelShader in Flush path B (`call [edx+0x160]`) |

### EXE Call Sites

| Address | Description |
|---------|-------------|
| 0x47BC55 | `call [eax+0xD8]` — exe calls `EnableHighEndTerrain(quality)` |
| 0x473BF8 | `call [eax+0xCC]` — exe checks `IsHighEndTerrain()` to determine effective quality |
| 0x473C14 | `call [eax+0xD0]` — exe checks `IsMediumTerrain()` as fallback |

### Recommended Patch Strategy

**Phase 1: Force IsHighEndTerrain=false (simplest test)**

Patch `IsHighEndTerrain()` in TTerrainShaderD3D.dll to always return 0:

`
Original (RVA 0x10D50, 8 bytes):
  0F B6 81 B8 02 00 00 C3   ; movzx eax, byte ptr [ecx+0x2B8]; ret

Patched:
  33 C0 90 90 90 90 90 C3   ; xor eax, eax; nop*5; ret
`

This keeps the DLL loaded (sky works), but forces Q0/Q1 terrain paths in both Flush and Render. Test if terrain appears in Remix with this alone.

**Phase 2: VS/PS bypass (if Phase 1 terrain still invisible)**

If Q0/Q1 per-tile Render still doesn't show terrain in Remix (because terrain VS/PS are set in Flush common path), additionally bypass the shader setup:

Option A — Skip VS setup, let Flush pass NULL VS:
`
At 0x10004469 (RVA 0x4469), replace:
  8B CF           ; mov ecx, edi
  E8 70 CB 00 00  ; call GetVertexShaderHandle
  8B 13           ; mov edx, [ebx]
  50              ; push eax          ← VS handle
  53              ; push ebx

With:
  E9 8F 00 00 00  ; jmp 0x100044FD    (skip entire VS/PS setup block)
  90 90 90 90 90  ; nops
`

This jumps directly to the counter reset + TOrderTable::Render, skipping VS/PS entirely. Terrain tiles would render with FFP (no shaders), which Remix can track.

Option B — Keep VS call but force NULL handle:
`
At 0x10004469 (RVA 0x4469), replace:
  8B CF                     ; mov ecx, edi
  E8 70 CB 00 00            ; call GetVertexShaderHandle

With:
  31 C0                     ; xor eax, eax  (VS handle = NULL)
  90 90 90 90 90            ; 5x NOP (skip GetVSHandle call)
`

**Phase 3: IsMediumTerrain selection (optional)**

If Q0 LOW terrain looks too simple, also force `IsMediumTerrain=true` for nicer Q1 rendering:

`
Option: Patch EnableHighEndTerrain to force the medium flag instead:
At 0x10010D08 (RVA 0x10D08):
  Original: 74 0E  (Q1 sky+0x0E → force-0 path)
  Patched:  EB 0E  (jmp +0x0E → always force flag to 0)
`

### Details: IsHighEndTerrain / EnableHighEndTerrain

`c
// IsHighEndTerrain at DLL VA 0x10010D50 (RVA 0x10D50)
bool __thiscall IsHighEndTerrain(TTerrainShaderHAL* this) {
    return *(BYTE*)(this + 0x2B8);
}

// EnableHighEndTerrain at DLL VA 0x10010CF0 (RVA 0x10CF0)
void __thiscall EnableHighEndTerrain(TTerrainShaderHAL* this, bool enable) {
    if (IsCapableHighEndTerrain()) {  // vtable[0xE4]
        this->field_0x2B8 = enable;
    } else {
        this->field_0x2B8 = 0;        // force false
    }
}
`

### Suggested Live Verification

1. Set `TerrainQuality=2` in Options.ini
2. Apply Phase 1 patch (IsHighEndTerrain → always false) via ASI
3. Launch game — expected: Q2 sky dome renders (textured), terrain uses Q0/Q1 DLL fallback path
4. If terrain invisible: apply Phase 2 (VS/PS bypass) — terrain renders with FFP
5. If terrain visible but too simple: apply Phase 3 (force IsMediumTerrain=true)


## Q2 Sky + Q0 Terrain FFP — Complete Implementation Plan

### Summary

Deep analysis of both shader DLLs and the proxy reveals the exact mechanism needed to get Q2 sky + working terrain in Remix. The key insight: TSysShaderD3D's working FFP path calls `SetTransform(WORLD, render_packet+0xC)` before each DIP — this is what Remix uses to track geometry. TTerrainShaderD3D never calls SetTransform for terrain. The fix patches TTerrainShaderD3D.dll in memory via ASI to: (1) force Q0 Render path (stride 0x14 = FFP-compatible), (2) strip VS/PS to NULL for terrain (sky keeps its own shaders), and (3) inject SetTransform(WORLD) per tile before DIP. No proxy changes needed — the existing `PreserveWorldOnMvStall=1` and `gameWorldSet` mechanism handles it.

### Architecture Discovery: Why Q1 Works and Q2 Doesn't

**TSysShaderD3D::Render FFP path (field_0x3C == 0) at 0x100022D0:**
```
; Sets three transforms before DIP:
push (render_context + 0x484)     ; projection matrix
push 3                             ; D3DTS_PROJECTION
push device
call [device_vtable + 0x94]        ; SetTransform

lea eax, [render_packet + 0xC]     ; <<<< PER-TILE WORLD MATRIX
push eax
push 0x100                         ; D3DTS_WORLD (256)
push device
call [device_vtable + 0x94]        ; SetTransform

push identity_matrix               ; global at 0x100098A0
push 2                             ; D3DTS_VIEW
push device
call [device_vtable + 0x94]        ; SetTransform
```

This is why Q1 terrain works in Remix: each terrain tile gets a `SetTransform(D3DTS_WORLD)` call with its per-tile world matrix from the render packet. Remix reads this and tracks geometry correctly.

**TTerrainShaderD3D::Render (all quality paths):**
- Never calls `[device+0x94]` (SetTransform)
- Terrain position transform happens entirely in the VS via constants (c0-c3 MVP, c4-c5 per-tile offset)
- Remix cannot extract world position from VS constants → terrain invisible

### Toshi Engine SetTransform Mechanism

| Vtable offset | D3D equivalent | State constant |
|--------------|---------------|----------------|
| `[device + 0x94]` | `IDirect3DDevice8::SetTransform` | - |
| state = 2 | D3DTS_VIEW | 2 |
| state = 3 | D3DTS_PROJECTION | 3 |
| state = 0x100 (256) | D3DTS_WORLD | 256 |

Device pointer obtained via: `this->vtable[0x134]() → return_value + 0x178`

### TRenderPacket Layout (Toshi engine)

| Offset | Type | Description |
|--------|------|-------------|
| +0x00 | ptr | vtable / linked list |
| +0x08 | ptr | Resource pointer (TMesh / TTerrainMesh) |
| +0x0C | float[16] | Per-instance world matrix (used by TSysShader FFP) |

### TTerrainShaderD3D::Render — Stack Frame at Q0 Entry

After prologue (`push edi; push esi; push ebp; push ebx; sub esp, 0x64`):

| Offset | Content |
|--------|---------|
| [esp+0x00]-[esp+0x63] | Local variables (100 bytes) |
| [esp+0x64] | Saved EBX |
| [esp+0x68] | Saved EBP |
| [esp+0x6C] | Saved ESI |
| [esp+0x70] | Saved EDI |
| [esp+0x74] | Return address |
| [esp+0x78] | param_2 (TRenderPacket*) — __thiscall stack param |

Key registers at Q0 entry (0x10005AA4):
- `esi` = device (Toshi wrapper, obtained from render_context + 0x178)
- `ebp` = this (TTerrainShaderHAL)
- `ebx` = render_packet->resource (at packet + 8)

### Q0 Vertex Format

Q0 LOW uses stride **0x14 (20 bytes)**, single stream:
- Matches `D3DFVF_XYZ | D3DFVF_TEX1` = float3 position (12) + float2 UV (8) = 20 bytes
- This is the **same format** the proxy already defined as `TERRAIN_FVF`
- FFP-compatible: D3D9 can process this without a vertex shader
- Single texture stage (stage 0 = terrain diffuse, stage 1 disabled)

### Q0 Texture Stage States

```
TSS(0, COLOROP=1, MODULATE=2)
TSS(0, COLORARG1=2, TEXTURE=2)
TSS(0, COLORARG2=3, value=3)
TSS(0, ALPHAOP=4, MODULATE=2)
TSS(0, ALPHAARG1=5, TEXTURE=2)
TSS(1, COLOROP=1, DISABLE=1)
TSS(1, ALPHAOP=4, DISABLE=1)
RS(ZWRITEENABLE=0x1D, 0)
RS(FOGENABLE=0x1B, 0)
```

### Proxy Flow with Patches

With `PreserveWorldOnMvStall=1` (current setting):

1. ASI hook calls `SetTransform(WORLD=0x100, render_packet+0xC)` on Toshi device
2. Proxy's `WD_SetTransform` intercepts → sets `gameWorldSet = 1`, forwards to Remix
3. Q0 Render sets vertex buffer, textures, TSS, render states (no SVCF writes)
4. Q0 Render calls DIP via `[device + 0x11C]`
5. Proxy's `WD_DrawIndexedPrimitive` runs:
   - `applyMvPath`: `mvDirty=0`, `PreserveWorldOnMvStall=1` → skips (preserves WORLD) ✓
   - `applyCentroidOrMvpPath`: `gameWorldSet=1` → returns immediately (no centroid) ✓
6. Real DIP called with WORLD = per-tile world matrix → Remix tracks geometry ✓

### Implementation: 5 Memory Patches via ASI

#### Patch 1: IsHighEndTerrain → return false (RVA 0x10D50, 8 bytes)
```
Before: 0F B6 81 B8 02 00 00 C3   ; movzx eax, byte ptr [ecx+0x2B8]; ret
After:  33 C0 90 90 90 90 90 C3   ; xor eax, eax; nop×5; ret
```

#### Patch 2: IsMediumTerrain → return false (RVA 0x10D40, 8 bytes)
```
Before: 0F B6 81 BA 02 00 00 C3   ; movzx eax, byte ptr [ecx+0x2BA]; ret
After:  33 C0 90 90 90 90 90 C3   ; xor eax, eax; nop×5; ret
```

#### Patch 3: GetVertexShaderHandle → return NULL (RVA 0x10FEA, 3 bytes)
```
Before: 8B 47 70   ; mov eax, [edi+0x70]   (returns VS handle)
After:  33 C0 90   ; xor eax, eax; nop     (returns NULL)
```
Note: Validate call at RVA 0x10FE7 preserved — still updates internal state.

#### Patch 4: GetPixelShaderHandle → return NULL (RVA 0x10FDA, 3 bytes)
```
Before: 8B 47 7C   ; mov eax, [edi+0x7C]   (returns PS handle)
After:  33 C0 90   ; xor eax, eax; nop     (returns NULL)
```

#### Patch 5: Hook Q0 Render entry to inject SetTransform(WORLD) (RVA 0x5AA4, 6 bytes)

Overwrite 6 bytes at 0x10005AA4 with JMP to ASI trampoline + NOP:
```
Before: 8B 06 8B 54 24 08   ; mov eax,[esi]; mov edx,[esp+8]
After:  E9 XX XX XX XX 90   ; jmp hook_Q0_SetWorldTransform; nop
```

The trampoline (in ASI-allocated executable memory):
```asm
hook_Q0_SetWorldTransform:
    ; Save clobberable registers
    push ecx
    push edx                    ; ESP is now +8 from entry

    ; Get render packet world matrix: render_packet + 0xC
    mov eax, [esp + 0x80]      ; [esp+0x78+8] = render packet (param_2)
    lea eax, [eax + 0xC]       ; world matrix pointer

    ; Get device vtable
    mov edx, [esi]              ; esi = device → edx = device->vtable

    ; SetTransform(device, D3DTS_WORLD=0x100, world_matrix)
    push eax                    ; matrix pointer
    push 0x100                  ; state = WORLD
    push esi                    ; device
    call [edx + 0x94]           ; __stdcall SetTransform (cleans 12 bytes)

    ; Restore
    pop edx
    pop ecx

    ; Execute overwritten instructions
    mov eax, [esi]              ; original: mov eax, [esi]
    mov edx, [esp + 8]          ; original: mov edx, [esp+8]

    ; Jump back to continue Q0 path
    jmp 0x10005AAA              ; address patched at runtime
```

### Why Sky Is Unaffected

- `RenderSky()` at RVA 0x9C90 is called from Flush at 0x10004559 **before** IsHighEndTerrain check
- RenderSky sets its own sky VS and PS directly (not through GetVertexShaderHandle/GetPixelShaderHandle)
- Sky vertex declaration (SKYSHADERDECL) and sky textures are set independently
- Patches 3-4 only affect the terrain VS/PS getters called from Flush's common path at 0x10004469
- Sky rendering is completely isolated from terrain quality path changes

### Key Addresses Summary

| Module | RVA | VA | Description |
|--------|-----|-------|-------------|
| TTerrainShaderD3D.dll | 0x4430 | 0x10004430 | Flush() — frame dispatch |
| TTerrainShaderD3D.dll | 0x4469 | 0x10004469 | Flush common path (VS/PS setup) |
| TTerrainShaderD3D.dll | 0x4510 | 0x10004510 | TOrderTable::Render call in Flush |
| TTerrainShaderD3D.dll | 0x5790 | 0x10005790 | Render() — per-tile dispatch |
| TTerrainShaderD3D.dll | 0x5AA4 | 0x10005AA4 | Q0 LOW render entry (hook target) |
| TTerrainShaderD3D.dll | 0x5BD5 | 0x10005BD5 | Q0 DIP call [device+0x11C] TRISTRIP(5) |
| TTerrainShaderD3D.dll | 0x9C90 | 0x10009C90 | RenderSky() — unconditional |
| TTerrainShaderD3D.dll | 0x10CF0 | 0x10010CF0 | EnableHighEndTerrain(bool) |
| TTerrainShaderD3D.dll | 0x10D40 | 0x10010D40 | IsMediumTerrain() |
| TTerrainShaderD3D.dll | 0x10D50 | 0x10010D50 | IsHighEndTerrain() |
| TTerrainShaderD3D.dll | 0x10FD0 | 0x10010FD0 | GetPixelShaderHandle() |
| TTerrainShaderD3D.dll | 0x10FE0 | 0x10010FE0 | GetVertexShaderHandle() |
| TSysShaderD3D.dll | 0x22D0 | 0x100022D0 | Render() — FFP path sets WORLD |
| TSysShaderD3D.dll | 0x19B0 | 0x100019B0 | Flush() — FFP vs shader path |
| Toshi device | vtable+0x94 | - | SetTransform(__stdcall) |
| Toshi device | vtable+0x11C | - | DrawIndexedPrimitive |
| Toshi device | vtable+0x130 | - | SetVertexShader |
| Toshi device | vtable+0x14C | - | SetStreamSource |
| Toshi device | vtable+0x160 | - | SetPixelShader |

### Proxy Config Required (no changes needed)

```ini
PreserveWorldOnMvStall=1   ; already set — preserves our WORLD
TerrainFFPConversion=0     ; disabled — ASI handles FFP now
TerrainShaderBypass=0      ; disabled — not needed
TerrainQ2LatchGuard=0      ; disabled — not needed
```

### Suggested Live Verification

1. Set `TerrainQuality=2` in Options.ini
2. Build + deploy updated ASI with all 5 patches
3. Launch game — expected: Q2 textured sky dome + Q0 terrain tiles visible in Remix
4. If terrain is positioned incorrectly: the render_packet+0xC world matrix may need adjustment (try identity instead)
5. If terrain sky dome vtable is wrong: try Q1 stride (0x1C, 28 bytes) with `IsMediumTerrain=true` instead

## ASI + Proxy Code Review — 2026-04-01

### Summary

Reviewed all active source files: `jpog_view_fix.c` (ASI), `d3d9_device.c`, `d3d9_main.c`, `d3d9_wrapper.c` (proxy), plus build scripts and configuration. The code is well-structured with clear separation of concerns. Found several correctness and robustness issues, plus some performance opportunities.

### Issues Found

#### 1. [BUG] Stale VB pointers after Device Reset (d3d9_device.c)

`WD_Reset` clears proxy state (`mvDirty`, `viewCapturedThisFrame`, etc.) but does NOT clear the VB sample table (`g_vbSamples`, `g_vbSampleCount`) or lock table (`g_vbLocks`). After a Reset, all D3D resources are released and new ones allocated. New VBs can reuse the same virtual addresses, so `vbsample_get()` returns stale centroid data from the old VB that happened to share the address.

**Fix:** Clear `g_vbSampleCount = 0` and zero `g_vbLocks` in `WD_Reset`:
`c
// In WD_Reset, add before the return:
g_vbSampleCount = 0;
memset(g_vbLocks, 0, sizeof(g_vbLocks));
g_curStream0VB = NULL;
`

**Severity:** Medium. Causes wrong centroid positions after Alt+Tab or resolution changes.

---

#### 2. [BUG] Shared static vtable — second CreateDevice overwrites first (d3d9_device.c)

`s_device_vtbl` and `s_d3d9_vtbl` are static arrays shared by all instances. If `CreateDevice` is called twice (rare, but happens with some overlay/debug tools), the second call overwrites the vtable while the first device is still alive. Both devices share the same array, so this is mostly harmless in practice, but it's architecturally fragile.

**Fix (low priority):** Either document the single-device assumption, or allocate per-device:
`c
w->vtbl = (void**)HeapAlloc(GetProcessHeap(), 0, sizeof(s_device_vtbl));
memcpy(w->vtbl, s_device_vtbl_template, sizeof(s_device_vtbl));
`

**Severity:** Low. JPOG only creates one device.

---

#### 3. [PERF] Redundant SetTransform(WORLD, identity) every draw (d3d9_device.c)

`applyMvPath()` is called on every `DrawPrimitive` and `DrawIndexedPrimitive`. When `mvDirty == 0`, it calls `SetTransform(WORLD, identity)` unconditionally. At 60fps with hundreds of draws per frame, this generates thousands of redundant D3D calls that Remix must process.

**Fix:** Track whether identity was already set and skip:
`c
static void applyMvPath(WrappedDevice *self) {
    ...
    if (!self->mvDirty) {
        if (!self->hasCachedWorld) return; // identity already set or nothing to do
        // Only set identity once when transitioning from dirty->clean
        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal,
            D3DTS_WORLD, (float*)s_identity);
        self->hasCachedWorld = 0;
        return;
    }
    ...
}
`

**Severity:** Medium. Each unnecessary SetTransform call goes through the Remix runtime.

---

#### 4. [PERF] Linear scan in vbsample_get on every draw call (d3d9_device.c)

`vbsample_get()` does a linear scan of up to 4096 entries on every draw call. With a high draw count this adds up.

**Fix (optional):** A simple hash-by-pointer lookup would reduce this to O(1):
`c
#define VB_HASH_SIZE 256
#define VB_HASH(p) (((unsigned int)(p) >> 4) & (VB_HASH_SIZE - 1))
static VBSample *g_vbHashBuckets[VB_HASH_SIZE]; // chain or open-address
`

**Severity:** Low-Medium. Profile before optimizing — JPOG's draw count may be small enough.

---

#### 5. [ROBUSTNESS] Missing FlushInstructionCache after code patches (jpog_view_fix.c)

`PatchBytes()` and `InstallJump()` write over executable code using raw memcpy but never call `FlushInstructionCache()`. On x86 this is usually safe (coherent I-cache), but it's undefined behavior per Microsoft docs and could cause issues on some CPU/OS combinations.

**Fix:** Add after every code patch:
`c
static void PatchBytes(BYTE *addr, const BYTE *data, int len) {
    DWORD oldProt;
    VirtualProtect(addr, len, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy(addr, data, len);
    FlushInstructionCache(GetCurrentProcess(), addr, len);
    VirtualProtect(addr, len, oldProt, &oldProt);
}
`

**Severity:** Low. x86 is forgiving, but this is correct practice.

---

#### 6. [CLEANUP] Dead per-frame counters (d3d9_device.c)

These counters are maintained every frame but never logged or exposed:
- `g_centroidOverrides`, `g_centroidMisses`, `g_centroidNoVB`, `g_centroidNearOrigin`
- `g_gameWorldSets`, `g_svcfMvWrites`, `g_svcfMvpWrites`, `g_mvpWorldExtracts`

They cost a few instructions per draw call for zero benefit in release.

**Fix:** Either log them once per frame in `WD_Present` (useful for debugging), or remove them entirely. A good middle ground: log summary on first N frames only:
`c
if (self->frameCount < 5) {
    log_int("  centroidOverrides=", g_centroidOverrides);
    log_int("  mvpWorldExtracts=", g_mvpWorldExtracts);
    // ...
}
`

**Severity:** Negligible cost, but dead code is technical debt.

---

#### 7. [ROBUSTNESS] mat4_isIdentity uses exact float comparison (d3d9_device.c)

`mat4_isIdentity()` checks `m[i] != expected` with exact float equality. If the engine ever sends a matrix that's 0.9999999f instead of 1.0f, this would pass through as non-identity and reach Remix as a near-identity VIEW, breaking camera placement.

**Fix:** Add a tolerance:
`c
static int mat4_isIdentity(const float *m) {
    int i;
    for (i = 0; i < 16; i++) {
        float expected = (i % 5 == 0) ? 1.0f : 0.0f;
        float diff = m[i] - expected;
        if (diff > 1e-5f || diff < -1e-5f) return 0;
    }
    return 1;
}
`

**Severity:** Low. The Toshi engine likely sends exact identity, but the tolerance guard costs nothing.

---

#### 8. [ROBUSTNESS] ASI RenderSky re-entrancy (jpog_view_fix.c)

`Hook_RenderSky_Entry` saves the caller's return address in a single global (`g_skyOrigRet`). If `RenderSky` were called re-entrantly, the first caller's return address would be lost. Single-threaded rendering makes this safe in practice, but a stack-based approach would be more robust.

**Severity:** Negligible for JPOG (single render thread, no re-entrant sky calls).

---

#### 9. [CLEANUP] Orphan Python patch scripts in proxy/

`_patch_device.py`, `_patch_device2.py`, `_patch_ini.py`, `_patch_main.py`, `_patch_terrain_dip.py` appear to be one-shot development scripts that patch the C source. They reference variables (`g_cfgTerrainFFPConversion`, `currentVertexShader`, `terrainDrawPending`) that no longer exist in the current `d3d9_device.c`. These are dead files.

**Fix:** Delete them or move to `backups/`. They add confusion about the current code state.

**Severity:** None (doesn't affect runtime), but clutters the workspace.

---

#### 10. [MINOR] Proxy build.bat doesn't clean temporary files

Unlike the ASI `build.bat` (which does `del /q *.obj *.exp *.lib`), the proxy `build.bat` leaves `.obj` files behind.

**Fix:** Add cleanup at the end:
`at
:done
del /q *.obj 2>nul
endlocal
`

---

### Architecture Assessment

**What's good:**
- Clean separation: ASI hooks engine internals, proxy intercepts D3D9 calls. They communicate via a single exported symbol (`g_skipWorldOverride`).
- No-CRT builds keep the DLLs tiny and avoid runtime dependencies.
- The `RELAY_THUNK` pattern for unintercepted vtable methods is efficient — zero overhead, pure register forwarding.
- `proxy.ini`-driven runtime toggles make A/B testing easy without rebuilds.
- The return-address swap in `Hook_RenderSky_Entry/Exit` is clever and correct — brackets the sky render with minimal overhead.

**Overall:** The code is solid and well-engineered for its purpose. The main actionable items are #1 (stale VB pointers), #3 (redundant SetTransform), and #5 (FlushInstructionCache). The rest are minor improvements.

---

## JPOG weather controller and Remix preset synchronization — 2026-08-21

### Verified engine state

- `JPOGWeatherController_Update` is at `simjp.exe + 0x1F7B80` (`0x005F7B80` without ASLR) with prologue `56 53 83 EC 14`.
- The updater receives the controller in `ECX` and frame delta time on the stack.
- Controller field `+0x4C` is the effective visible weather ID, including cheat-forced weather. Fields `+0x54` and `+0x58` hold scheduled weather state.
- Startup weather registration and parsed `Weather.ini` arrays establish IDs: `0 normal`, `1 lightrain`, `2 heavyrain`, `3 tornado`, `4 heatwave`.
- Natural transitions copy current to previous and next to current, so synchronizing `+0x54` after the original updater observes completed transitions in the same frame.

### Remix mapping

| JPOG ID | JPOG weather | Remix preset |
|---:|---|---|
| 0 | `normal` | `partlyCloudy` |
| 1 | `lightrain` | `rainstorm` |
| 2 | `heavyrain` | `thunderstorm` |
| 3 | `tornado` | `thunderstorm` |
| 4 | `heatwave` | `sandstorm` |

The ASI initializes the bridge export `remixapi_InitializeLibrary`, obtains `remixapi_Interface.SetGameValue`, and sends `__weather.blend_seconds=11.0` before `__weather.target=<preset>`. Duplicate effective weather IDs are suppressed.

### Tornado cheat correction

The original hook watched scheduled weather at `+0x54`. During a visibly active tornado cheat, live controller state was `[+0x4C=3, +0x50=3, +0x54=0, +0x58=1]`, while the ASI success latch remained `0` (`partlyCloudy`). Returning to forced normal settled at `[+0x4C=0, +0x50=3, +0x54=0, +0x58=0]`. This proves `+0x50` can remain stale and `+0x54` can retain the ordinary schedule while a cheat is active. The ASI now reads the effective visible weather ID at `+0x4C`, which tracks both tornado (`3`) and normal (`0`).

Final validation used deployed SHA-256 `5083C1FED856D2C06965156A0650DDD4DDFBAA24C8A3938B9A51AD83EA3012A7`. With tornado visibly active, controller state was `[+0x4C=3, +0x50=3, +0x54=0, +0x58=0]` and the ASI success latch was `3`. Remix visibly switched to `thunderstorm`, proving the corrected field and API mapping end to end.

### End-to-end runtime proof

- The deployed hook replaced `0x005F7B80` with an `E9` jump into `jpog_view_fix.asi+0x1310` while preserving the updater trampoline.
- The final rebuilt and deployed ASI files have matching SHA-256 `159051EC1F2C48126D8836E5DAADD5C35054BC5B6B691A265A78063779C05150`.
- `jpog_view_fix.asi` and `d3d9_remix.dll` were both loaded in final validation PID `4072`; bridge client configuration reported `exposeRemixApi = True` and the server reported `[RemixApi] Initialized RemixApi.`
- After the first updater hit, the ASI cached `SetGameValue` as `0x6A2E18B0`, inside the loaded `d3d9_remix.dll` image.
- The ASI success latch was `0`, matching JPOG's initial `normal` ID. This latch changes from `-1` only after both `SetGameValue` calls return `REMIXAPI_ERROR_CODE_SUCCESS`, proving the initial `partlyCloudy` target was accepted by the bridge.
- Runtime memory confirmed the deployed mapping strings exactly: `partlyCloudy`, `drizzle`, `rainstorm`, `thunderstorm`, and `sandstorm`.

---

## TSkinShaderD3D.dll Native FFP (`FVF 0x1D2`) Switch — 2026-11-24

### Motivation
Big-park profiling (`D:\Games\JPOGRTX\JPOG\rtx-remix\logs\remix-dxvk.log`) showed `2,097–2,495` `D3D9VertexCapture` (`programmableVS`) draws/frame pinning the Remix CS thread at `44–47 ms` (`15–22 fps`) with `264–339` BLAS builds and `594–946` BLAS refits/frame.

### Verified Addresses (`TSkinShaderD3D.dll`, ImageBase `0x10000000`)
- `TSkinShaderHAL::Flush` (`RVA 0x1B60`, 7-byte prologue `57 56 55 53 83 EC 6C`): binds `FVF 0x1D2` (`XYZ | NORMAL | DIFFUSE | SPECULAR | TEX1`, 40B stride) and `SetPixelShader(0)` when `+0x6C == 0`, `+0xB0 == 0`, `+0xB1 == 0`.
- `TSkinShaderHAL::Render` (`RVA 0x2570`, 9-byte prologue `55 8B EC 81 EC 78 05 00 00`): submits `D3DTS_WORLD = packet->MV` (decomposed to `World = MV * inv(View)` by `d3d9.dll` while `g_mvWorldTransformActive == 1`).
- `TSkinShaderHAL::IsCapableHighEndSkinning` (`RVA 0x2120`) and `IsCapableScattering` (`RVA 0x2190`): patched to `xor eax, eax; ret`.
- **1-bone vs multi-bone branch (`RVA 0x26E4`, `83 FE 01 77 1B`):** When all submeshes have `numBones <= 1` (jeeps, animated building parts), the native 1-bone static-VB path (`0x10003110`) caused broken jeep geometry/frozen motion and shifted animated building submeshes in Remix. Patching `RVA 0x26E7` (`77` `ja` → `EB` `jmp`) routes all skinned meshes through the pooled CPU-skinning FFP path (`0x10003270`), fixing jeeps and buildings cleanly.

### Live Verification (`test-game`, `jpog_view_fix.asi` SHA-256 `993BE468...D43EC321`)
- `programmableVS` / `capture` dropped from `~2,100–2,500/frame` to **`2/frame`** out of `1,987–3,138` draws/frame.
- Render/CS thread dropped to `12.3–24.1 ms`, BLAS builds/refits dropped to `21–101` / `25–106` per frame; scene became GPU-bound (`91–98%` GPU utilization).
- Trade-off: forcing 1-bone meshes through CPU skinning raises game-thread CPU to `13.5–26.1 ms` (`13.6–25.8 MB/frame` dynamic VB uploads). Deployed to `D:\Games\JPOGRTX\JPOG\jpog_view_fix.asi` with rollback backup in `backups/JPOG/2026-11-24_0901_deploy-skinshader-ffp-main/`.

---

## Particle Pipeline (`TSpriteShaderD3D.dll`) & Remix Particle/Ripple/Flare Fixes — 2026-11-26

### Engine & Remix Root Causes
- **`TSpriteShaderD3D.dll` (`0x10002660` Flush / `0x10002A30` Render):** Switching 3D main-camera draws (`SyncShaderMainCameraContext == 1`) to the built-in `FVF 0x142` (`XYZ | DIFFUSE | TEX1`) FFP path preserves per-vertex `COLOR0` and per-material blend modes (`TSpriteMaterialHAL::PreRender` `+0x48`: `1=translucent`, `2=mul`, `3=additive`, `6=opaque`) without needing `rtx.uiTextures` workarounds or forced additive blending in the proxy.
- **Dark translucent smoke/dust/fire (`Smoke2.dds` / `0xAB056E8048DB8091`, e.g. `WelcCenterGateFire`):** Two compounding Remix runtime issues caused translucent game particles to render near-black against daylight skies:
  1. `resolve.slangh`'s NUMOS sky-ambient block was gated by `#ifdef RESOLVE_SKY_LIT_PARTICLES`, which was never defined anywhere in the codebase.
  2. `rtx.volumetrics.volumetricConsumerGain` scales `evalVolumetricConsumerNEE` to `0.008` under NUMOS (`rtx.skyMode = 1`).
  - **Fix:** Included `sky_provider.slangh` directly under `#ifdef RESOLVE_OPACITY_LIGHTING_APPROXIMATION` in `resolve.slangh`, removed the dead `#ifdef RESOLVE_SKY_LIT_PARTICLES`, and added `skyProviderSunDiffuse(worldPos)` alongside `skyProviderAmbient` so sunlit dust, splashes, and translucent orange fire keep proper daylight brightness and chroma while dimming under clouds and at night (`rtx.particles.skyAmbientScale = 1`).
- **4× water ripples per footstep (`FWave128.dds` / `0xA030C96CE856579C`):** `FWave128.dds` (`128x128` ARGB4444) is a single 90° quarter-circle arc; JPOG builds each ring from 4 quadrant quads sharing the exact center vertex.
  - **Fix:** `SceneManager::collectWaterRipples` now groups quads sharing any vertex within `0.15` world units into connected components and emits one centered `WaterRipple` per cluster with rotation-invariant radius `maxHorizDist / sqrt(2)`.
- **Disappearing light flare lights (`LFlare2.dds` / `0x425128AF7045BE2A`):** Static RE of `simjp.exe` (`ALensFlareParticles` / `ALensFlareGroupParticles` / `ALensFlareLocatorParticles`) revealed 6 engine-side culling/truncation causes (and confirmed no raycast occlusion or camera-motion VB cache exists):
  1. `ALensFlare*Particles::GetFarDistRecipSq` (`RVA 0x2BBE50`) culled flare systems past `buildingFadeFar` (and vehicles/helis stayed stuck at 160 units because `ALensFlareLocatorParticles::Initialize` never refreshed `+0x1C`).
  2. `ALensFlareParticles::Render` (`RVA 0x2BC806`) zeroed out building/vehicle flares past `HIGH_BuildingFadeFarDistance`.
  3. `ALensFlareGroupParticles::Render` (`RVA 0x2BAEA0`) collapsed fence/road-lamp flares to degenerate zero-area quads past `HIGH_WallFadeFarDistance`.
  4. `AUnitBuilding::SetRender(true)` (`RVA 0x0C6090`) skipped `InitLensFlares(false)` when a building entered the frustum beyond `buildingFadeFar`.
  5. `AParticleSystem::Render` (`RVA 0x2CFAA2`) downscaled every emitter whenever total particle vertices exceeded `m_iMaxVerts` (`2000`), integer-truncating small 1–3 flare buildings to `0` verts.
  6. `ALensFlareParticles` pool cap (`m_aiNumSystems[0x11]` at `0x008AE1C4`, RVAs `0x2D1C29`/`0x2D1D9B`/`0x2D1F4D`) was hardcoded to `15` buildings (backed by a `32`-entry color-chunk pool at `0x00843140`, RVAs `0x2CE8B0`/`0x2CEBF4`), causing >15 visible buildings to starve until camera movement evicted other buildings.
  - **Fix:** Removed the Remix-side persistence workaround and added `PatchFlareCulling(hSimJp)` synchronously in `patches/JPOG/asi/jpog_view_fix.c` to patch all 5 culling/truncation sites and expand the building flare pool (`15 -> 256`) and color-chunk pool (`32 -> 300`).
- **Frozen blinking fence flares & glowing electrified fence wires on stationary camera:** Static RE of `simjp.exe` (`ALensFlareGroupParticles::Update` `0x006BB9B0`, `ALensFlareParticles::Update` `0x006BD390`, `AElectricFenceParticles::Update`/`Render` `0x006B4C80`/`0x006B4D80`, `AWallManager::RenderScene` `0x005ED3E0`) and `TSysShaderD3D.dll`/`TRenderD3DInterface.dll` confirmed JPOG updates colors and rewrites dynamic vertex buffers unconditionally every frame — when the camera stops, positions/UVs/indices/transforms stay identical and **only `DIFFUSE` (`color0`) changes**. Under Remix Plus 2.0's `d3d9v2` frontend (`d3d9_frontend.cpp` + `RetainedScene`), `rules::FullGeometryHash` omitted vertex color, so stationary-camera frames hit cached meshes in `perDraws.meshes` (`d3d9_frontend.cpp:1186`), skipped `descriptionKey` updates (`d3d9_frontend.cpp:473`), and skipped `RetainedScene::acquireGeometry` (`rtx_retained_scene.cpp:1215`) — freezing both rendered flares/wires and `createEffectLight` updates.
  - **Fix:** Added `HashComponents::VertexColor` (`"colors"`) to `rules::VertexDataHash` / `rules::FullGeometryHash` (`rtx_hashing.h/.cpp`, `d3d9_rtx_geometry.cpp`, `d3d9_rtx.cpp`, `rtx_asset_replacer.cpp`, `rtx_remix_api.cpp`), leaving USD asset hashes (`geometryAssetHashRule`) unchanged and static-mesh memoization intact. In `SceneManager::processGeometryInfo`, when only `HashComponents::VertexColor` changes, `RtxGeometryUtils::cacheVertexDataOnGPU` refreshes the GPU vertex buffer in place with zero BVH rebuild. Also raised `SceneManager::createEffectLight`'s per-quad cap from hardcoded `128` to configurable `rtx.effectLightMaxPerQuad` (default `256`, range `1..1024`) under **Lighting → Effect Light**.
- **NUMOS night-time storm flares (`rtx.effectLightNightFlaresEnable`, `rtx.effectLightNightSunElevation`):** Static RE confirmed all 4 storm-only flare gates in `simjp.exe`: `AUnitBuilding::InitLensFlares` (`RVA 0x0C8246` & `0x0C8387`, buildings), `g_pGroupLensFlares` (`RVA 0x450038`, road lamps, speakers & static fence lights via `+0x4C/+0x50/+0x54`), `AUnitHelicopter::SetRender` (`RVA 0x0D044A`), and `AUnitVehicle::SetRender` (`RVA 0x0D61EA`, jeeps/cruisers/cleaners).
  - **Fix:** `RtxNumos::beginFrame` publishes `__numos.force_night_flares` (`"1"`/`"0"`) to `GameStateStore` whenever `rtx.effectLightNightFlaresEnable` is enabled and NUMOS sun elevation drops at or below `rtx.effectLightNightSunElevation` (default `0.0°`, exposed under **Lighting → Effect Light**). `jpog_view_fix.asi` polls `__numos.force_night_flares` every 15 frames via `remixapi_GetGameValue` in `Hook_WeatherUpdate`, drives `g_pGroupLensFlares` smooth fade (`0.2/s`), and routes all 4 weather checks through 36-byte read-only code-cave stubs checking `g_forceNightFlares`.
