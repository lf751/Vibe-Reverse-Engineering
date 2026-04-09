/*
 * JPOG RTX Remix ASI Plugin
 *
 * Two independent hooks loaded by dxwrapper from the plugins/ folder:
 *
 * 1. View Matrix Fix (TRenderD3DInterface.dll)
 *    Hooks SetRenderMatrices to pass the real camera View matrix instead
 *    of the identity the engine sends. Remix needs this for camera tracking.
 *
 * 2. Terrain Q1 FFP Patch (TTerrainShaderD3D.dll)
 *    Forces Q1 (MEDIUM) terrain rendering path and injects SetTransform(WORLD)
 *    per tile so Remix can track terrain geometry.
 *
 * 3. Q1 Sky UV Fix (TTerrainShaderD3D.dll)
 *    Q1 sky dome has vertex UV V ∈ [0.25, 0.75] but Q2 sky texture expects
 *    [0.0, 1.0]. Hooks RenderSky entry (RVA 0x9C90) to capture TTerrainShaderHAL
 *    'this' on first call, then locks the sky VB at this+0xD4 (stride 20 bytes,
 *    V at float offset 16) and remaps: new_V = 2 * old_V - 0.5 for V ∈ [0.2, 0.8].
 *    One-time operation, flag-guarded.
 *
 * RE discoveries (Toshi engine):
 *   TRenderContext: +0x8C = View, +0x484 = Projection
 *   TRenderPacket:  +0x08 = resource, +0x0C = world matrix (4x4 float)
 *   Toshi device vtable: +0x94 = SetTransform(__stdcall)
 *   SetRenderMatrices: RVA 0x7180  in TRenderD3DInterface.dll
 *   IsHighEndTerrain:  RVA 0x10D50 in TTerrainShaderD3D.dll
 *   IsMediumTerrain:   RVA 0x10D40 in TTerrainShaderD3D.dll
 *   Q1 Render entry:   RVA 0x5866  in TTerrainShaderD3D.dll
 *   RenderSky entry:   RVA 0x9C90  in TTerrainShaderD3D.dll (12-byte prologue hooked)
 *   Sky VB pointer:    TTerrainShaderHAL + 0xD4 (IDirect3DVertexBuffer8*)
 *   Sky VB stride:     20 bytes (5 floats: X,Y,Z,U,V); V at float offset 16
 *   Sky VB vtable:     [11]=Lock, [12]=Unlock (D3D8 IDirect3DVertexBuffer8)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ---- No-CRT memcpy ---- */
