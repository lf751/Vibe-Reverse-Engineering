
## Terrain control fields - static investigation 2026-10-09

Three game DLLs analyzed with bootstrap, r2ghidra, and saved Ghidra projects. No game, proxy, runtime, or Toolkit files changed. Analysis dependencies installed in repository tooling environment; pyghidra 2.2.1 plus JPype1 1.7.1 runs with local JDK21 (setup originally installed incompatible pyghidra3 requiring Ghidra12).

### Confirmed control field

TTerrainInterface manages a TStrataFieldBuffer cache, while ASimJPTerrain in SimJPTerrain.dll provides the virtual implementation that fills it. This is a SINGLE scalar material/stratum coordinate, not three independent RGB material weights. A field contains 17 by17 one-byte samples covering a 16 by16 heightfield-cell section, with shared border vertices. The coordinate is generated from the terrain's height thresholds plus Perlin noise, then multiplied by253.0 and truncated to a byte. The default base-class implementation merely clears the field.

### Addresses (preferred image base 0x10000000; rebase all DLL addresses)

| DLL | RVA | Function |
|---|---|---|
| TTerrainInterface | 0x12530 | global terrain-interface singleton pointer |
| TTerrainInterface | 0x5DD0 | GetSection(x,z) |
| TTerrainInterface | 0xA8A0 | CacheStrataBuffer(x,z,section) |
| TTerrainInterface | 0x6140 | section.GetStrataBuffer |
| TTerrainInterface | 0x6130 | section.GetShadowBuffer |
| TTerrainInterface | 0x60F0 | section.GetPos(xOut,zOut) |
| SimJPTerrain | 0x17820 | ComputeStrataBuffer(buffer,x,z,lod,previousLod) |
| SimJPTerrain | 0x17AF0 | ComputeShadowBuffer(buffer,x,z,lod,previousLod) |
| SimJPTerrain | 0x17D60 | ComputeGrassMask(section) |
| SimJPTerrain | 0x17300 | Precache - packs control bytes into GPU vertices |
| SimJPTerrain | 0x1B960 | ComputeStrataPoint(x,z) |
| TTerrainShaderD3D | 0x5E30 | RenderToTexture(mesh,flags,three byte arrays) |

All are x86 thiscall: this in ECX, stack arguments after return address. ComputeStrataBuffer has five stack args and ret0x14; CacheStrataBuffer three args ret0xC. Return buffer pointer in EAX.

### Layouts

Terrain-interface object: +0x28 heightfield pointer; +0x30 section-array pointer. Heightfield +0x2C section count X, +0x30 count Z, +0x38 field size X, +0x3C size Z. Section stride0x50; address = sectionArray+0x50*(x+countX*z). Section +0x18 flags (LOD bits0xC00>>10), +0x1C UI flags, +0x24 strata pointer, +0x28 shadow pointer, +0x32/+0x33 byte section coordinates.

Strata buffer: +0/+4 linked-list pointers; +8/+9 byte section X/Z; +0xA byte current LOD; +0xC owner-section pointer; +0x10 289 bytes row-major stride17; allocation0x134. Valid samples use step1<<LOD; unsampled entries at coarse LOD may be stale or uninitialized. Capture LOD and build previews using only valid samples, not the entire raw289 bytes as valid data.

Shadow buffer: one289-byte field begins+0x10 and second begins+0x131. First derives from terrain normals and a fixed sunlight vector; second derives from vertical normal component, terrain height, and shifted Perlin noise. These are shading controls, not established paint/material masks. The grass mask is a FOUR-BIT quadrant exclusion/control mask, using section steepness and UI flags/building/road-like states; not a smooth texture-weight map.

### Mapping and consumption

Integer height samples correspond directly to game X/Z coordinates: float GetHeight truncates input coordinates directly, then interpolates adjacent samples. Each terrain section advances16 X/Z units. World Y is NEGATIVE stored terrain height (GetHeight(TVector3&) uses x87 fchs before writing Y). Stored height reconstruction is section-base float + signed-byte offset *0.125. Read global heightfield size for map dimensions; avoid assuming all border vertices exist at positive field endpoint.

Precache obtains raw17x17 signed height offsets, strata field and both shadow fields, then writes packed32-bit GPU vertex data through the remap table:

```
shadowA <<24 | shadowB <<16 | (signedHeightOffset+128)<<8 | strata
```

Thus the strata scalar is carried in the LOW BYTE of the packed GPU control value. This confirms there is useful material-control data before final terrain shading. It does not establish that the current low-end rendering route binds a standalone splat texture: RenderToTexture also accepts the three byte-array inputs and has render-target baking logic. Precise scalar-to-grass/sand/rock mapping still needs correlating the captured values with texture-stack lookup/shaders.

No exported symbols or meaningful strings mentioning fertility or mud appeared in these two terrain DLLs. This is not proof such controls do not exist elsewhere in the game.

### Evidence artifacts

- SimJPTerrain/compute_strata_ghidra.txt
- SimJPTerrain/compute_shadow_ghidra.txt
- SimJPTerrain/precache_ghidra.txt
- TTerrainShaderD3D/render_to_texture_ghidra.txt
- TTerrainShaderD3D/load_volume_textures_ghidra.txt
- Per-DLL bootstrap reports, backed-up KB additions and Ghidra projects.

### Suggested live verification

Read singleton, heightfield dimensions, sectionarray and active strata pointers without calling generation/cache methods. Export bytes plus coords and LOD. Optionally observe ComputeStrataBuffer entry/exit to capture only completed valid outputs during camera motion, without modifying args/returns. Export both shadow planes separately and grass quadrant flags under distinct labels. Compare to screenshots before assigning material interpretations. No integration into blend plugin until user has reviewed acquisitions.

Ghidra KB application attempted after first decompilation and target functions re-decompiled. Exported indexes now contain264 SimJPTerrain functions/6508 xrefs and230 TTerrainInterface functions/2492 xrefs. The KB parser skipped __thiscall prototypes and multiline struct start/end, so original Ghidra export-derived names/prototypes remain; do not interpret kb-apply counts as successful full type application. This does not affect disassembly-confirmed layouts above.
