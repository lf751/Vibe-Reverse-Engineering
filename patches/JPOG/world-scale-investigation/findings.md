## Native length units — 2026-11-03

### Result

The native safari path coordinate system uses one coordinate unit per metre. With Remix getMeterToWorldUnitScale=100*sceneScale, the corresponding sceneScale is0.01 if rendered world coordinates preserve native coordinates. This is strong native-unit evidence, not a visual calibration of every rendered asset.

### Exact binary evidence

Analyzed installed simjp.exe, PE base0x400000. Exported decorated names establish semantics:

- 0x00628BF0 `?GetLengthInMeters@ASafariPath@@QAEMXZ`.
- 0x0061D610 `?GetPathLengthInMeters@ASafariNetwork@@QAEMXZ`.
- Import slot0x00739BCC resolves `?Distance@TVector2@Toshi@@SGMABV12@0@Z`.

ASafariPath::GetLengthInMeters initializes a float accumulator from VA0xD3D0E4 (verified0.0). Loop0x628C20 obtains pointers to consecutive node-position vectors at node+0xC, calls TVector2::Distance at0x628C2B, adds the returned distance to the accumulator at0x628C31-0x628C41, advances the linked list, and returns the accumulator directly at0x628C5F-0x628C66. There is no metre conversion multiplier before/after the sum.

ASafariNetwork::GetPathLengthInMeters independently initializes from VA0xD3A764 (verified0.0). Loop0x61D640 calls the same Distance import at0x61D64E on consecutive positions at node+0x14, adds raw distances, and returns the sum directly at0x61D683-0x61D68A. Again no conversion multiplier.

These names and exact arithmetic are substantially stronger than estimating scale from dinosaur appearance or assuming metadata Size is geometric length.

### Corroborating data and boundary

Data/Constant.ini JeepCostPerMeter=.2 and MaxSafariPathLength=800 agree with metre-labelled native safari distances. Dinosaur and staff traits look plausible in metres but are supporting context, not the derivation.

Existing fence findings identify RenderModel005EDB80 constructing worldmatrix8502E0 from wall-node position floats+0C/+10/+14. Terrain findings use raw node/section coordinates and render/camera world coordinates consistently. An exact simulation-to-D3D world translation trace was not newly exercised here. No live attachment, game launch, configuration edits, source or binary patches performed.

### Recommended scope of claim

Native simulation path units: one metre, proven by exported meter-named functions directly summing raw coordinate distances. Recommended matching Remix sceneScale0.01, conditional only on native-to-render world coordinates staying at1:1. Do not claim that model authoring files, texture scale, camera distance, or per-asset transforms all use identical units without checking their import transforms.

### Safari coordinate to terrain bridge

ASafariPoint constructor0061EE10 calls setter0061EE70. At0061EE88-0061EE90 the setter copies input TVector2 components directly to owner+14/+18, without scale. These are the positions measured by ASafariNetwork::GetPathLengthInMeters. Setter then rounds raw coordinates to integers and calls terrain invalidation0061EECD=>005DBE40 using x±12,z±12. Update0061F2C0 similarly reads owner+14/+18 and passes their rounded coordinates into ASimJPTerrain::SetUIFlags via IAT007395A0. Therefore these metre-defined safari positions share native terrain coordinates; they are not an independently scaled display distance. Existing fence RenderModel findings bridge native positions to worldmatrix8502E0. No complete safari decal-to-D3D render chain was newly traced.

### Original geometry and existing capture cross-check

Read the HENDRIX-ZT2/jpog-blender importer at commit231a7886bc0c0b1e535b3633e704a2f4f71668da. It creates Blender mesh vertices directly from TMD position floats (import_tmd.py:192); its skeleton correction matrices are rotations, not a general unit multiplier. The repository has no standalone license file, so the add-on was inspected only and was not installed, executed, modified or redistributed. An independent little-endian TMD record reader measured native bind-pose vertices without importing the add-on.

Native model bounds: VisMNeut Z-height1.94952; VisFneut Z-height1.75082; Ranger Z-height1.98314; Cleaner Z-height1.87500; jeep longest extent4.93432; TRex_hi longest extent13.95951. These are coordinate units, consistent with the independently established native metre coordinate system. They are corroboration, not assertions about real species dimensions or animated poses. Evidence/scripts: test-artifacts/JPOG/world-scale/native-model-measurements.json and measure_native_models.py.

Current Remix source copies vertex positions unchanged into capture buffers (rtx_game_capturer.cpp:777-783) and exports those buffers directly to USD points (src/lssusd/game_exporter.cpp:653-656). Coordinate corrections rebase origins and flip handedness; no sceneScale multiplier is applied to those positions. The custom proxy forwards WORLD matrices unchanged at d3d9_device.c:1158-1165, or reconstructs them from MV*inverse(View), without adding a global scene unit factor.

Capture metadata is separate: rtx_game_capturer.cpp:1184 selects rtx.capture.metersPerUnit when positive, otherwise rtx.sceneScale; game_exporter.cpp:279 writes that value to USD metadata. Four existing September13/15/23 capture root layers all have metersPerUnit0.10000000149 and Y-up. The September23 capture has743 mesh prims;738 have unit-length world transform bases, and5 have only small ~1percent deviations. No human model was positively identified in this capture, so do not claim a captured visitor height was measured. Records: capture-unit-metadata.json, capture-mesh-dimensions.json and capture-inspection.log in the same evidence directory.

Recommended physical runtime scale remains rtx.sceneScale=0.01 (100*0.01=1 native unit/metre). For physically accurate future USD capture metadata, the separate rtx.capture.metersPerUnit should be1.0. Its automatic fallback is not that conversion. Existing USD importers may honor the stored0.1 metadata and rescale their display, so compare raw coordinates and transforms rather than blindly reading a displayed metric dimension. No game, mod or capture file was changed; no new game launch was performed.
