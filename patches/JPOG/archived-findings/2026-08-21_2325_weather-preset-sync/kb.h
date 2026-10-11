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
@ 0x00519B60 void __thiscall ASky::Create(float radius, float height, unsigned int ringCount, unsigned int sliceCount); __thiscall RenderSky(TTerrainShaderHAL *this);
@ 0x10004830 uint __thiscall TTerrainShaderHAL_Validate(TTerrainShaderHAL *this);

// === TSysShaderD3D.dll ===

@ 0x100019B0 void __thiscall TSysShaderHAL_Flush(TSysShaderHAL *this);
@ 0x100022D0 void __thiscall TSysShaderHAL_Render(TSysShaderHAL *this, TRenderPacket *packet);
@ 0x10002780 void __thiscall TSysShaderHAL_EnableHighEnd(TSysShaderHAL *this, bool enable);
@ 0x10003070 void* __thiscall GetTerrainShader(TSysShaderHAL *this);
@ 0x10002890 void __thiscall SetVertexShaderScatteringConstants(TSysShaderHAL *this);

// === simjp.exe ===

@ 0x00473762 void __cdecl TerrainQualitySetup(void);
$ 0x0083D5B8 int g_effectiveTerrainQuality

// === Toshi Engine Structures ===

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
