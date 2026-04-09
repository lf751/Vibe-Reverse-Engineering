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
