/*
 * JPOG RTX Remix ASI Plugin
 *
 * Hooks loaded by dxwrapper from the game folder:
 *
 * 1. View Matrix Fix (TRenderD3DInterface.dll)
 *    Hooks SetRenderMatrices to pass the real camera View matrix instead
 *    of the identity the engine sends. Remix needs this for camera tracking.
 *
 * 2. Weather Preset Sync (simjp.exe)
 *    Maps JPOG's active weather index to the matching Remix weather preset.
 *
 * 3. Terrain Texture Detection (TTerrainShaderD3D.dll)
 *    Brackets terrain rendering so the D3D9 proxy can tag generated island
 *    textures as Remix terrain without relying on unstable content hashes.
 *
 * Terrain quality and sky rendering remain owned by the game and Remix.
 *
 * RE discoveries (Toshi engine):
 *   TRenderContext: +0x8C = View, +0x484 = Projection
 *   TRenderPacket:  +0x08 = resource, +0x0C = world matrix (4x4 float)
 *   Toshi device vtable: +0x94 = SetTransform(__stdcall)
 *   SetRenderMatrices: RVA 0x7180  in TRenderD3DInterface.dll
 *   Terrain Render:    RVA 0x5790 in TTerrainShaderD3D.dll
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define REMIX_ALLOW_X86
#define REMIX_WINAPI_NO_LIBRARY_LOADER
#include "remix/remix_c.h"

int InstallTerrainWaterResetHook(HMODULE module);

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
#define TERRAIN_COMPUTE_LOD_RVA  0x7470
#define TERRAIN_LOD_COUNTER_RVA  0x123C4
#define SYSSHADER_FLUSH_RVA      0x19B0
#define SYSSHADER_RENDER_RVA     0x22D0
#define SYSSHADER_CAP_HIGHEND_RVA 0x27D0
#define SYSSHADER_CAP_SCATTER_RVA 0x2830
#define SYSSHADER_OFF_HIGHEND    0x3C
#define SYSSHADER_OFF_SCATTER    0x3D
#define CAMERA_MIN_TRANSLATION2  2500.0f

typedef int (__stdcall *FnSetTransform)(void*, int, void*);
typedef int (__fastcall *FnWeatherUpdate)(void*, void*, float);
typedef void (__fastcall *FnTerrainRender)(void*, void*, void*);
typedef void (__fastcall *FnSysShaderFlush)(void*, void*);
typedef void (__fastcall *FnSysShaderRender)(void*, void*, void*);
typedef void (__fastcall *FnDestroyRenderContext)(void*, void*);

/* ====================================================================
 * Hook 1: View Matrix Fix (TRenderD3DInterface.dll)
 * ==================================================================== */

static void *g_mainCameraContext = NULL;
static FnDestroyRenderContext g_originalDestroyRenderContext = NULL;
static BYTE *g_contextDestroyTrampoline = NULL;
static FnWeatherUpdate g_originalWeatherUpdate = NULL;
static FnTerrainRender g_originalTerrainRender = NULL;
static FnSysShaderFlush g_originalSysShaderFlush = NULL;
static FnSysShaderRender g_originalSysShaderRender = NULL;
static BYTE *g_weatherTrampoline = NULL;
static BYTE *g_terrainTrampoline = NULL;
static BYTE *g_sysShaderFlushTrampoline = NULL;
static BYTE *g_sysShaderRenderTrampoline = NULL;
static volatile int *g_terrainRenderActive = NULL;
static volatile int *g_mvWorldTransformActive = NULL;
static volatile unsigned int *g_terrainLodCounter = NULL;
static remixapi_Interface g_remix = { 0 };
static int g_lastWeatherIndex = -1;

static int InitializeRemixApi(void) {
    HMODULE hRemix;
    PFN_remixapi_InitializeLibrary initializeLibrary;
    remixapi_InitializeLibraryInfo info;

    if (g_remix.SetConfigVariable)
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
    return g_remix.SetConfigVariable != NULL;
}

