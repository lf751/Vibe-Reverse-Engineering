# Sharing package verification — 2026-10-11

Target: `C:\Users\User\Desktop\JPOGLatestRTX`.

Release runtime build and installation passed. The JPOG proxy and ASI were
rebuilt from `patches/JPOG/proxy` and `patches/JPOG/asi`. Bridge server and
32-bit client/launcher builds passed. PE headers confirm x86 for the root
proxy, bridge client and launcher; x64 for the server and runtime.

The complete 191-file runtime output was overlaid and hash-verified before
excluding its three development artifacts (`d3d9.pdb`, `d3d9.lib`, `d3d9.exp`).
With integration files, bridge settings and the neural-rendering DLSS library,
the final shipping manifest contains 195 verified files. Existing game patches,
configuration, data and mod content were preserved. The deployment manifest
records the backup location and file hashes.

All 16 terrain texture files in `rtx-remix/mods/JPOGRTX/SubUSDs` are present and
match the working mod's ingested Terrain textures byte for byte. Ground.usda
uses relative paths pointing to these copies. Both Ground.usda and
vegetation.usda compose through mod.usd. The asset audit inspected every USD
layer, including binary layers: 260 unique per-layer asset references, no
missing or external file dependencies. MDL identifiers are runtime material
modules, not missing mod files. Smooth normals, displacement and self-shadowing
remain enabled in the packaged terrain preset.

The consolidated terrain runtime reduced d3d9.dll from 551,930,368 bytes
(526.4 MiB) to 388,189,184 bytes (370.2 MiB), a 29.7% reduction. Terrain blending
and displacement now use one terrain shader family; authored displacement
settings still select relief at runtime. Non-terrain variants remain separate.

Release compilation, shader validation, smooth-normal and hot-reload regressions
passed. The same build was tested in the isolated game on the mountain save:
displacement enabled, disabled, zero strength, and terrain blending disabled.
Disabling terrain restored the original ground material; zero displacement
strength matched disabled displacement. The authored preset and day/night cycle
were restored after testing. Captures use the `consolidation-` prefix.

The sharing folder was updated and all 195 shipping files reverified. All 16
terrain textures and packaged USD dependencies passed the repeated asset audit.
No fresh gameplay test was performed directly from the sharing folder, and no
controlled GPU performance benchmark was performed. Install by overlaying its
contents, including `.trex`, onto an existing JPOG installation.

Evidence: `release-share-*.log`, `release-share-deployment.json`, and
`release-asset-audit.json` beside this document. Development artifacts removed
from the sharing folder are retained in the backup.

Shader compression release update: the release build now enables
compress_embedded_shaders. Build and install passed; the DLL exactly matches the
compressed binary tested in the mountain save. It is 261,105,152 bytes (249.0 MiB),
a 32.7% reduction from the consolidated uncompressed build. All 195 shipping
files were hash-verified after deployment, and files outside the overlay were
preserved. Previous runtime backup: backups/2026-10-11_170521_sharing-release.
The user reported slightly slower loading, with normal rendering and gameplay.

