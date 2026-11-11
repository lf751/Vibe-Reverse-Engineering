# Remix Plus dev update - 2026-11-11

User requested the complete latest dev update and deployment to the installed game, plus a check for tone-mapping configurations.

## Source

- Upstream dev: `f9e5f857763e82e134e2c44138cec7a0ae710d91` (18 incoming commits).
- Local merge: `5ac1225161d81414b0b6e31b435a321e649add4d`, fast-forwarded into `JPOGRemix`. No push.
- Build checkout: `D:\Unity\feat-20261030-jpog-terrain`.
- Source recovery bundle: `backups/JPOG/2026-11-11_dev-update-source/runtime-before.bundle`.
- Documentation conflicts retained custom bridge hash support, both changelog histories and generated options. Code auto-merges were reviewed around terrain prewarming and lighting variants.
- No dependency or bridge code changes. Existing bridge client/server/launcher hashes match source outputs.

## Validation

- `scripts/build.ps1 -Jobs 4 -MesonArgs '-Dbuild_tag=JPOG-Dev-20261111'` passed.
- Shader compression remains enabled. Runtime DLL: 331297280 bytes.
- Dedicated `_Comp64UnitTest`: 31 passed, 0 failed.
- Reused 974 release SPIR-V binaries after verifying compiler-option equivalence except output paths/header compression, regenerated raw test headers and checked every binary hash. Dedicated test C++ and runtime were rebuilt.
- Unit build initially used two workers while release compilation overlapped. Its own process tree was stopped and resumed with four workers after release finished; final passing results are in `unit-tests-final.log`.
- `git diff --check` passed. Upstream dev is an ancestor of the integrated commit.

## Installed deployment

- Target: `D:\Games\JPOGRTX\JPOG`.
- Complete shipping runtime package: 228 files verified, including the retained local neural DLL override.
- Runtime SHA256: `E31A233405C6C563D47494C443DE00C9E86DFB6A3FE5AEBD8D9EAD024061E656`.
- Patched neural DLL SHA256 preserved: `CDFA6C1EC784867182164C7836F324F087B5C9F374D0C9FE46F54999FEABF3ED`.
- Recovery: `backups/JPOG/2026-11-11_180801_dev-installed-runtime` (complete previous .trex, backup hashes verified before copying).
- Existing rtx.conf, user.conf, Options.ini, dxwrapper.ini, weather profiles, bridge configuration/binaries, root proxy and ASI hashes preserved.
- Used a runtime-only deployment script, avoiding the older share script's automatic weather migration because the installed game now uses different weather profiles.
- Game was closed for deployment. No game launch/GPU test performed; gameplay and flicker improvement remain unverified.
- Reports, deployment manifest and build logs: `test-artifacts/JPOG/dev-update-20261111/`.

## Tone mapping

No separate tone-mapping/color-grade `.conf` or `.cube` presets exist in upstream dev or the runtime output package. The DLL already contains `None`, `Warm Amber`, and `Olive Green`, selectable in Post-Processing > Tonemapping. Custom grade files load from `rtx-remix/colorgrade` by default; reserved built-in names cannot be overridden by files. Installed `rtx.tonemap.gradePreset = None` was preserved; no grade or exposure setting was changed.