static void SyncRemixWeather(void *weatherController) {
    static const char *presets[] = {
        "jpog.clear",
        "jpog.rain",
        "jpog.storm",
        "jpog.tornado",
        "jpog.sandstorm"
    };
    int weatherIndex;

    weatherIndex = *(int*)((char*)weatherController + WEATHER_OFF_ACTIVE);
    if (weatherIndex < 0 || weatherIndex >= 5 || weatherIndex == g_lastWeatherIndex)
        return;
    if (!InitializeRemixApi())
        return;
    if (g_remix.SetConfigVariable("rtx.weather.gameWeather", presets[weatherIndex]) != REMIXAPI_ERROR_CODE_SUCCESS)
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

static void __fastcall Hook_SysShaderFlush(void *thisPtr, void *edx_unused) {
    int wasActive = *g_mvWorldTransformActive;
    (void)edx_unused;

    /* Enforce FFP mode even if Validate() ran before capability patches installed. */
    *((BYTE *)thisPtr + SYSSHADER_OFF_HIGHEND) = 0;
    *((BYTE *)thisPtr + SYSSHADER_OFF_SCATTER) = 0;
    *g_mvWorldTransformActive = 1;
    g_originalSysShaderFlush(thisPtr, NULL);
    *g_mvWorldTransformActive = wasActive;
}

static void __fastcall Hook_SysShaderRender(void *thisPtr, void *edx_unused, void *packet) {
    int wasActive = *g_mvWorldTransformActive;
    (void)edx_unused;

    *g_mvWorldTransformActive = 1;
    g_originalSysShaderRender(thisPtr, NULL, packet);
    *g_mvWorldTransformActive = wasActive;
}

/* Lock every terrain section to LOD 0 so tiles stay single full-tile draws
 * instead of splitting/merging into 4 stitch quadrants during camera movement. */
static signed char __fastcall Hook_TerrainComputeLOD(void *thisPtr, void *edx_unused,
    void *camPos, signed char *outMinLOD)
{
    unsigned int *flags;
    (void)edx_unused;
    (void)camPos;

    flags = (unsigned int *)((char *)thisPtr + 0x18);
    *flags = (*flags & 0x000003FCu) | *g_terrainLodCounter;
    if (outMinLOD)
        *outMinLOD = 0;
    return 0;
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

    (void)edx_unused;
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

/* The context owns camera selection, so destruction releases that selection. */
static void __fastcall Hook_DestroyRenderContext(void *context, void *unused) {
    (void)unused;
    if (context == g_mainCameraContext)
        g_mainCameraContext = NULL;
    g_originalDestroyRenderContext(context, NULL);
}

static int MatchModuleBytes(HMODULE module, DWORD rva, const BYTE *expected, DWORD length) {
    const BYTE *base = (const BYTE*)module;
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER*)base;
    const IMAGE_NT_HEADERS *nt;
    DWORD i;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0)
        return 0;
    nt = (const IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || rva > nt->OptionalHeader.SizeOfImage ||
        length > nt->OptionalHeader.SizeOfImage - rva)
        return 0;
    for (i = 0; i < length; i++)
        if (base[rva + i] != expected[i])
            return 0;
    return 1;
}

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
    FlushInstructionCache(GetCurrentProcess(), g_weatherTrampoline, 10);
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
    FlushInstructionCache(GetCurrentProcess(), g_terrainTrampoline, 12);
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

/* ---- Install terrain tagging hook ---- */

static int InstallTerrainTagHook(HMODULE hMod) {
    HMODULE hProxy = GetModuleHandleA("d3d9.dll");
    if (hProxy) {
        g_terrainRenderActive = (volatile int*)GetProcAddress(
            hProxy, "g_terrainRenderActive");
    }
    if (!g_terrainRenderActive)
        return 0;
    return InstallTerrainRenderHook(hMod);
}

static int InstallTerrainLodHook(HMODULE hMod) {
    static const BYTE expectedPrologue[6] = { 0x56, 0x55, 0x53, 0x83, 0xEC, 0x30 };
    BYTE patch[6];
    BYTE *target;
    int rel;

    if (!MatchModuleBytes(hMod, TERRAIN_COMPUTE_LOD_RVA, expectedPrologue, 6)) {
        OutputDebugStringA("JPOG ASI: unsupported TTerrainInterface ComputeLOD signature; LOD lock skipped.\n");
        return 0;
    }
    g_terrainLodCounter = (volatile unsigned int *)GetProcAddress(
        hMod, "?sm_ui32Counter@TTerrainSection@Toshi@@2IA");
    if (!g_terrainLodCounter)
        g_terrainLodCounter = (volatile unsigned int *)((BYTE *)hMod + TERRAIN_LOD_COUNTER_RVA);

    target = (BYTE *)hMod + TERRAIN_COMPUTE_LOD_RVA;
    patch[0] = 0xE9;
    rel = (int)((BYTE *)Hook_TerrainComputeLOD - target - 5);
    memcpy(patch + 1, &rel, 4);
    patch[5] = 0x90;
    return PatchBytes(target, patch, 6);
}

