## Repeated SetVertexShader crash — 2026-10-31

Both save-load and restore crashes enter DxWrapper 1.9.8700.25 at RVA 0x154756 with the same invalid vertex shader handle 0xFFFFFFFF. The exact matching source identifies the containing function as d3d8to9 Direct3DDevice8::SetVertexShader (RVA 0x154710). This is a client-side crash before the underlying D3D9 SetVertexShader method is called.

### Evidence

- Official release tag v1.9.8700.25 resolves to d77702d55f1725eeae15a6466091db4b998d89b4. Its External/d3d8to9 gitlink resolves to 255338f698c8270b537f0a91a13f795f4f988250. Matching source is retained under test-artifacts/JPOG/plus2-regression-investigation/2026-10-31_dxwrapper-close/official-source, including licenses.
- Matching source External/d3d8to9/source/d3d8to9_device.cpp:1636-1661 implements SetVertexShader: low-bit handles are FVF, high-bit handles decode via `Handle << 1` and reinterpret as VertexShaderInfo*. The decoded pointer is dereferenced without consulting VertexShaderHandles. DeleteVertexShader below it does consult the handle set before decoding.
- Binary preferred base 0x10000000. Function RVA0x154710 loads EDI from [EBP+0xC], tests sign, and enters the negative branch at RVA0x154755. Fault at RVA0x154756 is `push dword ptr [edi+edi]`; next instructions would call the D3D9 vtable slot 0x170. This matches ShaderInfo->Shader in the exact source.
- Both exception contexts (not the later handler thread contexts) have EIP0x6C0E4756, EDI0xFFFFFFFF, EDX0xFFFFFFFF, EBP0x001AF9C8, ESP0x001AF9BC. Both exception records indicate read of 0xFFFFFFFF. Decoded pointer starts at 0xFFFFFFFE and the four-byte load crosses 0xFFFFFFFF.
- First dump simjp.exe.37524.dmp: ESI/device0x011B5528, EBX/terrain object0x037FE1E0. Second simjp.exe.13244.dmp: ESI/device0x0108F148, EBX/terrain object0x036E61E0.
- At [EBP+4] both frames return to terrain DLL RVA0xABBC. [EBP+8] is the device; [EBP+0xC] is 0xFFFFFFFF. This directly establishes the caller argument, independently of fault registers.
- Terrain DLL binary function RVA0xAB90: `mov edx,[ebx+0x35c]` at0xABAE, `push edx; push esi; call dword ptr [eax+0x130]` at0xABB4-0xABB6. The return address is0xABBC and slot0x130 is IDirect3DDevice8::SetVertexShader. Terrain object field+0x35C supplies the bad handle.

### Ownership and limits

The immediate invalid input belongs to the game terrain shader lifecycle: its stored handle field is 0xFFFFFFFF at a rendering call. DxWrapper/d3d8to9 additionally treats any high-bit input as a valid encoded pointer and thus crashes rather than returning invalid-call. This proves both the incoming bad handle and unchecked decode, not why terrain ended up holding that sentinel. It does not establish a use-after-free, shader compilation failure, reset loss, or a particular regression commit. A controlled previous-version A/B is the appropriate next check; inspect shader creation/reset result and writes to terrain field+0x35C only if needed afterward.

### Tool limits

verify_install passed required checks. Bootstrap was started per repository instructions but did not finish within the bounded source-assisted investigation and was interrupted. Ghidra analysis was attempted but installed pyghidra3.1 requires Ghidra12 while local Ghidra is11.4.3. No decompiler-derived claim is used here: exact pinned source, PE disassembly, and two offline exception records establish the reported function/call chain. No game launches, attachments, source patches, or deployment were performed by this analysis.

### Previous release source comparison

Official v1.8.8600.25 pins d3d8to9 commit6cdb8a82184898f1b9371e4c8412c2d33ebb7b51. Comparison with current255338f698c8270b537f0a91a13f795f4f988250 has NO differences under source/. Only d3d8to9.vcxproj and res/d3d8to9.rc differ (3 insertions,2 deletions). Therefore GetVertexShader, Reset, SetVertexShader and the rest of d3d8to9 source are identical between these official pinned versions. An observed A/B difference would need explanation in DxWrapper integration/configuration/build behavior or other release changes, not a direct edit of those d3d8to9 methods.

## Terrain shader sentinel lifecycle — 2026-10-31 follow-up

Reused existing populated TTterrainShaderD3D analysis project/index under patches/JPOG/terrain-control-map-investigation/TTerrainShaderD3D; its index lacks xrefs, so targeted PE disassembly and r2ghidra fallback were used. The analyzed game binary is test-artifacts/JPOG/terrain-blend/test-game/Shaders/D3D/TTerrainShaderD3D.dll. Addresses below are preferred VAs (base0x10000000); exported decorated symbols independently establish names.

### Exact owner

+0x35C is the bump/normal-simulation vertex shader handle of Toshi::TTerrainShaderHAL, not an output of GetVertexShader. Constructor0x10003F50 initializes it to0xFFFFFFFF (store0x100040AB). DoSingleTimeStep_Optimized0x1000AB90 reads it at0x1000ABAE and passes it to SetVertexShader at0x1000ABB6.

