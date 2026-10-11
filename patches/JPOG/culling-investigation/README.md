# JPOG extended-distance terrain cache patch

The native low/medium terrain mode can exhaust its terrain and grass caches
when a large draw distance exposes most of the island. Live tracing observed
failed terrain mesh allocation with the original pools. The first capacity
patch restored terrain in the user's affected level; grass needs its own pool.

The patch changes five allocation constants in `TTerrainInterface.dll`:

| Pool | Original | Patched |
| --- | ---: | ---: |
| Terrain mesh | 900 | 4096 |
| Material/strata | 400 | 4096 |
| Shadow | 400 | 4096 |
| Grass | 350 | 4096 |

Material/shadow counts require three shared/instance initialization constants.
Allocation and cleanup consume these same counts. The high-end initialization
branch is unchanged. Changes must be loaded before terrain creation.

Fog fractions are scaled by camera distance in the native weather code. That
linkage is separate from observed mesh-allocation failure. The patch leaves
camera, draw-distance, weather, save data, proxy, ASI, and Remix settings alone.

## Rebuild and restore

Run `python patches/JPOG/culling-investigation/build_cache_patch.py` from the
workspace. The builder uses the backed-up original DLL, checks its exact hash,
and verifies that every changed byte belongs to the five capacity constants.

Original SHA256:
`4961df57871f539525b95c41dc72001414e4acb23569c8df3c958e155e50e3a3`

Patched SHA256:
`ab112afdb3863ee47a90419250c89e71d6b486b995f5ee36cda2a84dcdfb53fd`

Close JPOG before running `restore_original.ps1` to restore the original DLL.

## Validation

Before patch: original mesh count900, material/shadow counters capped400,
GetFreeMesh returnedNULL in all8sampled attempts.

After terrain patch: live mesh capacity4096 and material/shadow capacity4096,
with3969material and3969shadow buffers retained. User confirmed terrain
restored and reported remaining grass gaps. Temporary camera/depth/culling
probes were removed before installation.

Grass patch installed and hash verified. After restart, live inspection found
all3969island sections retaining grass, strata, and shadow resources. The
captured view shows continuous terrain and grass. Native camera near1/far7000
and the original Constant.ini hash are unchanged. This live check used Remix;
a separate run without Remix was not performed.
Higher capacities use more memory and startup work. No performance benchmark
has been performed.