static int InstallTrampoline10(BYTE *target, void *hook, BYTE **outTrampoline, void **outOriginal) {
    BYTE patch[10];
    BYTE *trampoline;
    int rel;
    int i;

    trampoline = (BYTE *)VirtualAlloc(NULL, 15,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline)
        return 0;

    memcpy(trampoline, target, 10);
    trampoline[10] = 0xE9;
    rel = (int)((target + 10) - (trampoline + 15));
    memcpy(trampoline + 11, &rel, 4);
    FlushInstructionCache(GetCurrentProcess(), trampoline, 15);

    /* Publish the trampoline before redirecting target so the render thread
     * can never invoke the hook while the original pointer is still NULL. */
    *outTrampoline = trampoline;
    *outOriginal = (void *)trampoline;
    MemoryBarrier();

    patch[0] = 0xE9;
    rel = (int)((BYTE *)hook - target - 5);
    memcpy(patch + 1, &rel, 4);
    for (i = 5; i < 10; i++)
        patch[i] = 0x90;
    if (!PatchBytes(target, patch, 10)) {
        *outOriginal = NULL;
        *outTrampoline = NULL;
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return 0;
    }
    return 1;
}

/* Switch TSysShaderD3D (trees, buildings, fences, props) to its built-in
 * FVF 0x142 fixed-function path and signal the proxy to decompose its
 * ModelView D3DTS_WORLD matrices into world space. */
static int InstallSysShaderFfpHooks(HMODULE hMod) {
    static const BYTE flushPrologue[10] = {
        0x57, 0x56, 0x55, 0x53, 0x81, 0xEC, 0x84, 0x00, 0x00, 0x00
    };
    static const BYTE renderPrologue[10] = {
        0x57, 0x56, 0x55, 0x53, 0x81, 0xEC, 0xF4, 0x00, 0x00, 0x00
    };
    static const BYTE capPrologue[6] = {
        0x81, 0xEC, 0xD4, 0x00, 0x00, 0x00
    };
    static const BYTE retZero[3] = { 0x33, 0xC0, 0xC3 };
    HMODULE hProxy = GetModuleHandleA("d3d9.dll");

    if (!hProxy)
        return 0;
    g_mvWorldTransformActive = (volatile int *)GetProcAddress(
        hProxy, "g_mvWorldTransformActive");
    if (!g_mvWorldTransformActive) {
        OutputDebugStringA("JPOG ASI: proxy lacks g_mvWorldTransformActive; TSysShader FFP skipped.\n");
        return 1;
    }
    if (!MatchModuleBytes(hMod, SYSSHADER_FLUSH_RVA, flushPrologue, 10) ||
        !MatchModuleBytes(hMod, SYSSHADER_RENDER_RVA, renderPrologue, 10) ||
        !MatchModuleBytes(hMod, SYSSHADER_CAP_HIGHEND_RVA, capPrologue, 6) ||
        !MatchModuleBytes(hMod, SYSSHADER_CAP_SCATTER_RVA, capPrologue, 6)) {
        OutputDebugStringA("JPOG ASI: unsupported TSysShaderD3D signatures; FFP switch skipped.\n");
        return 1;
    }

    if (!InstallTrampoline10((BYTE *)hMod + SYSSHADER_RENDER_RVA,
            (void *)Hook_SysShaderRender, &g_sysShaderRenderTrampoline,
            (void **)&g_originalSysShaderRender))
        return 1;

    if (!InstallTrampoline10((BYTE *)hMod + SYSSHADER_FLUSH_RVA,
            (void *)Hook_SysShaderFlush, &g_sysShaderFlushTrampoline,
            (void **)&g_originalSysShaderFlush))
        return 1;

    PatchBytes((BYTE *)hMod + SYSSHADER_CAP_HIGHEND_RVA, retZero, 3);
    PatchBytes((BYTE *)hMod + SYSSHADER_CAP_SCATTER_RVA, retZero, 3);
    return 1;
}

/* ---- Install render hooks ---- */

