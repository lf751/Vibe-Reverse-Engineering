// JPOG Knowledge Base — Toshi Engine RE
// Format: @ = function, $ = global, struct/enum = types

// === TTerrainShaderD3D.dll ===

// Quality flag accessors
@ 0x10010D50 bool __thiscall IsHighEndTerrain(TTerrainShaderHAL *this);
@ 0x10010D40 bool __thiscall IsMediumTerrain(TTerrainShaderHAL *this);
@ 0x10010D30 bool __thiscall IsLowTerrain(TTerrainShaderHAL *this);
@ 0x10010CF0 void __thiscall EnableHighEndTerrain(TTerrainShaderHAL *this, bool enable);
@ 0x10010CB0 void __thiscall EnableMediumTerrain(TTerrainShaderHAL *this, bool enable);
@ 0x10010C70 void __thiscall EnableLowTerrain(TTerrainShaderHAL *this, bool enable);

// Shader handle getters (call Validate first, return field)
@ 0x10010FE0 void* __thiscall GetVertexShaderHandle(TTerrainShaderHAL *this);
@ 0x10010FD0 void* __thiscall GetPixelShaderHandle(TTerrainShaderHAL *this);

// Rendering
@ 0x10004430 void __thiscall TTerrainShaderHAL_Flush(TTerrainShaderHAL *this);
@ 0x10005790 void __thiscall TTerrainShaderHAL_Render(TTerrainShaderHAL *this, TRenderPacket *packet);
@ 0x10007AF0 void __thiscall TTerrainShaderHAL_RenderWater(TTerrainShaderHAL *this);
// Low-end (reflection-off) water flipbook frame index (0..31); loaded at RVA 0x8A32.
// Frame 2 (t_water0002_1555.dds) matches Remix texture hash 0xEFEC3F35D7660711.
$ 0x1014C2C4 unsigned int g_lowEndWaterFlipbookFrame
@ 0x00519B60 void __thiscall ASky::Create(float radius, float height, unsigned int ringCount, unsigned int sliceCount); __thiscall RenderSky(TTerrainShaderHAL *this);
@ 0x10004830 uint __thiscall TTerrainShaderHAL_Validate(TTerrainShaderHAL *this);
@ 0x10011F50 void __thiscall TGrassShaderHAL_Flush(void *this);
@ 0x10012D70 void __thiscall TGrassShaderHAL_Render(void *this, TRenderPacket *packet);
@ 0x10013DA0 void __thiscall TGrassMaterialHAL_PreRender(void *this);

// === TSysShaderD3D.dll ===

@ 0x100019B0 void __thiscall TSysShaderHAL_Flush(TSysShaderHAL *this);
@ 0x100020C0 bool __thiscall TSysShaderHAL_Validate(TSysShaderHAL *this);
@ 0x100022D0 void __thiscall TSysShaderHAL_Render(TSysShaderHAL *this, TRenderPacket *packet);
@ 0x10002780 void __thiscall TSysShaderHAL_EnableHighEnd(TSysShaderHAL *this, bool enable);
@ 0x100027D0 bool __thiscall TSysShaderHAL_IsCapableHighEnd(TSysShaderHAL *this);
@ 0x10002830 bool __thiscall TSysShaderHAL_IsCapableLightScattering(TSysShaderHAL *this);
@ 0x10003070 void* __thiscall GetTerrainShader(TSysShaderHAL *this);
@ 0x10002890 void __thiscall SetVertexShaderScatteringConstants(TSysShaderHAL *this);

// === TTerrainInterface.dll ===

@ 0x10007470 signed char __thiscall TTerrainSection_ComputeLOD(void *this, void *camPos, signed char *outMinLOD);
// TTerrainInterface::Create cache pool cap imm32 sites: RVA 0x8A11, 0x8A16, 0x8A1B (400), 0x8A2A (350), 0x8A34 (900)
$ 0x100123C4 unsigned int TTerrainSection_sm_ui32Counter

// === simjp.exe ===

