# 2026-11-11 proxy and ASI lifecycle fixes

User approved the review fixes and requested automatically detected terrain hashes
remain temporary for the game session. Work is isolated from the installed game
and December BaseMod release pending live validation.

- Proxy reset-state invalidation, successful-setter shadow updates and partial
  matrix validity tracking.
- Resource-owned vertex samples and per-frame texture hashes; shader identity from
  the live shader rather than a bounded pointer cache.
- Growable session terrain hash deduplication with authored tags preserved.
- Bounded loader paths, synchronized backend initialization, allocation cleanup,
  particle declaration cleanup and process-lifetime proxy pinning.
- ASI signature validation and native camera-context destructor hook so save
  transitions can reacquire the camera. ASI remains pinned while hooks are live.
- Runtime AddTextureHash/RemoveTextureHash `session:` prefix targets Derived layer;
  no API table or bridge command format changes.

The first isolated menu run exposed broken ownership in the bridge's private data
factory. It stored shallow copies of owning heap pointers, and its read path made
another owning copy. The new resource-owned proxy metadata triggers this existing
bug. Bridge repair and a test against the real header are required before retest.
Windows recorded exception c0000005 in d3d9_remix.dll at offset1844b; local dump
simjp.exe.36728.dmp is retained outside the source tree.

Completed CPU checks so far: x86 ASI build and 14 camera assertions, proxy native
regressions, four actual-DLL COM scenarios, and 31 dedicated runtime unit tests
including session hash export isolation. Final live results belong below.

Source and package recovery copies are under root backups/JPOG; logs/manifests
are under test-artifacts/JPOG/proxy-lifecycle and asi-lifecycle.

## Final validation

- Bridge private-data ownership repaired and both bridge architectures rebuilt.
  Actual-header debug-CRT tests passed42 checks; the original COM replacement
  reproduction failed as expected. No exact PDB-resolved stack is claimed for
  the first isolated crash.
- Complete runtime and matching bridge/proxy/ASI staged in isolated game with
  verified hashes. Patched DLSS-NR DLL preserved. Installed game and December
  BaseMod package remain unchanged.
- Corrected isolated game loaded terrain, auto-tagged terrain, and set up particles.
  Asked user to load mountain save, change saves/restart, edit terrain and
  minimise/restore. User replied: "everything working normally".
- Final runtime source commit: d3489f275 (session tag API, export test, bridge fix).
- Captured live logs and machine-readable validation in
  test-artifacts/JPOG/proxy-lifecycle/verified-gameplay-logs and validation.json.
- No performance benchmark was performed; no measured frame-time gain is claimed.

## Installed deployment

User requested deployment after the isolated checks passed. Updated
D:/Games/JPOGRTX/JPOG with232 hash-verified runtime/bridge/proxy/ASI files;
all copied files matched the tested isolated build before deployment. Preserved
62 root/configuration/weather files, including bridge.conf and the patched
DLSS-NR DLL. Complete previous runtime and integration recovery copy:
backups/JPOG/2026-11-11_193254_proxy-lifecycle-installed.
Report: test-artifacts/JPOG/proxy-lifecycle/installed-deployment/validation.json.
Installed game was closed for deployment and was not launched afterward.
December BaseMod release remains unchanged.