static int InstallRenderHooks(HMODULE hMod) {
    static const BYTE matrixPrologue[6] = { 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x04 };
    BYTE destroyPrologue[6] = { 0xC7, 0x01, 0, 0, 0, 0 };
    BYTE destroyPatch[6];
    BYTE matrixPatch[6];
    BYTE *destroyTarget = (BYTE*)hMod + 0x71C0;
    DWORD contextVtable = (DWORD)((BYTE*)hMod + 0x74480);
    int relative;

    memcpy(destroyPrologue + 2, &contextVtable, 4);
    if (!MatchModuleBytes(hMod, 0x7180, matrixPrologue, 6) ||
        !MatchModuleBytes(hMod, 0x71C0, destroyPrologue, 6)) {
        OutputDebugStringA("JPOG ASI: unsupported render-context signatures; camera hooks skipped.\n");
        return 0;
    }
    g_contextDestroyTrampoline = (BYTE*)VirtualAlloc(NULL, 11,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_contextDestroyTrampoline)
        return 0;
    memcpy(g_contextDestroyTrampoline, destroyPrologue, 6);
    g_contextDestroyTrampoline[6] = 0xE9;
    relative = (int)(destroyTarget + 6 - (g_contextDestroyTrampoline + 11));
    memcpy(g_contextDestroyTrampoline + 7, &relative, 4);
    FlushInstructionCache(GetCurrentProcess(), g_contextDestroyTrampoline, 11);
    g_originalDestroyRenderContext = (FnDestroyRenderContext)g_contextDestroyTrampoline;

    destroyPatch[0] = 0xE9;
    relative = (int)((BYTE*)Hook_DestroyRenderContext - destroyTarget - 5);
    memcpy(destroyPatch + 1, &relative, 4);
    destroyPatch[5] = 0x90;
    matrixPatch[0] = 0xE9;
    relative = (int)((BYTE*)Hook_SetRenderMatrices - ((BYTE*)hMod + 0x7180) - 5);
    memcpy(matrixPatch + 1, &relative, 4);
    matrixPatch[5] = 0x90;
    if (!PatchBytes(destroyTarget, destroyPatch, 6)) {
        VirtualFree(g_contextDestroyTrampoline, 0, MEM_RELEASE);
        g_contextDestroyTrampoline = NULL;
        g_originalDestroyRenderContext = NULL;
        return 0;
    }
    if (!PatchBytes((BYTE*)hMod + 0x7180, matrixPatch, 6)) {
        /* The destructor trampoline stays live even if only its hook installed. */
        OutputDebugStringA("JPOG ASI: camera matrix patch failed.\n");
        return 0;
    }
    return 1;
}

/* ---- Delayed init thread ---- */

static DWORD WINAPI InitThread(LPVOID param) {
    HMODULE hSimJp;
    HMODULE hRenderD3D = NULL;
    HMODULE hTerrainShader = NULL;
    HMODULE hTerrainIface = NULL;
    HMODULE hSysShader = NULL;
    int renderAttempted = 0, terrainDone = 0, waterResetDone = 0;
    int terrainLodAttempted = 0, sysShaderAttempted = 0;
    int i;

    (void)param;
    hSimJp = GetModuleHandleA(NULL);
    if (hSimJp)
        InstallWeatherHook(hSimJp);

    for (i = 0; i < 300; i++) {  /* 30 seconds max */
        if (!renderAttempted) {
            hRenderD3D = GetModuleHandleA("TRenderD3DInterface.dll");
            if (hRenderD3D) {
                InstallRenderHooks(hRenderD3D);
                renderAttempted = 1;
            }
        }
        if (!terrainLodAttempted) {
            hTerrainIface = GetModuleHandleA("TTerrainInterface.dll");
            if (hTerrainIface) {
                InstallTerrainLodHook(hTerrainIface);
                terrainLodAttempted = 1;
            }
        }
        if (!sysShaderAttempted) {
            hSysShader = GetModuleHandleA("TSysShaderD3D.dll");
            if (hSysShader)
                sysShaderAttempted = InstallSysShaderFfpHooks(hSysShader);
        }
        if (!terrainDone || !waterResetDone) {
            hTerrainShader = GetModuleHandleA("TTerrainShaderD3D.dll");
            if (hTerrainShader) {
                if (!terrainDone)
                    terrainDone = InstallTerrainTagHook(hTerrainShader);
                if (!waterResetDone)
                    waterResetDone = InstallTerrainWaterResetHook(hTerrainShader);
            }
        }
        if (renderAttempted && terrainLodAttempted && sysShaderAttempted &&
            terrainDone && waterResetDone)
            return 0;
        Sleep(100);
    }
    return 0;
}

/* ---- DLL entry point ---- */

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID reserved) {
    HANDLE thread;
    HMODULE pinnedModule;

    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        /* Game modules keep jumps into this ASI for the process lifetime. */
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCSTR)hDll, &pinnedModule))
            return FALSE;
        DisableThreadLibraryCalls(hDll);
        thread = CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}

int _fltused = 0;
