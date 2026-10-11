# Grass secondary alpha texture static investigation

Read-only source/binary analysis. No game files or runtime/source code changed.

## Confirmed distinction

GrassType1_128.dds and GrassType1low_128.dds are grass billboard cutout textures, not full-island coverage maps. TGrassShaderHAL::Create (TTerrainShaderD3D RVA 0x128B0) selects the regular or lowend original texture and assigns it to grass material +0x40. TGrassMaterialHAL::PreRender (RVA 0x13DA0) binds it to stage 0, and the result of TTerrainShaderHAL::GetTerrainTexture (RVA 0x10D60) to stage 1.

GetTerrainTexture returns terrain shader +0x338 on the highend route, or +0x204 on the lowend route. CreateTerrainTextureRenderTargets (RVA 0xC180) creates the highend texture at +0x338 as 512x512, one mip, usage RENDERTARGET, format A8R8G8B8 (21), default pool. Surface pointer +0x33C is RenderToTexture's render target. This is a strong candidate for the full-island alpha plane currently consumed by Remix's custom worldSpaceSecondaryOpacityTextures mechanism.

## Shader evidence

TGrassShaderHAL::Validate RVA0x12BA0 supplies pixel shader bytecode at RVA0x495E0. Shader is ps_1_1 with comments retaining TGrassShaderD3D.psh source name. Relevant instructions: tex t0; tex t1; mul r0,t0,t1. This initial multiply includes alpha. Subsequent lrp writes RGB only, and final mul writes alpha using the vertex fade. Therefore the highend original grass path consumes terrain texture alpha as well as billboard alpha.

TGrassShaderHAL::Flush RVA0x11F50 configures the lowend FFP stage1 RGB modulation but stage1 alpha SELECTARG1=CURRENT; the terrain texture alpha is not applied in that branch. This distinguishes original routes and does not contradict Remix explicitly sampling a secondary texture's alpha for a configured material.

## Tools and limits

Existing exported entry/string index was sufficient to select functions; focused r2ghidra decompilation confirmed them. verify_install.py required checks pass; optional Java check warns. Generic find_shader_bytecode.py reports no embedded shaders because this original game DLL calls D3D8 method offsets, whereas scanner expects D3D9; focused Validate decompilation successfully located bytecode instead.

No live game process was running during this analysis. Need view/extract alpha from a saved 512x512 A8R8G8B8 terrain texture or acquire the live stage1 texture. Billboard texture previews must be labeled separately. Alpha generation details and exact saved capture identity remain unverified here.
