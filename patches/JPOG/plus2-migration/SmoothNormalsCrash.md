# Smooth-normal crash on the first 2.0 scene load

The isolated run on 2026-10-30 reached terrain auto-tagging and bound the live
grass coverage mask, then crashed in the x64 runtime. Matching build symbols
resolve exception RVA `0x586f5b` to `dxvk::smoothNormalsAccumulate+0x11b`,
`smooth_normals.h:185`, loading the second vertex position.

The exception context contains these smoothing arguments:

- 9 vertices, 17 triangles; position offset 0 and stride 28 bytes.
- 16-bit indices, index offset 0; hash table size 256.
- The failing second vertex index is 19,711, outside the 9-vertex domain.

The CPU dispatcher read original `RasterGeometry` buffers while configuring
index width and triangle count from converted `RaytraceGeometry`. Those
representations can differ in topology, index format and lifetime. In particular,
a 19-index strip/fan expands to 51 triangle-list indices; the original stream
cannot be read as that expanded list.

Runtime commit `f07264ab0` makes both execution paths consume the converted
ray-tracing buffers. CPU execution requires those same buffers to be mapped and
free of pending GPU writes. The original-raster argument is removed from the
dispatcher and both callers. Smoothing remains enabled and the shared algorithm
is unchanged.

The regression exercises 9 vertices and 17 triangles, fan conversion from
32-bit raster indices to a rebased 16-bit triangle list, an invalid original
index, padded vertex stride and nonzero buffer offsets. It checks resulting
normals using the production conversion and smoothing functions.

Evidence, including the dump and original matching DLL/PDB, is retained in
`test-artifacts/JPOG/plus2-migration/crash-2026-10-30_2213/`.
The focused regression, corrected release build and fresh full unit run pass
(29/29 tests). The clean committed release was relinked and deployed to the
isolated game: 225 files hash-verified, with 2817 existing files accounted for
and unmapped files unchanged. Weather migration did not rerun; authored profiles
were preserved. Recovery backup:
`backups/JPOG/2026-10-30_224109_sharing-release`.

The same save now loads and remains running. The active runtime log has no
errors through the live checks. Off, Raymarched POM and Quadtree POM were
hot-reloaded in sequence, then the original preset was restored byte for byte.
The user confirmed the terrain looks correct, smooth normals refresh after a
height edit, and dirt/grass exclusion follows an extended path.

Automated window capture was unavailable (native pipe error 2), so visual and
editing results are user-confirmed. Runtime stability, live mask binding and
preset restoration are independently checked. Evidence is under
`test-artifacts/JPOG/plus2-migration/smooth-fix-live-confirmed/`, with mode results
in `live-pom-results.json` beside it. The user subsequently requested deployment
of this tested correction to `D:/Games/JPOGRTX/JPOG`; 239 files were verified and
that game was left closed for their test. The Desktop release folder remains
on its prior build.