#pragma function(memcpy)
void * __cdecl memcpy(void *dst, const void *src, unsigned int n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

/* ---- Offsets (from RE) ---- */
#define RCTX_OFF_WORLDVIEW   0x8C
#define RCTX_OFF_PROJECTION  0x484
#define RCTX_OFF_IFACE       0x4
#define IFACE_OFF_DEVICE     0x178
#define D3D8_VT_SETTRANSFORM 0x94

#define D3DTS_VIEW       2
#define D3DTS_PROJECTION 3
#define D3DTS_WORLD      256   /* 0x100 */

static const float s_identity[16] = {
    1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
};

/* ====================================================================
 * Hook 1: View Matrix Fix (TRenderD3DInterface.dll)
 * ==================================================================== */

static float g_lastRealView[16] = {
    1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
};
static int g_hasLastRealView = 0;

static void __fastcall Hook_SetRenderMatrices(void *thisPtr, void *edx_unused) {
    typedef int (__stdcall *FnSetTransform)(void*, int, void*);

    void **ppIface;
    void *device;
    void **vtbl;
    FnSetTransform fnST;
    float *viewMatrix;
    float *projMatrix;
    float txRow, tyRow, tzRow;
    float txCol, tyCol, tzCol;
    float tRow2, tCol2;
    void *viewToSend;

    ppIface = *(void**)((char*)thisPtr + RCTX_OFF_IFACE);
    device  = *(void**)((char*)ppIface + IFACE_OFF_DEVICE);
    vtbl    = *(void***)device;
    fnST    = (FnSetTransform)vtbl[D3D8_VT_SETTRANSFORM / 4];

    viewMatrix = (float*)((char*)thisPtr + RCTX_OFF_WORLDVIEW);
    projMatrix = (float*)((char*)thisPtr + RCTX_OFF_PROJECTION);

    txRow = viewMatrix[12]; tyRow = viewMatrix[13]; tzRow = viewMatrix[14];
    txCol = viewMatrix[3];  tyCol = viewMatrix[7];  tzCol = viewMatrix[11];
    tRow2 = txRow*txRow + tyRow*tyRow + tzRow*tzRow;
    tCol2 = txCol*txCol + tyCol*tyCol + tzCol*tzCol;

    if (tRow2 > 1.0f || tCol2 > 1.0f) {
        memcpy(g_lastRealView, viewMatrix, sizeof(g_lastRealView));
        g_hasLastRealView = 1;
        viewToSend = (void*)viewMatrix;
    } else if (g_hasLastRealView) {
        viewToSend = (void*)g_lastRealView;
    } else {
        viewToSend = (void*)s_identity;
    }

    fnST(device, D3DTS_VIEW, viewToSend);
    fnST(device, D3DTS_PROJECTION, projMatrix);
}

/* ====================================================================
 * Hook 2: Terrain Q1 FFP Patch (TTerrainShaderD3D.dll)
 *
 * Patches applied:
 *   1a. Render Q2 branch (RVA 0x584A) -> NOP jne (skip Q2 terrain)
 *   1b. RenderSky Q2 branch (RVA 0x9E9A) -> NOP je (force Q2 sky always)
 *   2. IsMediumTerrain  (RVA 0x10D40) -> always return 1
 *   3. Q1 Render entry  (RVA 0x5866)  -> JMP to hook that injects
 *      SetTransform(WORLD, render_packet+0xC) per terrain tile
 *
 * At Q1 Render entry (same frame as Q0 -- after prologue: push edi/esi/ebp/ebx + sub esp,0x64):
 *   ESI = Toshi device (D3D8 wrapper)
 *   EBP = TTerrainShaderHAL this pointer
 *   EBX = render_packet->resource
 *   [ESP+0x78] = render_packet (TRenderPacket*)
 *
 * Overwritten 6 bytes at RVA 0x5866:
 *   8B 06           mov eax, [esi]       ; device vtable
 *   8B 54 24 08     mov edx, [esp+8]     ; local: vertex buffer
 * Re-executed after the hook before jumping back to 0x586C.
 * ==================================================================== */

static BYTE *g_Q1ReturnAddr = NULL;
static void *g_d3dDevice    = NULL;  /* Toshi device, cached from terrain render ESI */

/* ====================================================================
 * Hook 3: Sky VB UV Remap (TTerrainShaderD3D.dll)
 *
 * The Q2 sky render path (forced by Patch 1b) uses TTerrainShaderHAL+0xD4
 * as the sky vertex buffer (stride 20: X,Y,Z,U,V). Q1 sky dome was created
 * with V coords in [0.25, 0.75]; Q2 sky texture expects [0.0, 1.0].
 *
 * Hook: intercept RenderSky at entry to capture 'this' (ecx), then on first
 * call lock the sky VB and remap V (float at byte offset 16) in-place:
 *   new_V = 2 * old_V - 0.5   →  [0.25, 0.75] maps to [0.0, 1.0]
 * ==================================================================== */

static BYTE *g_RenderSkyRetAddr = NULL;
static void *g_patchedSkyVB     = NULL;  /* last VB address we remapped */
static volatile int *g_pSkyOverride  = NULL;  /* &proxy->g_skipWorldOverride */
static void         *g_skyOrigRet    = NULL;  /* trampoline: caller's return address */
static void         *g_skyExitHookPtr = NULL; /* set to Hook_RenderSky_Exit at init */

/* D3D8 IDirect3DVertexBuffer8 vtable slots */
#define D3D8VB_LOCK_SLOT   11
#define D3D8VB_UNLOCK_SLOT 12

/* Sky VB vertex layout (stride = 20 bytes) */
#define SKY_VB_STRIDE       20
#define SKY_VB_V_OFFSET     16   /* float V at byte 16 in each vertex */
#define SKY_VB_VERTS_MAX 65536   /* safety cap */

static void PatchSkyVBUV(void *pThis) {
    typedef long (__stdcall *FnLock)(void *self, unsigned int off,
                                     unsigned int sz, unsigned char **pp,
                                     unsigned int flags);
    typedef long (__stdcall *FnUnlock)(void *self);

    void          *pVB;
    void         **vtbl;
    unsigned char *pData;
    unsigned int   numVerts;
    unsigned int   i;
    int            remapped;

    pVB = *(void **)((unsigned char *)pThis + 0xD4);
    if (!pVB || pVB == g_patchedSkyVB)
        return;  /* null or already patched this exact VB */

    vtbl = *(void ***)pVB;

    pData = NULL;
    if (((FnLock)vtbl[D3D8VB_LOCK_SLOT])(pVB, 0, 0, &pData, 0) != 0)
        return;
    if (!pData) {
        ((FnUnlock)vtbl[D3D8VB_UNLOCK_SLOT])(pVB);
        return;
    }

    /* Vertex count: read from TTerrainShaderHAL.field_0x55C (Q2 sky DIP param) */
    numVerts = *(unsigned int *)((unsigned char *)pThis + 0x55C);
    if (numVerts == 0 || numVerts > SKY_VB_VERTS_MAX)
        numVerts = SKY_VB_VERTS_MAX;

    /* Remap V ∈ [0.2, 0.8] at byte offset 16 → new_V = 2*V - 0.5 */
    remapped = 0;
    for (i = 0; i < numVerts; i++) {
        float *vf = (float *)(pData + i * SKY_VB_STRIDE + SKY_VB_V_OFFSET);
        if (*vf >= 0.20f && *vf <= 0.80f) {
            *vf = 2.0f * (*vf) - 0.5f;
            ++remapped;
        }
    }

    ((FnUnlock)vtbl[D3D8VB_UNLOCK_SLOT])(pVB);

    if (remapped > 0)
        g_patchedSkyVB = pVB;  /* record which VB was remapped; re-patch if it changes */
}

/* Called when RenderSky returns: clears the proxy's skip flag. */
__declspec(naked) static void Hook_RenderSky_Exit(void) {
    __asm {
        mov  eax, dword ptr [g_pSkyOverride]
        test eax, eax
        jz   NoClear
        mov  dword ptr [eax], 0
    NoClear:
        jmp  dword ptr [g_skyOrigRet]
    }
}

__declspec(naked) static void Hook_RenderSky_Entry(void) {
    /* ecx = TTerrainShaderHAL* (thiscall).
     * 1. Swap return address with Hook_RenderSky_Exit so we get an exit callback.
     * 2. Remap sky VB UV (one-time).  3. Set proxy skip flag.
     * 4. Re-execute the 12 overwritten prologue bytes before RenderSky+12. */
    __asm {
        /* Save caller's return address; replace with exit hook so RenderSky
         * returns to Hook_RenderSky_Exit, which clears the flag and tail-calls
         * the original caller. */
        pop  eax
        mov  dword ptr [g_skyOrigRet], eax
        push dword ptr [g_skyExitHookPtr]

        /* One-time VB UV remap */
        push ecx
        push ecx
        call PatchSkyVBUV
        add  esp, 4
        pop  ecx

        /* Set proxy flag: suppress WorldOverride for all DIPs inside RenderSky */
        mov  eax, dword ptr [g_pSkyOverride]
        test eax, eax
        jz   NoSet
        mov  dword ptr [eax], 1
    NoSet:

        /* Re-execute original 12-byte prologue: push edi/esi/ebp/ebx, sub esp 0x1CC, mov ebp ecx */
        push edi
        push esi
        push ebp
        push ebx
        sub  esp, 0x1CC
        mov  ebp, ecx

        jmp  dword ptr [g_RenderSkyRetAddr]
    }
}

__declspec(naked) static void Hook_Q1_InjectWorld(void) {
    __asm {
        /* Cache Toshi device from ESI on first terrain tile */
        cmp  dword ptr [g_d3dDevice], 0
        jne  DeviceCached
        mov  dword ptr [g_d3dDevice], esi
        DeviceCached:

        mov  eax, dword ptr [esp + 078h]  /* render_packet */
        add  eax, 0x0C                     /* world matrix at packet+0xC */
        mov  edx, dword ptr [esi]          /* device vtable */
        push eax                           /* arg3: matrix ptr */
        push 0x100                         /* arg2: D3DTS_WORLD */
        push esi                           /* arg1: device */
        call dword ptr [edx + 094h]        /* SetTransform (stdcall, cleans 12) */

        /* Re-execute overwritten instructions */
        mov  eax, dword ptr [esi]          /* original: mov eax, [esi] */
        mov  edx, dword ptr [esp + 08h]    /* original: mov edx, [esp+8] */

        jmp  dword ptr [g_Q1ReturnAddr]
    }
}

/* ---- Patch helpers ---- */

static void PatchBytes(BYTE *addr, const BYTE *data, int len) {
    DWORD oldProt;
    VirtualProtect(addr, len, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy(addr, data, len);
    VirtualProtect(addr, len, oldProt, &oldProt);
}

static void InstallJump(HMODULE hMod, DWORD rva, void *pHook) {
    BYTE *pTarget;
    DWORD oldProt;
    int rel;

    pTarget = (BYTE*)hMod + rva;
    VirtualProtect(pTarget, 5, PAGE_EXECUTE_READWRITE, &oldProt);
    pTarget[0] = 0xE9;
    rel = (int)((BYTE*)pHook - pTarget - 5);
    memcpy(pTarget + 1, &rel, 4);
    VirtualProtect(pTarget, 5, oldProt, &oldProt);
}

/* ---- Install terrain patches ---- */

static void InstallTerrainPatches(HMODULE hMod) {
    BYTE *base = (BYTE*)hMod;

    /* Patch 1: NOP the Q2 terrain branch in Render (RVA 0x584A)
     * Original: jne 0x5C80 (0F 85 30 04 00 00) -- skips to Q2 terrain when
     * IsHighEndTerrain is true. NOPing forces fall-through to IsMediumTerrain
     * check, while leaving IsHighEndTerrain intact for RenderSky Q2 sky. */
    {
        static const BYTE patch[] = { 0x90,0x90,0x90,0x90,0x90,0x90 };
        PatchBytes(base + 0x584A, patch, 6);
    }

    /* Patch 1b: NOP the Q2 sky branch in RenderSky (RVA 0x9E9A)
     * Original: je 0xA5B6 (0F 84 16 07 00 00) -- skips Q2 sky when
     * IsHighEndTerrain is false. NOPing forces Q2 textured sky always. */
    {
        static const BYTE patch1b[] = { 0x90,0x90,0x90,0x90,0x90,0x90 };
        PatchBytes(base + 0x9E9A, patch1b, 6);
    }

    /* Patch 2: IsMediumTerrain -> mov al,1; nop*5; ret (always true) */
    {
        static const BYTE patch[] = {  0xB0,0x01, 0x90,0x90,0x90,0x90,0x90, 0xC3 };
        PatchBytes(base + 0x10D40, patch, 8);
    }

    /* Patch 3: Hook Q1 Render entry with JMP to SetTransform injector */
    g_Q1ReturnAddr = base + 0x586C;
    {
        BYTE hook[6];
        int rel = (int)((BYTE*)Hook_Q1_InjectWorld - (base + 0x5866) - 5);
        hook[0] = 0xE9;
        memcpy(&hook[1], &rel, 4);
        hook[5] = 0x90;
        PatchBytes(base + 0x5866, hook, 6);
    }

    /* Patch 4: Hook RenderSky entry to remap Q1 sky dome UV V [0.25,0.75]→[0,1].
     * Also brackets RenderSky with g_skipWorldOverride=1 (via return-address swap
     * to Hook_RenderSky_Exit) so the proxy's WorldOverride is suppressed only
     * during sky DIPs, leaving terrain/object overrides intact. */
    g_skyExitHookPtr = (void *)Hook_RenderSky_Exit;
    g_pSkyOverride   = (volatile int *)GetProcAddress(
        GetModuleHandleA("d3d9.dll"), "g_skipWorldOverride");
    g_RenderSkyRetAddr = base + 0x9C90 + 12;
    InstallJump(hMod, 0x9C90, (void *)Hook_RenderSky_Entry);
}

/* ---- Install render hooks ---- */

static void InstallRenderHooks(HMODULE hMod) {
    InstallJump(hMod, 0x7180, (void*)Hook_SetRenderMatrices);
}

/* ---- Delayed init thread ---- */

static DWORD WINAPI InitThread(LPVOID param) {
    HMODULE hRenderD3D = NULL;
    HMODULE hTerrainShader = NULL;
    int renderDone = 0, terrainDone = 0;
    int i;

    for (i = 0; i < 300; i++) {  /* 30 seconds max */
        if (!renderDone) {
            hRenderD3D = GetModuleHandleA("TRenderD3DInterface.dll");
            if (hRenderD3D) {
                InstallRenderHooks(hRenderD3D);
                renderDone = 1;
            }
        }
        if (!terrainDone) {
            hTerrainShader = GetModuleHandleA("TTerrainShaderD3D.dll");
            if (hTerrainShader) {
                InstallTerrainPatches(hTerrainShader);
                terrainDone = 1;
            }
        }
        if (renderDone && terrainDone)
            return 0;
        Sleep(100);
    }
    return 0;
}

/* ---- DLL entry point ---- */

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hDll);
        CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
    }
    return TRUE;
}

int _fltused = 0;
