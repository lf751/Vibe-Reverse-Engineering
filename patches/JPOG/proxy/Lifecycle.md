# Proxy lifecycle and session terrain tags

The proxy keeps D3D9 device state coherent with successful setters and invalidates
bindings, transform validity and shader classification on reset. Matrix register
validity is tracked so partial uploads after reset cannot reuse old matrix rows.

Vertex-buffer samples and upload pointers are private data on their owning
resources. They disappear with those resources and cannot survive pointer reuse.
Sample capture requires at least three floats and applies only to stream offset
zero. Short or partial uploads invalidate the sample. Shader classification reads
the live shader's GetFunction data when its binding changes; it has no creation
count limit or stale pointer table. The bridge serves GetFunction locally.

Texture hash cache entries are also resource-owned, and are valid for one frame.
This avoids fixed-slot collisions without caching stale generated terrain content
across frames. Particle hashing is skipped when no particle tags are configured.
Auto-tagged terrain hashes use a growable process-lifetime deduplication set.
The proxy sends `session:rtx.terrainTextures` through AddTextureHash: the matching
runtime merges the hash into its non-persistent Derived layer. Authored config
hashes are left intact. Existing hashes already saved in user.conf are not removed.

Backend initialization uses InitOnceExecuteOnce; a failed load/export lookup can
retry without calling a null function pointer. Sibling DLL paths are checked
against their buffer bounds. Allocation failures release the inner COM references.
The proxy is pinned for process lifetime because resource vtables contain hooks
into its code. Its particle declaration is released with the owning wrapper.

## Validation

`tests/lifecycle_test.c` includes the actual proxy device implementation and runs
CPU-only x86 regressions for reset, partial matrix uploads, rejected setters,
shader churn, resource-address reuse, short uploads and 200 session terrain hashes.
`tests/path_test.c` includes the actual loader and checks short-buffer rejection.
Compile with the runtime public include path, `/TC /O2 /Oi /GS- /Zl`, and link
`/NODEFAULTLIB /ENTRY:main kernel32.lib`. Put generated files in
`test-artifacts/JPOG/proxy-lifecycle`, not in the source folder.

The actual built DLL is also checked by the existing proxy-com-test fixture for
both bridge registration capabilities and resource-only device lifetime.
These CPU checks do not substitute for live save transitions, landscaping,
weather, or minimise/restore testing.
