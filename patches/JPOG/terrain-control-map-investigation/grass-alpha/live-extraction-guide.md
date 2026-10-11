# Live extraction pointer guide

TTerrainInterface.dll preferred base0x10000000, runtime relocated: singleton pointer at module+0x12530 (already livevalidated previous investigation). singleton instance+0xD8 holds terrain shader; +0x500 holds grass shader (GetGrassShader RVA5D80). Grass shader+0x40 holds terraininterface (accessor TTerrainShaderD3D RVA119A0). Grass material+0x34 holds grassshader.

Terrain shader+0x204 lowend IDirect3DTexture8 COM pointer; +0x338 highend IDirect3DTexture8 COM pointer. These are not Toshi resources. Stage0 grassmaterial+0x40 is Toshi resource, with COM pointer resource+0x60.

Grass stage1 bind callsite TTerrainShaderD3D RVA13DEC: immediately before call ESP+0device,ESP+4stage1,ESP+8D3D8texture; ESIdevice,EAXtexture. GetTerrainTexture entry RVA10D60 ECXterrainshader, returns10D7B lowend or10D83 highend.

Lowend CreateLowEndTextures_TerrainSky RVA91E0 width/height are terrainshader+0x298, managedpool1, tries original D3D8 formats25,26,23,24,30. Highend CreateTerrainTextureRenderTargets RVAC180 hardcodes512x512 A8R8G8B8=21 defaultpool0 RENDERTARGET1.

The wrapper COM pointer requires its proxy implementation to obtain underlying D3D9 texture. Reading existing memory / proxy wrapper structures is sufficient to locate it; no game generation calls or source changes are needed. Static dimensions refer original DLL requests; wrapper may substitute formats, and captured512/1024 differences need liveverification.
