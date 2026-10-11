# Smooth terrain normals

Toolkit 3.1.0 adds Smooth terrain normals to Window > Terrain Blend. The optional
`remix:terrainBlend:smoothNormals` bool defaults to false. The deployed preset
enables it. Material normal maps, signed displacement, and self-shadowing remain
enabled independently.

The SceneManager routes tagged terrain through Remix's normal generator. A change
in smoothing state bypasses cached instance reuse and updates the normal buffer.
Disabling smoothing restores the input normals unless the mesh independently has
the standard Smooth Normals category. Geometry changes use the existing update
path to regenerate normals. Smoothing can change slope-based layer selection and
triplanar texture projection. Independently submitted meshes still have independent
normal generation, so cross-chunk continuity is not guaranteed.

Live validation first exposed triangular cliff patches. Those persisted with
displacement disabled and slope-based Rock disabled. The existing generator had
two defects on regular terrain grids: poorly mixed hashes exhausted a 128-entry
probe limit, and 32-bit position tags merged unrelated vertices. The generator now
uses well-mixed hashes, full-capacity probing, and exact position identity through
an immutable representative vertex. The scratch table remains four integers per
entry. Converted strip/fan indices use the cached triangle-list GPU path rather
than being misinterpreted by the small-mesh CPU path.

The release build and `test_smooth_normals` passed. Regressions cover a steep
17-by-17 grid, formerly aliased positions, duplicate positions and signed zero,
a 160-entry deliberate hash collision chain, and strip/fan conversion. Toolkit
build, 25 headless tests, formatting, lint, and whitespace checks passed.

The corrected isolated game rendered the edited terrain without the previously
visible triangular patches. `smooth-fixed-on.png` and `smooth-fixed-off.png` record
the live toggle comparison. This is a rendering smoke check, not a performance
benchmark or exhaustive cross-chunk validation. No cliff geometry or Rock layer
was removed. The original faulty captures are retained as diagnostic evidence.

The user confirmed that the sharp triangular patches are gone with smoothing
enabled. The corrected runtime and enabled preset were deployed to
`D:\Games\JPOGRTX\JPOG`; all 191 runtime files were hash-verified. Both deployment
manifests record backups and retained integration files. The isolated game's
day/night cycle was restored to enabled after the comparison.

Build and test logs use the `smooth-` prefix.
