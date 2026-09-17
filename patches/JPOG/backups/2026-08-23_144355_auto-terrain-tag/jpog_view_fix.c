/*
 * JPOG RTX Remix ASI Plugin
 *
 * Hooks loaded by dxwrapper from the game folder:
 *
 * 1. View Matrix Fix (TRenderD3DInterface.dll)
 *    Hooks SetRenderMatrices to pass the real camera View matrix instead
 *    of the identity the engine sends. Remix needs this for camera tracking.
 *
 * 2. Terrain path remains unmodified; the game already uses medium terrain.
 *    This patch only adjusts the view/projection matrix flow for Remix.
 *
 * 3. Weather Preset Sync (simjp.exe)
 *    Maps JPOG's active weather index to the matching Remix weather preset.
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
#include <math.h>
#include <float.h>
#define REMIX_ALLOW_X86
#define REMIX_WINAPI_NO_LIBRARY_LOADER
#include "remix/remix_c.h"

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

#define WEATHER_UPDATE_RVA       0x1F7B80
#define WEATHER_OFF_ACTIVE       0x4C

typedef int (__stdcall *FnSetTransform)(void*, int, void*);
typedef int (__fastcall *FnWeatherUpdate)(void*, void*, float);

static const float s_identity[16] = {
    1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
};

/* ====================================================================
 * Hook 1: View Matrix Fix (TRenderD3DInterface.dll)
 * ==================================================================== */

static float g_lastRealView[16] = {
    1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
};
static float g_lastRealProjection[16] = {
    1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
};
static int g_hasLastRealView = 0;
static void *g_mainCameraContext = NULL;
static FnSetTransform g_originalSetTransform = NULL;
static FnWeatherUpdate g_originalWeatherUpdate = NULL;
static remixapi_Interface g_remix = { 0 };
static int g_lastWeatherIndex = -1;

static int InitializeRemixApi(void) {
    HMODULE hRemix;
    PFN_remixapi_InitializeLibrary initializeLibrary;
    remixapi_InitializeLibraryInfo info;

    if (g_remix.SetGameValue)
        return 1;

    hRemix = GetModuleHandleA("d3d9_remix.dll");
    if (!hRemix)
        hRemix = GetModuleHandleA("d3d9.dll");
    if (!hRemix)
        return 0;

    initializeLibrary = (PFN_remixapi_InitializeLibrary)GetProcAddress(
        hRemix, "remixapi_InitializeLibrary");
    if (!initializeLibrary)
        return 0;

    info.sType = REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
    info.pNext = NULL;
    info.version = REMIXAPI_VERSION_MAKE(
        REMIXAPI_VERSION_MAJOR,
        REMIXAPI_VERSION_MINOR,
        REMIXAPI_VERSION_PATCH);

    if (initializeLibrary(&info, &g_remix) != REMIXAPI_ERROR_CODE_SUCCESS)
        return 0;
    return g_remix.SetGameValue != NULL;
}

static void SyncRemixWeather(void *weatherController) {
    static const char *presets[] = {
        "partlyCloudy",
        "rainstorm",
        "thunderstorm",
        "thunderstorm",
        "sandstorm"
    };
    int weatherIndex;

    weatherIndex = *(int*)((char*)weatherController + WEATHER_OFF_ACTIVE);
    if (weatherIndex < 0 || weatherIndex >= 5 || weatherIndex == g_lastWeatherIndex)
        return;
    if (!InitializeRemixApi())
        return;
    if (g_remix.SetGameValue("__weather.blend_seconds", "11.0") != REMIXAPI_ERROR_CODE_SUCCESS)
        return;
    if (g_remix.SetGameValue("__weather.target", presets[weatherIndex]) != REMIXAPI_ERROR_CODE_SUCCESS)
        return;

    g_lastWeatherIndex = weatherIndex;
}

static int __fastcall Hook_WeatherUpdate(void *thisPtr, void *edx_unused, float deltaTime) {
    int result;
    (void)edx_unused;

    result = g_originalWeatherUpdate(thisPtr, NULL, deltaTime);
    SyncRemixWeather(thisPtr);
    return result;
}

static int __stdcall Hook_DeviceSetTransform(void *device, int state, void *matrix) {
    if (state == D3DTS_VIEW && g_hasLastRealView) {
        matrix = (void*)g_lastRealView;
    } else if (state == D3DTS_PROJECTION && g_hasLastRealView) {
        matrix = (void*)g_lastRealProjection;
    }
    return g_originalSetTransform(device, state, matrix);
}

