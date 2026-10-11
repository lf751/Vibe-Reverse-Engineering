# Acquired JPOG terrain fields

Read-only acquisition on October 9, 2026 from the isolated game copy, process 7956.
All instrumentation sessions were detached afterward. No game binary, proxy, runtime,
Toolkit extension, or terrain preset was changed for this investigation.

## Live strata capture

- Terrain interface loaded at `0x015D0000`; singleton instance `0x03A13B80`.
- Heightfield extent: 1008 by 1008 game X/Z units, organized as 63 by 63 sections.
- Acquired 400 cached strata buffers, each containing a raw 17 by 17 byte array.
- Captured LOD counts: 340 at LOD 0, 45 at LOD 1, and 15 at LOD 3.
- Valid LOD samples: 102,040; observed byte range 1 through 253, with 236 distinct values.
- Buffer section-coordinate headers matched their section-array positions.
- Strata is generated from height and noise, and carried in the low byte of packed
  GPU vertex control data. It is a scalar coordinate, not three RGB material weights.

`capture/strata.json` contains addresses, coordinates, LODs, flags, and original bytes.
Its sibling `.bin` files preserve every raw buffer, including unused coarse-LOD
entries; only entries spaced by `1 << LOD` are established valid samples.

`capture/strata-valid-samples.png` displays only established valid samples.
`capture/strata-nearest-preview.png` reconstructs captured sections with nearest
valid samples for readability. This reconstruction is a visualization, not an
original game texture. Purple indicates missing data. The captured region is
1009 by 769 samples with origin X=0, Z=240; the entire island was not acquired.

## Other fields

`capture-with-shadows/fields.json` additionally contains 400 cached two-plane
shadow buffers. Both planes have 17 by 17 byte grids and separate raw `.bin` files.
Static analysis identifies these as terrain shading controls, not established
material paint masks. Grass control is four quadrant bits, not a smooth texture.

See `findings.md` and the saved decompilations for layout, generation, and packing
evidence. Scalar-to-material interpretation remains to be correlated with the game's
texture-stack lookup and screenshots. Fertility, mud, and path paint maps have not
been established by this investigation. No integration work has been performed.