GenerateNormalInit0x1000B950 creates this shader (D3D8 CreateVertexShader vtable+0x12C, embedded bytecode0x10048AC0, outputthis+0x35C); then checks HRESULT and subsequently creates associated pixel shaders. Validate calls GenerateNormalInit at0x1000515C. This initial path can create the bump shader separately from reset-time CreateShaders.

### Reset asymmetry

- OnLostDevice0x1000A8B0 calls DestroyShaders at0x1000A8C2.
- DestroyShaders0x1000C700 handles main terrain shaders under IsHighEndTerrain, then independently handles water/bump shaders under IsHighEndWater. It passes this+0x35C to DelVertexShader at0x1000C758.
- DelVertexShader0x1000C3F0 skips handles already-1; otherwise calls D3D8 DeleteVertexShader at0x1000C40E and writes-1 at0x1000C414, regardless of delete HRESULT.
- OnResetDevice0x1000A910 tests TResource::IsValid. If valid, it calls CreateShaders at0x1000A979, then recreates terrain texture targets. It returns true without propagating shader creation status.
- CreateShaders0x1000C450 first tests IsCreated at0x1000C463. It then requires IsHighEndTerrain (vtable+0xCC) at0x1000C47A; false jumps directly to return0x1000C6F7. Inside that terrain branch, it additionally tests IsHighEndWater (vtable+0xD4) at0x1000C5A6. The bump VS CreateVertexShader call is0x1000C649, outputthis+0x35C; HRESULT is passed to TD3DAssert at0x1000C655 rather than controlling a branch or return value.
- Exact vtable entries/export names: +0xCC=>0x10010D50 IsHighEndTerrain; +0xD4=>0x10010D30 IsHighEndWater. Existing older JPOG KB labeling10D30 as low terrain is inaccurate; export verifies water.

Therefore high-end-water with low/medium-terrain is a concrete static lifecycle mismatch: initial GenerateNormalInit can create the bump shader, lost-device handling deletes and marks it-1, while reset-time CreateShaders skips its recreation because high-end-terrain is false. DoSingleTimeStep_Optimized subsequently submits-1 without validation. This is a candidate causal chain, not yet a live-proven chain: the crash dumps do not capture the full owner object/quality flags, and reset call ordering has not been traced in this analysis.

### Next live confirmation

Trace OnLostDevice(A8B0), OnResetDevice(A910), CreateShaders(C450), and the bump create returnC64F; record ECX owner and flag accessor results, owner+35C before/after, HRESULT atC64F. Break at DelVertexShader storeC414 only when EDI equals owner+35C to establish the writer. Observe whether reset enters CreateShaders yet jumpsC485=>C6F7 with IsHighEndTerrain=false, followed by DoSingleTimeStep_Optimized(AB90) with handle-1. This distinguishes skipped recreation from a CreateVertexShader failure. No guard or source fix is justified until that branch/flag sequence is confirmed.

## Quality-1 water shader reset repair — 2026-10-31

The user confirmed TerrainQuality=2 avoids the closure, but TerrainQuality=1 is required for the mod. WaterReflection=true is retained; the user identified that option as the water-shader control. The historical 9-19 runtime worked with quality1, so this remains a compatibility regression even though the repair belongs in the game lifecycle.

Live trace: test-artifacts/JPOG/plus2-regression-investigation/quality1-reset-trace.jsonl. For owner0x036AB460, high-terrain/high-water bytes were [0,1]. OnLostDevice entered with bump handle2280577900; OnResetDevice and its CreateShaders call entered with4294967295. The static low-terrain early return explains why this shader is not recreated. The trace is from the unpatched isolated game with quality1 and reflections enabled.

Added patches/JPOG/asi/terrain_water_reset.c: signature-checked detour of CreateShaders, calls native code first, then recreates missing water resources only for created low-terrain/high-water owners. It uses the native bytecode/declarations and the owning renderer's D3D8 device. Each live handle is preserved; a newly created handle is published only on successful HRESULT. No terrain quality or reflection setting is forced. It does not change the native quality2 path or swallow invalid shader draws.

Build passed. x86 CPU mock lifecycle tests in tests/test_jpog_terrain_water_reset.c passed: full reset recreation, repeated callbacks, partially live resources, failed HRESULT without publishing the output, high-terrain exclusion, water-off exclusion, and uncreated owner exclusion. These tests do not prove live gameplay stability.

Isolated ASI deployment SHA256:9FB788152EF6B6979B2154B66BEDA303A1A1A84D71ECB20F700968644E50AA55. Installed game unchanged. Backups:backups/JPOG/2026-10-31_191743_q1-water-reset-fix. Runtime remains correctedC853. TerrainQuality=1 and WaterReflection=true verified before launch. Each attempted shader recreation records offset,HRESULT,handle in isolated jpog_terrain_water_reset.log. User save-load/minimise/restore validation pending. The change that exposed the mismatch after9-19 has not been established.
