# DLSS-NR compatibility investigation

Date: 2026-11-03

## Result

Neural rendering failed before feature creation in the installed 2.0-derived runtime. The patched DLL is present in the correct runtime directory. The direct-library compatibility implementation is now verified in the isolated game. The user confirmed "yes it works" after the requested enabled/disabled comparison. Installed files and SDK files remain unchanged.

## Observed evidence

Installed `rtx-remix/logs/remix-dxvk.log` reports `DLSSNR.Available=0` and `NVSDK_NGX_Result_FAIL_FeatureNotFound` (0xBAD00004). The reported DLL is `.trex/nvngx_dlssnr.dll`, version 310.9.16.0.

A standalone x64 Vulkan/NGX probe using the same SDK static library and application ID 102100511 reproduced this with verbose NGX logging. On the final probe run the Vulkan device enabled supported NVX binary import, NVX image view handle, push descriptor extensions and supported Vulkan 1.2 features.

Patched DLL results:

- SHA256: CDFA6C1EC784867182164C7836F324F087B5C9F374D0C9FE46F54999FEABF3ED
- Size: 276162560 bytes.
- NGX logs `nvLoadSignedLibraryW() failed` and `Querying GPU Architecture Failed`, followed by `unable to load DLL metadata via FileVersionInfo`.
- Core initialization and capability queries succeed, but DLSS-NR is unavailable with feature initialization 0xBAD00004.
- Ordinary Windows `LoadLibraryW` succeeds. Calling the exported Vulkan `Init_Ext2` directly returns 0xBAD00002 (`FAIL_PlatformError`). No hooks, caller spoofing or DLL modifications were used.

Stock build-output DLL results:

- SHA256: 2BC19BC09A7B89DD44F73E2AC0108FCC586E2154BDD0830F242F4F040E28789F
- Size: 266424944 bytes; version 310.9.16.0.
- NGX signed loading succeeds.
- NGX logs `GPU architecture : 0x190, Snippet expects at least : 0x1B0`, then rejects it as requiring a newer GPU architecture.
- DLSS-NR is unavailable with feature initialization 0xBAD00001 (`FAIL_FeatureNotSupported`). Direct initialization returns 0xBAD00002.

The first exploratory probe omitted NVX device extensions and produced unrelated DLSS initialization errors. Those errors were corrected before the final captures above; they are not evidence about Remix.

## Source comparison

Reference 9-19 commit 717b94bc9 contains a direct snippet backend and an optional caller-check bypass. The 2.0 commit 268fb5b24 removes that backend. Current runtime source uses the NGX capability query to gate both pass support and context creation. Consequently the patched DLL cannot reach rendering through the current path.

The direct-call PlatformError establishes that simply restoring direct loading is insufficient in the standalone test. Its internal cause was not proven; no claim is made that changing one check would produce correct neural rendering.

## Implementation constraint

Runtime AGENTS.md requires checking dependency licences and stopping when they forbid the proposed plan. `external/ngx_sdk_dlnr/LICENSE.txt`, section 4(d), prohibits bypassing technical limitations, encryption, security, DRM or authentication mechanisms. Section 4(a) restricts reverse engineering. Further binary analysis was stopped after reading this licence. No bypass implementation was made.

The user subsequently confirmed permission covering this integration and specified personal use. Work resumed on a local opt-in direct-library backend. The patched DLL is not added to source control or substituted into the shipping build output.

## Authorised compatibility test

With caller-origin compatibility handling, the patched DLL's direct initialization returns success (0x00000001). A standalone Vulkan test then successfully creates a 512x512 DLSS-NR feature, submits its initialization commands, waits for GPU completion, releases the feature and shuts down the DLL. This is in `patched-create-probe.log`; it establishes that the direct path can create features on this GPU, not visual image improvement.

The implementation adds `rtx.neuralUplift.useDirectLibrary` (default false), exposed as Direct DLL Compatibility in the neural-rendering menu. Independent passes share the DLL session and own separate feature handles. Backend changes recreate the features, and the DLL session is shut down after its last handle is released. The normal NGX backend remains the default.

## Integrated validation

- Release build passed; dedicated unit suite: 31 passed, 0 failed.
- Build tag: `JPOG-NeuralDirect-20261103`.
- Runtime SHA256: 6EEA785210D93814C3D73787FF6E64FC1C134B2BEB05C12769AB793D19C3E555.
- All 233 shipping/integration files were verified during isolated deployment, followed by the documented local patched-DLL override (hash above).
- Six selected installed integration/configuration files were hash-verified unchanged.
- Isolated log at 14:57:28 reports direct initialization success, a created 1920x1080 feature and the first successful direct evaluation.
- User visual confirmation: "yes it works".
- Full isolated log snapshot: `test-artifacts/JPOG/neural-rendering/verified-gameplay.log`.
- Validation and deployment recovery paths: `test-artifacts/JPOG/neural-rendering/validation.json`.
- Two independent feature handles also passed standalone creation/release/shutdown testing in `patched-multipass-probe.log`.

The user subsequently requested installed deployment. The tested runtime was deployed to `D:/Games/JPOGRTX/JPOG`, with 233 shipping/integration files verified before restoring the user's patched DLSS-NR DLL. Its SHA256 remains CDFA6C1EC784867182164C7836F324F087B5C9F374D0C9FE46F54999FEABF3ED. Direct DLL Compatibility is enabled in `user.conf`; the user's neural-rendering enable setting is preserved. Eight other configuration/integration files were hash-verified unchanged. No installed game launch was performed.

Installed recovery copies: `backups/JPOG/2026-11-03_192754_neural-direct-installed-runtime` and `backups/JPOG/2026-11-03_192754_neural-direct-installed-local`. The installed validation report is `test-artifacts/JPOG/neural-rendering/installed-deployment/validation.json`.

## Artifacts

Local evidence is under `test-artifacts/JPOG/neural-rendering/`:

- `probe.cpp`, `build-probe.cmd`, `probe.exe`
- `patched-probe.log`, `stock-probe.log`
- `dll-metadata-comparison.md`

GPU tests and the isolated game launch were explicitly authorised by the user.