@ 0x00473762 void __cdecl TerrainQualitySetup(void);
@ 0x0047BB80 void __thiscall LoadModelWithDetailSuffix(void *this); // load-time _lo/_med/_hi suffix selector
@ 0x005DBF20 void __thiscall ATreeManager_Render(void *this, void *renderContext);
@ 0x005DFCB0 void __thiscall ATreeManager_PopulateSubCell(void *this, unsigned int cacheSlot, unsigned short subcellX, unsigned short subcellZ);
@ 0x005E0A40 void __thiscall ATreeManager_PopulateTreeCell(void *this, unsigned int cellX, unsigned int cellZ, unsigned int cacheSlot);
@ 0x005E0C30 void __thiscall ATreeManager_TraverseQuadtree(void *this, float x, float z, float w, float h, void *planes, unsigned int mask);
@ 0x005E1340 void __thiscall ATreeManager_RenderCellList(void *this, void *listHead);
@ 0x005E2230 void __thiscall ATreeSceneNode_Render(void *this, void *renderContext);
@ 0x005F7B80 int __thiscall JPOGWeatherController_Update(JPOGWeatherController *this, float deltaTime);
@ 0x0065BC10 void* __thiscall LoadTmdWithDetailSuffix(void *this, void *name); // load-time _lo/_med/_hi suffix selector
@ 0x004C6052 void __thiscall AUnitBuilding_SetRender(void *this, unsigned char render); // RVA 0x0C6090 dist gate before InitLensFlares
@ 0x004C8210 void __thiscall AUnitBuilding_InitLensFlares(void *this, unsigned char flag);
@ 0x006BA990 void __thiscall ALensFlareGroupParticles_Render(void *this); // RVA 0x2BAEA0 per-flare distance fade/collapse
@ 0x006BBE50 float __thiscall ALensFlareParticles_GetFarDistRecipSq(void *this); // vtable +0x28 on all 3 flare classes
@ 0x006BC780 void __thiscall ALensFlareParticles_Render(void *this); // RVA 0x2BC806 distance fade/zeroing (base + locator)
@ 0x006CE7F0 void __thiscall AParticleSystem_Create(void *this); // RVA 0x2CE8B0/0x2CEBF4 32-entry flare color-chunk pool
@ 0x006CF4D0 void __thiscall AParticleSystem_Render(void *this); // RVA 0x2CFAA2 2000-vert budget downscale
@ 0x006D1F4D void __cdecl AParticleSystem_SetDetail_HighFlarePool(void); // writes m_aiNumSystems[0x11] (default 15)
$ 0x0083D5B8 int g_effectiveTerrainQuality
$ 0x00843034 int AParticleSystem_m_iMaxVerts
$ 0x00843140 int AParticleSystem_m_iNumFlareColorChunks
$ 0x008500A4 void* g_treeManager
$ 0x008AE1C4 int AParticleSystem_m_aiNumSystems_LensFlare

// === Toshi Engine Structures ===

enum JPOGWeatherId {
    JPOGWeather_Normal = 0,
    JPOGWeather_LightRain = 1,
    JPOGWeather_HeavyRain = 2,
    JPOGWeather_Tornado = 3,
    JPOGWeather_Heatwave = 4
};

struct JPOGWeatherController {
    char unknown_00[0x4C];
    int activeWeather;             // +0x4C effective visible weather, including cheats
    char unknown_50[0x04];
    int scheduledWeather;          // +0x54
    int nextScheduledWeather;      // +0x58
};

struct TTerrainShaderHAL {
    void *vtable;                  // +0x00
    // ...
    char field_0x3C;               // +0x3C  TSysShader: EnableHighEnd flag
    char field_0x3D;               // +0x3D  TSysShader: light scattering flag
    void *vs_handle;               // +0x70  current vertex shader handle
    void *ps_handle;               // +0x7C  current pixel shader handle
    void *field_0x80;              // +0x80  TSysShader: cached terrain shader ptr
    // ...
    void *texture0;                // +0x204 terrain diffuse texture
    void *texture1;                // +0x208 terrain detail texture
    // ...
    unsigned char isHighEndTerrain; // +0x2B8
    unsigned char isLowTerrain;     // +0x2B9
    unsigned char isMediumTerrain;  // +0x2BA
};

struct TRenderPacket {
    void *vtable;                  // +0x00
    void *field_0x04;              // +0x04
    void *resource;                // +0x08  TMesh or TTerrainMesh
    float worldMatrix[16];         // +0x0C  per-instance world transform (4x4)
};

// === Toshi Device Wrapper Vtable ===
// Device ptr: this->vtable[0x134]() + 0x178

enum ToshiDeviceVtable {
    TDEV_SetTransform       = 0x94,  // __stdcall(device, state, matrix_ptr)
    TDEV_SetRenderState     = 0xC8,  // __stdcall(device, state, value)
    TDEV_SetTexture         = 0xF4,  // __stdcall(device, stage, texture)
    TDEV_SetTSS             = 0xFC,  // __stdcall(device, stage, type, value)
    TDEV_SetTransformW      = 0x108, // alternative?
    TDEV_DrawIndexedPrim    = 0x11C, // __stdcall(device, primtype, bvi, nv, si, pc)
    TDEV_SetVertexShader    = 0x130, // __stdcall(device, vs_handle)
    TDEV_SetVSConstantF     = 0x13C, // __stdcall(device, count, data, startReg)
    TDEV_SetStreamSource    = 0x14C, // __stdcall(device, stream, vtxbuf, stride)
    TDEV_SetIndices         = 0x154, // __stdcall(device, indexbuf)
    TDEV_SetPixelShader     = 0x160, // __stdcall(device, ps_handle)
};

// D3D Transform State Constants
// D3DTS_VIEW = 2, D3DTS_PROJECTION = 3, D3DTS_WORLD = 0x100 (256)

