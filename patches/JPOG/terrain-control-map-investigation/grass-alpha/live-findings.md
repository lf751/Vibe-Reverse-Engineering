# Live grass coverage verification

Read-only capture from the user's test game on 9 October 2026, process 22972.
The original D3D8 grass renderer returns the terrain texture at shader +0x204.
The active texture was 0x0FE95B20 before and after the user's path edit.

The texture is 1024 by 1024, managed pool, A1R5G5B5, pitch 2048. Captures used
a read-only level-zero lock on the rendering thread and detached afterward.
`live-before/texture.bin` and `live-after-path/texture.bin` preserve raw surface bytes.
The PNG previews extract only the single alpha bit, with white meaning coverage.

The new path changed 580 unique alpha texels, all white to black, within X 503-511,
Z 648-719. This confirms path edits update the same live full-island mask.
The texture also contains existing buildings and other grass exclusion areas.
The alpha does not encode the reason for exclusion or separate material weights.

The current runtime's worldSpaceSecondaryOpacityTextures setting identifies the
grass primary hash BF84A6E3285BBB20. Its secondary alpha is sampled at world XZ/1024.
Terrain coverage sand can consume that same resource without importing a static map.
