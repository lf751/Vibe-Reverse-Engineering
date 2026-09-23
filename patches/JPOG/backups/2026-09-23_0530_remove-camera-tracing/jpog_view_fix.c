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
 * 4. Terrain Texture Detection (TTerrainShaderD3D.dll)
 *    Brackets terrain rendering so the D3D9 proxy can tag generated island
 *    textures as Remix terrain without relying on unstable content hashes.
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
#include <intrin.h>
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

#define WEATHER_UPDATE_RVA       0x1F7B80
#define WEATHER_OFF_ACTIVE       0x4C
#define TERRAIN_RENDER_RVA       0x5790
#define CAMERA_MIN_TRANSLATION2  2500.0f
#define CAMERA_TRACE_MAX_RECORDS 20000u

typedef int (__stdcall *FnSetTransform)(void*, int, void*);
typedef int (__fastcall *FnWeatherUpdate)(void*, void*, float);
typedef void (__fastcall *FnTerrainRender)(void*, void*, void*);

typedef struct CameraTraceRecord {
    DWORD tick;
    unsigned int context;
    unsigned int device;
    unsigned int caller;
    float rowTranslation[3];
    float columnTranslation[3];
    float determinant;
    float projection[4];
} CameraTraceRecord;

typedef struct CameraTraceSlot {
    unsigned int caller;
    float rowTranslation[3];
    float columnTranslation[3];
    int used;
} CameraTraceSlot;

/* ====================================================================
 * Hook 1: View Matrix Fix (TRenderD3DInterface.dll)
 * ==================================================================== */

static void *g_mainCameraContext = NULL;
static HANDLE g_cameraTrace = INVALID_HANDLE_VALUE;
static unsigned int g_cameraTraceCount = 0;
static CameraTraceSlot g_cameraTraceSlots[32];
static FnWeatherUpdate g_originalWeatherUpdate = NULL;
static FnTerrainRender g_originalTerrainRender = NULL;
static BYTE *g_weatherTrampoline = NULL;
static BYTE *g_terrainTrampoline = NULL;
static volatile int *g_terrainRenderActive = NULL;
static remixapi_Interface g_remix = { 0 };
static int g_lastWeatherIndex = -1;

