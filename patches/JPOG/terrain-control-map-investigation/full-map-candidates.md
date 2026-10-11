# Full-map grayscale reference candidates — 2026-10-09

This pass reviewed existing findings, saved decompilations and function indexes only. It did not attach to the game or change game/runtime/source files.

## What coverage establishes

The captured strata previews contain only resident cached TStrataFieldBuffer sections. Missing purple areas are acquisition gaps, not a property of the scalar field. A complete image could therefore be the same strata field assembled by sampling/generating every section, or by recording sections as they become resident. Full coverage alone does not identify a different control map. A different island/save, resampling and contrast mapping would also change appearance.

## Candidate comparison

| Candidate | Evidence | Implication for reference image |
|---|---|---|
| Strata scalar | ComputeStrataBuffer reads terrain heights and APerlinNoise section texture, applies height thresholds, writes scalar bytes; confirmed in saved SimJPTerrain/compute_strata_ghidra.txt | Smooth noisy terrain bands and mountain clusters are compatible. Complete assembly is plausible; image identity remains unverified. |
| Grass exclusion control | Previous static findings identify ComputeGrassMask as four quadrant bits per section, influenced by steepness and section/UI states | This particular control would produce coarse discrete quadrant regions when expanded. It is a weak match for smooth noisy grayscale. This does not exclude a separate grass texture or higher-level map elsewhere. |
| Stored terrain height | THeightField and terrain height accessors appear in existing TTerrainInterface index; both strata and shading decompilations read cached heights | Height visualized as grayscale can show bright mountains. A clean geometry-height preview is needed to compare; no reference match established. |
| First shading plane | ComputeShadowBuffer writes +0x10 plane from terrain-normal lighting expression | Smooth terrain shading candidate, not an identified material/paint map. |
| Second shading plane | Same function writes +0x131 plane from vertical normal, height and shifted Perlin section bytes | Has a stronger height/noise appearance and could be confused with strata. Must acquire/show separately. |
| Perlin texture | Existing SimJPTerrain index names InitializeTexture RVA0x24A0, GetSectionTexture RVA0x3340, GetCurSectionTextureValue RVA0x28E0/0x2980 and GetNoiseTexture RVA0x93F0 | Confirmed procedural-noise input to strata/shading via decompilation; method names do not establish full-world texture dimensions or a match. Pure noise alone would not explain island silhouette and geometry-shaped mountains without combination/masking. |
| Baked terrain output | Existing TTerrainShaderD3D RenderToTexture RVA0x5E30 accepts terrain mesh and three byte-array parameters; saved decompilation contains software composition plus D3D render-target path | A rendered/baked texture or isolated channel is possible, but existing evidence does not establish a single whole-island grayscale output. Do not label reference as baked map without texture dimensions/channel identity. |

## Limits and next evidence

The reference has no filename, address, texture format, channel or acquisition method, so it cannot be positively classified visually. Compare separately labeled height, strata, shading A and shading B on matching coordinates; retain capture coverage as a separate overlay. For a full-map comparison, prefer reading already-existing full height data or accumulating resident section samples rather than calling generation/cache routines during this read-only investigation. Camera movement can change LOD/residency, so retain coordinates and valid-sample stride.

No newly confirmed binary layout or function purpose was introduced in this pass; KB files were unchanged.