// === TRenderD3DInterface.dll: render-context lifecycle ===
// Exported native destructor. Restores vtable RVA 0x74480, then calls the base destructor.
@ 0x100071C0 void __thiscall DestroyRenderContextD3D(void *this);
@ 0x10007180 void __thiscall SetRenderContextMatrices(void *this);

// === TSkinShaderD3D.dll (ImageBase 0x10000000) ===
struct TSkinShaderHAL {
    unsigned char pad0[0x6C];
    unsigned char enableRenderEnvMap;  // +0x6C  must be 0 when envmap PS (+0x54) == -1
    unsigned char pad1[0x43];
    unsigned char highEndSkinning;     // +0xB0  0 = FVF 0x1D2 FFP, 1 = VS 1.1 skinning
    unsigned char scattering;          // +0xB1  0 = FF PS (NULL), 1 = scattering PS
    unsigned char pad2[0x0A];
    unsigned char hasSSE;              // +0xBC  enables SSE 2-bone matrix blend
};
@ 0x10001B60 void __thiscall TSkinShaderHAL_Flush(struct TSkinShaderHAL *this);
@ 0x10002120 unsigned char __thiscall TSkinShaderHAL_IsCapableHighEndSkinning(struct TSkinShaderHAL *this);
@ 0x10002190 unsigned char __thiscall TSkinShaderHAL_IsCapableScattering(struct TSkinShaderHAL *this);
@ 0x10002570 void __thiscall TSkinShaderHAL_Render(struct TSkinShaderHAL *this, struct TRenderPacket *packet);
// Dynamic CPU-skinning VB creation at RVA 0x3340; byte-size imm32 at RVA 0x334B (default 0x17700 = 96000 B -> patched to 0x280000 = 2.5 MiB)

// === TSpriteShaderD3D.dll (ImageBase 0x10000000) ===
struct TSpriteShaderHAL {
    unsigned char pad0[0x128];
    unsigned int vsHandle;             // +0x128  VS handle when useVS=1, FVF 0x142 (XYZ|DIFFUSE|TEX1) when useVS=0
    unsigned char useVS;               // +0x12C  1 = TSpriteShaderD3D.vsh, 0 = built-in FVF 0x142 FFP path
};
@ 0x10002660 void __thiscall TSpriteShaderHAL_Flush(struct TSpriteShaderHAL *this);
@ 0x10002840 unsigned char __thiscall TSpriteShaderHAL_Validate(struct TSpriteShaderHAL *this);
@ 0x10002A30 void __thiscall TSpriteShaderHAL_Render(struct TSpriteShaderHAL *this, struct TRenderPacket *packet);
@ 0x100030A0 void __thiscall TSpriteMaterialHAL_PreRender(void *this); // +0x48 blend mode: 1=translucent(5/6), 2=mul(1/3), 3=additive(5/2), 6=opaque(2/1)

// === simjp.exe: Lens Flare & Electric Fence Particle Systems ===
@ 0x004C8210 void __thiscall AUnitBuilding_InitLensFlares(void *this, unsigned char fadeIn);
@ 0x004D03F0 void __thiscall AUnitHelicopter_SetRender(void *this, unsigned char visible);
@ 0x004D6190 void __thiscall AUnitVehicle_SetRender(void *this, unsigned char visible);
@ 0x005ED3E0 void __thiscall AWallManager_RenderScene(void *this);
@ 0x005EF870 void __thiscall AWallManager_CullQuadTree(void *this);
@ 0x005F2730 void __thiscall AWallSceneNode_Render(void *this);
@ 0x006B4C80 int __thiscall AElectricFenceParticles_Update(void *this, float dt);
@ 0x006B4D80 void __thiscall AElectricFenceParticles_Render(void *this);
@ 0x006BA990 void __thiscall ALensFlareGroupParticles_Render(void *this);
@ 0x006BB9B0 int __thiscall ALensFlareGroupParticles_Update(void *this, float dt);
@ 0x006BBE50 float __thiscall ALensFlareParticles_GetFarDistRecipSq(void *this);
@ 0x006BC770 void __thiscall ALensFlareParticles_Render(void *this);
@ 0x006BD390 int __thiscall ALensFlareParticles_Update(void *this, float dt);
@ 0x006CF4D0 void __thiscall AParticleSystem_Render(void *this);
@ 0x006D2020 void __thiscall AParticleSystem_OnUpdate(void *this, float dt);
@ 0x006D2AD0 void __thiscall AParticleSystem_CreateMeshes(void *this); // uses TSysShaderHAL (GetSystemResource(8))
$ 0x00850038 void* g_pGroupLensFlares
$ 0x00853490 unsigned int g_electricFencePulseARGB
$ 0x00853724 unsigned char g_lensFlareUpdatedThisFrame
$ 0x00853728 float g_lensFlarePhaseTime
$ 0x008AE320 unsigned int g_lensFlareColors[6] // white, red, yellow, orange, fyellow, fblue
