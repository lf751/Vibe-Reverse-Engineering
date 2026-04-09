# JPOG Fresh — Architecture Notes

## Engine
- **Toshi Engine** by Blue Tongue Entertainment
- D3D8 FFP rendering (no D3D9 native)
- DLL chain: simjp.exe → TRenderD3DInterface.dll (D3D8) → d3d8.dll (dxwrapper D3D8→D3D9) → d3d9_remix.dll

## Camera/Matrix Pipeline
- **VIEW = IDENTITY always** — camera baked into per-object World matrices
- `TRenderContext+0x8C` = WorldView matrix (the REAL camera view, world→view)
- `TRenderContext+0x4C` = ModelView matrix (View × Model, sent as D3DTS_WORLD)
- `TRenderContext+0x484` = Pure perspective projection (no view component)
- `TRenderContext+0x10C` = ViewWorld (inverse camera, lazy computed)
- Camera feed: ACamera → TCameraViewportHandler::OnBeginRenderEvent → InvertOrthogonal → SetWorldViewMatrix

## Key DLL Exports
- `TRenderContextD3D::SetRenderMatrices` (0x10007180 in TRenderD3DInterface.dll) — sets VIEW=IDENTITY
- `TRenderContext::GetWorldViewMatrix` → +0x8C (actual camera)
- `TRenderContext::GetModelViewMatrix` → +0x4C (combined V×M)

## Camera Classes (simjp.exe)
- ACamera (base), AWorldCamera, AFollowCamera, AFreeCamera, ADebugCamera
- ATacMapCamera, AScopeCamera, AMiniGameFollowCamera
- ACameraManager, ACameraMove (transition system)

## RTX Remix Camera Fix
- Need to set D3DTS_VIEW = WorldView (+0x8C) instead of IDENTITY
- Need to un-bake view from each WORLD matrix: World = ViewWorld × ModelView