static void __fastcall Hook_SetRenderMatrices(void *thisPtr, void *edx_unused) {
    void **ppIface;
    void *device;
    void **vtbl;
    FnSetTransform fnST;
    float *viewMatrix;
    float *projMatrix;
    float txRow, tyRow, tzRow;
    float txCol, tyCol, tzCol;
    float tRow2, tCol2;
    float determinant;
    int isIdentity;
    void *viewToSend;

    ppIface = *(void**)((char*)thisPtr + RCTX_OFF_IFACE);
    device  = *(void**)((char*)ppIface + IFACE_OFF_DEVICE);
    vtbl    = *(void***)device;
    fnST    = (FnSetTransform)vtbl[D3D8_VT_SETTRANSFORM / 4];

    if (!g_originalSetTransform) {
        DWORD oldProt;
        g_originalSetTransform = fnST;
        VirtualProtect(&vtbl[D3D8_VT_SETTRANSFORM / 4], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
        vtbl[D3D8_VT_SETTRANSFORM / 4] = (void*)Hook_DeviceSetTransform;
        VirtualProtect(&vtbl[D3D8_VT_SETTRANSFORM / 4], sizeof(void*), oldProt, &oldProt);
    }

    viewMatrix = (float*)((char*)thisPtr + RCTX_OFF_WORLDVIEW);
    projMatrix = (float*)((char*)thisPtr + RCTX_OFF_PROJECTION);

    txRow = viewMatrix[12]; tyRow = viewMatrix[13]; tzRow = viewMatrix[14];
    txCol = viewMatrix[3];  tyCol = viewMatrix[7];  tzCol = viewMatrix[11];
    tRow2 = txRow*txRow + tyRow*tyRow + tzRow*tzRow;
    tCol2 = txCol*txCol + tyCol*tyCol + tzCol*tzCol;
    determinant =
        viewMatrix[0] * (viewMatrix[5] * viewMatrix[10] - viewMatrix[6] * viewMatrix[9]) -
        viewMatrix[1] * (viewMatrix[4] * viewMatrix[10] - viewMatrix[6] * viewMatrix[8]) +
        viewMatrix[2] * (viewMatrix[4] * viewMatrix[9] - viewMatrix[5] * viewMatrix[8]);
    isIdentity =
        viewMatrix[0]  == 1.0f && viewMatrix[1]  == 0.0f && viewMatrix[2]  == 0.0f && viewMatrix[3]  == 0.0f &&
        viewMatrix[4]  == 0.0f && viewMatrix[5]  == 1.0f && viewMatrix[6]  == 0.0f && viewMatrix[7]  == 0.0f &&
        viewMatrix[8]  == 0.0f && viewMatrix[9]  == 0.0f && viewMatrix[10] == 1.0f && viewMatrix[11] == 0.0f &&
        viewMatrix[12] == 0.0f && viewMatrix[13] == 0.0f && viewMatrix[14] == 0.0f && viewMatrix[15] == 1.0f;

    if ((tRow2 > 1.0f || tCol2 > 1.0f) && determinant < -0.5f) {
        g_mainCameraContext = thisPtr;
    }

    if (thisPtr == g_mainCameraContext && determinant > 0.5f) {
        memcpy(g_lastRealView, viewMatrix, sizeof(g_lastRealView));
        memcpy(g_lastRealProjection, projMatrix, sizeof(g_lastRealProjection));
        g_hasLastRealView = 1;
        viewToSend = (void*)viewMatrix;
    } else if (g_hasLastRealView) {
        viewToSend = (void*)g_lastRealView;
    } else if (isIdentity) {
        viewToSend = (void*)s_identity;
    } else {
        viewToSend = (void*)s_identity;
    }

    Hook_DeviceSetTransform(device, D3DTS_VIEW, viewToSend);
    Hook_DeviceSetTransform(device, D3DTS_PROJECTION, projMatrix);
}

/* ====================================================================
 * Terrain path remains unmodified; the game already uses medium terrain.
 * ==================================================================== */

/* ====================================================================
 * Sky rendering is intentionally left unmodified.
 * ==================================================================== */

/* ---- Patch helpers ---- */

static void PatchBytes(BYTE *addr, const BYTE *data, int len) {
    DWORD oldProt;
    VirtualProtect(addr, len, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy(addr, data, len);
    FlushInstructionCache(GetCurrentProcess(), addr, len);
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
    FlushInstructionCache(GetCurrentProcess(), pTarget, 5);
    VirtualProtect(pTarget, 5, oldProt, &oldProt);
}

static int InstallWeatherHook(HMODULE hMod) {
    static const BYTE expectedPrologue[5] = { 0x56, 0x53, 0x83, 0xEC, 0x14 };
    BYTE *target;
    BYTE *trampoline;
    int rel;
    int i;

    target = (BYTE*)hMod + WEATHER_UPDATE_RVA;
    for (i = 0; i < 5; i++) {
        if (target[i] != expectedPrologue[i])
            return 0;
    }

    trampoline = (BYTE*)VirtualAlloc(NULL, 10, MEM_COMMIT | MEM_RESERVE,
                                     PAGE_EXECUTE_READWRITE);
    if (!trampoline)
        return 0;

    memcpy(trampoline, target, 5);
    trampoline[5] = 0xE9;
    rel = (int)((target + 5) - (trampoline + 10));
    memcpy(trampoline + 6, &rel, 4);
    g_originalWeatherUpdate = (FnWeatherUpdate)trampoline;
    InstallJump(hMod, WEATHER_UPDATE_RVA, (void*)Hook_WeatherUpdate);
    return 1;
}

/* ---- Install terrain patches ---- */

static void InstallTerrainPatches(HMODULE hMod) {
    (void)hMod;
    /* Terrain path remains untouched; the game already uses medium terrain. */
}

/* ---- Install render hooks ---- */

static void InstallRenderHooks(HMODULE hMod) {
    InstallJump(hMod, 0x7180, (void*)Hook_SetRenderMatrices);
}

/* ---- Delayed init thread ---- */

static DWORD WINAPI InitThread(LPVOID param) {
    HMODULE hSimJp;
    HMODULE hRenderD3D = NULL;
    HMODULE hTerrainShader = NULL;
    int renderDone = 0, terrainDone = 0;
    int i;

    (void)param;
    hSimJp = GetModuleHandleA(NULL);
    if (hSimJp)
        InstallWeatherHook(hSimJp);

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