static void TraceCameraContext(void *context, void *device,
    void *caller, const float *view, const float *projection, float determinant)
{
    CameraTraceRecord record;
    CameraTraceSlot *slot = NULL;
    DWORD written;
    unsigned int i;
    float rowDelta2, columnDelta2;

    for (i = 0; i < 32; i++) {
        if (g_cameraTraceSlots[i].used &&
            g_cameraTraceSlots[i].caller == (unsigned int)(unsigned long)caller) {
            slot = &g_cameraTraceSlots[i];
            break;
        }
        if (!slot && !g_cameraTraceSlots[i].used)
            slot = &g_cameraTraceSlots[i];
    }
    if (slot && slot->used) {
        rowDelta2 =
            (view[12] - slot->rowTranslation[0]) * (view[12] - slot->rowTranslation[0]) +
            (view[13] - slot->rowTranslation[1]) * (view[13] - slot->rowTranslation[1]) +
            (view[14] - slot->rowTranslation[2]) * (view[14] - slot->rowTranslation[2]);
        columnDelta2 =
            (view[3] - slot->columnTranslation[0]) * (view[3] - slot->columnTranslation[0]) +
            (view[7] - slot->columnTranslation[1]) * (view[7] - slot->columnTranslation[1]) +
            (view[11] - slot->columnTranslation[2]) * (view[11] - slot->columnTranslation[2]);
        if (rowDelta2 < 0.0001f && columnDelta2 < 0.0001f)
            return;
    }
    if (slot) {
        slot->caller = (unsigned int)(unsigned long)caller;
        slot->rowTranslation[0] = view[12];
        slot->rowTranslation[1] = view[13];
        slot->rowTranslation[2] = view[14];
        slot->columnTranslation[0] = view[3];
        slot->columnTranslation[1] = view[7];
        slot->columnTranslation[2] = view[11];
        slot->used = 1;
    }

    if (g_cameraTraceCount >= CAMERA_TRACE_MAX_RECORDS)
        return;
    if (g_cameraTrace == INVALID_HANDLE_VALUE) {
        g_cameraTrace = CreateFileA("asi_camera_trace.bin", GENERIC_WRITE,
            FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (g_cameraTrace == INVALID_HANDLE_VALUE)
            return;
    }

    record.tick = GetTickCount();
    record.context = (unsigned int)(unsigned long)context;
    record.device = (unsigned int)(unsigned long)device;
    record.caller = (unsigned int)(unsigned long)caller;
    record.rowTranslation[0] = view[12];
    record.rowTranslation[1] = view[13];
    record.rowTranslation[2] = view[14];
    record.columnTranslation[0] = view[3];
    record.columnTranslation[1] = view[7];
    record.columnTranslation[2] = view[11];
    record.determinant = determinant;
    record.projection[0] = projection[0];
    record.projection[1] = projection[5];
    record.projection[2] = projection[10];
    record.projection[3] = projection[14];
    if (WriteFile(g_cameraTrace, &record, sizeof(record), &written, NULL) &&
        written == sizeof(record))
        g_cameraTraceCount++;
}

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

static void __fastcall Hook_TerrainRender(void *thisPtr, void *edx_unused, void *packet) {
    int wasActive = 0;
    (void)edx_unused;

    if (g_terrainRenderActive) {
        wasActive = *g_terrainRenderActive;
        *g_terrainRenderActive = 1;
    }
    g_originalTerrainRender(thisPtr, NULL, packet);
    if (g_terrainRenderActive)
        *g_terrainRenderActive = wasActive;
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
    determinant =
        viewMatrix[0] * (viewMatrix[5] * viewMatrix[10] - viewMatrix[6] * viewMatrix[9]) -
        viewMatrix[1] * (viewMatrix[4] * viewMatrix[10] - viewMatrix[6] * viewMatrix[8]) +
        viewMatrix[2] * (viewMatrix[4] * viewMatrix[9] - viewMatrix[5] * viewMatrix[8]);
    if ((tRow2 > CAMERA_MIN_TRANSLATION2 || tCol2 > CAMERA_MIN_TRANSLATION2) &&
        determinant > 0.5f)
        TraceCameraContext(thisPtr, device, _ReturnAddress(), viewMatrix,
            projMatrix, determinant);
    if (!g_mainCameraContext &&
        (tRow2 > CAMERA_MIN_TRANSLATION2 || tCol2 > CAMERA_MIN_TRANSLATION2) &&
        determinant > 0.5f) {
        g_mainCameraContext = thisPtr;
    }

    if (thisPtr == g_mainCameraContext && determinant > 0.5f) {
        fnST(device, D3DTS_VIEW, viewMatrix);
        fnST(device, D3DTS_PROJECTION, projMatrix);
    }
}

/* ====================================================================
 * Terrain path remains unmodified; the game already uses medium terrain.
 * ==================================================================== */

/* ====================================================================
 * Sky rendering is intentionally left unmodified.
 * ==================================================================== */

/* ---- Patch helpers ---- */

static int PatchBytes(BYTE *addr, const BYTE *data, int len) {
    DWORD oldProt;
    if (!VirtualProtect(addr, len, PAGE_EXECUTE_READWRITE, &oldProt))
        return 0;
    memcpy(addr, data, len);
    FlushInstructionCache(GetCurrentProcess(), addr, len);
    VirtualProtect(addr, len, oldProt, &oldProt);
    return 1;
}

static int InstallJump(HMODULE hMod, DWORD rva, void *pHook) {
    BYTE *pTarget;
    DWORD oldProt;
    int rel;

    pTarget = (BYTE*)hMod + rva;
    if (!VirtualProtect(pTarget, 5, PAGE_EXECUTE_READWRITE, &oldProt))
        return 0;
    pTarget[0] = 0xE9;
    rel = (int)((BYTE*)pHook - pTarget - 5);
    memcpy(pTarget + 1, &rel, 4);
    FlushInstructionCache(GetCurrentProcess(), pTarget, 5);
    VirtualProtect(pTarget, 5, oldProt, &oldProt);
    return 1;
}

static int InstallWeatherHook(HMODULE hMod) {
    static const BYTE expectedPrologue[5] = { 0x56, 0x53, 0x83, 0xEC, 0x14 };
    BYTE *target;
    int rel;
    int i;

    target = (BYTE*)hMod + WEATHER_UPDATE_RVA;
    for (i = 0; i < 5; i++) {
        if (target[i] != expectedPrologue[i])
            return 0;
    }

    g_weatherTrampoline = (BYTE*)VirtualAlloc(NULL, 10,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_weatherTrampoline)
        return 0;

    memcpy(g_weatherTrampoline, target, 5);
    g_weatherTrampoline[5] = 0xE9;
    rel = (int)((target + 5) - (g_weatherTrampoline + 10));
    memcpy(g_weatherTrampoline + 6, &rel, 4);
    g_originalWeatherUpdate = (FnWeatherUpdate)g_weatherTrampoline;
    if (!InstallJump(hMod, WEATHER_UPDATE_RVA, (void*)Hook_WeatherUpdate)) {
        VirtualFree(g_weatherTrampoline, 0, MEM_RELEASE);
        g_weatherTrampoline = NULL;
        g_originalWeatherUpdate = NULL;
        return 0;
    }
    return 1;
}

static int InstallTerrainRenderHook(HMODULE hMod) {
    static const BYTE expectedPrologue[7] = {
        0x57, 0x56, 0x55, 0x53, 0x83, 0xEC, 0x64
    };
    BYTE *target;
    BYTE patch[7];
    int rel;
    int i;

    target = (BYTE*)hMod + TERRAIN_RENDER_RVA;
    for (i = 0; i < 7; i++) {
        if (target[i] != expectedPrologue[i])
            return 0;
    }

    g_terrainTrampoline = (BYTE*)VirtualAlloc(NULL, 12,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_terrainTrampoline)
        return 0;

    memcpy(g_terrainTrampoline, target, 7);
    g_terrainTrampoline[7] = 0xE9;
    rel = (int)((target + 7) - (g_terrainTrampoline + 12));
    memcpy(g_terrainTrampoline + 8, &rel, 4);
    g_originalTerrainRender = (FnTerrainRender)g_terrainTrampoline;

    patch[0] = 0xE9;
    rel = (int)((BYTE*)Hook_TerrainRender - target - 5);
    memcpy(patch + 1, &rel, 4);
    patch[5] = 0x90;
    patch[6] = 0x90;
    if (!PatchBytes(target, patch, 7)) {
        VirtualFree(g_terrainTrampoline, 0, MEM_RELEASE);
        g_terrainTrampoline = NULL;
        g_originalTerrainRender = NULL;
        return 0;
    }
    return 1;
}

/* ---- Install terrain patches ---- */

static int InstallTerrainPatches(HMODULE hMod) {
    HMODULE hProxy = GetModuleHandleA("d3d9.dll");
    if (hProxy) {
        g_terrainRenderActive = (volatile int*)GetProcAddress(
            hProxy, "g_terrainRenderActive");
    }
    if (!g_terrainRenderActive)
        return 0;
    return InstallTerrainRenderHook(hMod);
}

/* ---- Install render hooks ---- */

static int InstallRenderHooks(HMODULE hMod) {
    return InstallJump(hMod, 0x7180, (void*)Hook_SetRenderMatrices);
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
                renderDone = InstallRenderHooks(hRenderD3D);
            }
        }
        if (!terrainDone) {
            hTerrainShader = GetModuleHandleA("TTerrainShaderD3D.dll");
            if (hTerrainShader) {
                terrainDone = InstallTerrainPatches(hTerrainShader);
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
    HANDLE thread;

    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hDll);
        thread = CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
        if (thread)
            CloseHandle(thread);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_cameraTrace != INVALID_HANDLE_VALUE) {
            CloseHandle(g_cameraTrace);
            g_cameraTrace = INVALID_HANDLE_VALUE;
        }
        if (g_weatherTrampoline) {
            VirtualFree(g_weatherTrampoline, 0, MEM_RELEASE);
            g_weatherTrampoline = NULL;
        }
        if (g_terrainTrampoline) {
            VirtualFree(g_terrainTrampoline, 0, MEM_RELEASE);
            g_terrainTrampoline = NULL;
        }
    }
    return TRUE;
}

int _fltused = 0;
