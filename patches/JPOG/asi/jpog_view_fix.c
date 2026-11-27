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

/* ---- No-CRT byte copy (avoids MSVC /GL C2268 on redefining memcpy) ---- */
static void CopyBytes(void *dst, const void *src, unsigned int n) {
    volatile unsigned char *d = (volatile unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
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
#define FLARE_FAR_RECIP_RVA      0x2BBE50
#define FLARE_BASE_FADE_RVA      0x2BC7FD
#define FLARE_GROUP_FADE_RVA     0x2BAE9B
#define FLARE_BLDG_DIST_RVA      0x0C608B
#define PARTICLE_MAXVERTS_RVA    0x2CFA9A
#define FLARE_POOL_LOW_RVA       0x2D1C29
#define FLARE_POOL_MED_RVA       0x2D1D9B
#define FLARE_POOL_HIGH_RVA      0x2D1F4D
#define FLARE_CHUNK_INIT_RVA     0x2CE8B0
#define FLARE_CHUNK_REALLOC_RVA  0x2CEBF4
#define GROUP_LENS_FLARES_RVA    0x450038
#define FLARE_BLDG_WEATHER1_RVA  0x0C8246
#define FLARE_BLDG_WEATHER2_RVA  0x0C8387
#define FLARE_HELI_WEATHER_RVA   0x0D044A
#define FLARE_VEH_WEATHER_RVA    0x0D61EA
#define TERRAIN_RENDER_RVA           0x5790
#define TERRAIN_WATER_FRAME_LOAD_RVA 0x8A32
#define TERRAIN_WATER_FRAME_IDX_RVA  0x14C2C4
#define GRASS_FLUSH_RVA              0x11F50
#define GRASS_RENDER_RVA         0x12D70
#define TERRAIN_COMPUTE_LOD_RVA  0x7470
#define TERRAIN_LOD_COUNTER_RVA  0x123C4
#define TERRAIN_CACHE_MOV1_RVA   0x8A11
#define TERRAIN_CACHE_MOV2_RVA   0x8A16
#define TERRAIN_CACHE_MOV3_RVA   0x8A1B
#define TERRAIN_CACHE_STORE1_RVA 0x8A2A
#define TERRAIN_CACHE_STORE2_RVA 0x8A34
#define SYSSHADER_FLUSH_RVA      0x19B0
#define SYSSHADER_RENDER_RVA     0x22D0
#define SYSSHADER_CAP_HIGHEND_RVA 0x27D0
#define SYSSHADER_CAP_SCATTER_RVA 0x2830
#define SYSSHADER_OFF_HIGHEND    0x3C
#define SYSSHADER_OFF_SCATTER    0x3D
#define SKINSHADER_FLUSH_RVA     0x1B60
#define SKINSHADER_RENDER_RVA    0x2570
#define SKINSHADER_VB_CREATE_RVA   0x3340
#define SKINSHADER_VB_SIZE_RVA     0x334B
#define SKINSHADER_CAP_HIGHEND_RVA 0x2120
#define SKINSHADER_CAP_SCATTER_RVA 0x2190
#define SKINSHADER_OFF_ENVMAP    0x6C
#define SKINSHADER_OFF_HIGHEND   0xB0
#define SKINSHADER_OFF_SCATTER   0xB1
#define SPRITESHADER_FLUSH_RVA    0x2660
#define SPRITESHADER_RENDER_RVA   0x2A30
#define SPRITESHADER_OFF_VSHANDLE 0x128
#define SPRITESHADER_OFF_USEVS    0x12C
#define CAMERA_MIN_TRANSLATION2  2500.0f

typedef int (__stdcall *FnSetTransform)(void*, int, void*);
typedef int (__fastcall *FnWeatherUpdate)(void*, void*, float);
typedef void (__fastcall *FnTerrainRender)(void*, void*, void*);
typedef void (__fastcall *FnGrassFlush)(void*, void*);
typedef void (__fastcall *FnGrassRender)(void*, void*, void*);
typedef void (__fastcall *FnSysShaderFlush)(void*, void*);
typedef void (__fastcall *FnSysShaderRender)(void*, void*, void*);
typedef void (__fastcall *FnDestroyRenderContext)(void*, void*);
typedef void* (__fastcall *FnGetCurrentRenderContext)(void*, void*);

/* ====================================================================
 * Hook 1: View Matrix Fix (TRenderD3DInterface.dll)
 * ==================================================================== */

static void *g_mainCameraContext = NULL;
static FnGetCurrentRenderContext g_getCurrentRenderContext = NULL;
static FnDestroyRenderContext g_originalDestroyRenderContext = NULL;
static BYTE *g_contextDestroyTrampoline = NULL;
static FnWeatherUpdate g_originalWeatherUpdate = NULL;
static FnTerrainRender g_originalTerrainRender = NULL;
static FnGrassFlush g_originalGrassFlush = NULL;
static FnGrassRender g_originalGrassRender = NULL;
static FnSysShaderFlush g_originalSysShaderFlush = NULL;
static FnSysShaderRender g_originalSysShaderRender = NULL;
static FnSysShaderFlush g_originalSkinShaderFlush = NULL;
static FnSysShaderRender g_originalSkinShaderRender = NULL;
static FnSysShaderFlush g_originalSpriteShaderFlush = NULL;
static FnSysShaderRender g_originalSpriteShaderRender = NULL;
static BYTE *g_weatherTrampoline = NULL;
static BYTE *g_terrainTrampoline = NULL;
static BYTE *g_grassFlushTrampoline = NULL;
static BYTE *g_grassRenderTrampoline = NULL;
static BYTE *g_sysShaderFlushTrampoline = NULL;
static BYTE *g_sysShaderRenderTrampoline = NULL;
static BYTE *g_skinShaderFlushTrampoline = NULL;
static BYTE *g_skinShaderRenderTrampoline = NULL;
static BYTE *g_spriteShaderFlushTrampoline = NULL;
static BYTE *g_spriteShaderRenderTrampoline = NULL;
static volatile int *g_terrainRenderActive = NULL;
static volatile int *g_mvWorldTransformActive = NULL;
static volatile int *g_viewSyncedThisFrame = NULL;
static volatile int *g_grassFlushState = NULL;
static float g_lastPushedView[16];
static int g_hasLastPushedView = 0;
static volatile unsigned int *g_terrainLodCounter = NULL;
static remixapi_Interface g_remix = { 0 };
static int g_lastWeatherIndex = -1;
static HMODULE g_hSimJp = NULL;
static volatile int g_forceNightFlares = 0;
static int g_nightFlarePolled = 0;
static int g_nightFlarePollFrames = 0;
static BYTE *g_nightFlareStubPage = NULL;

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

static void SyncNumosNightFlares(void *weatherController) {
    char buf[8];
    uint32_t actualSize = 0;
    int prevForce = g_forceNightFlares;
    int nextForce = prevForce;
    int weatherIndex;
    int nightChanged;

    if (!g_nightFlarePolled || ++g_nightFlarePollFrames >= 15) {
        g_nightFlarePollFrames = 0;
        if (InitializeRemixApi() && g_remix.GetGameValue) {
            if (g_remix.GetGameValue("__numos.force_night_flares", buf, sizeof(buf), &actualSize) == REMIXAPI_ERROR_CODE_SUCCESS) {
                nextForce = (actualSize > 1 && buf[0] == '1') ? 1 : 0;
                g_nightFlarePolled = 1;
            }
        }
    }

    nightChanged = (nextForce != prevForce);
    g_forceNightFlares = nextForce;

    if (!g_hSimJp)
        return;
    {
        BYTE *groupFlares = *(BYTE **)((BYTE *)g_hSimJp + GROUP_LENS_FLARES_RVA);
        if (groupFlares) {
            int *fadeState = (int *)(groupFlares + 0x4C);
            float *fadeVal = (float *)(groupFlares + 0x50);
            float *fadeRate = (float *)(groupFlares + 0x54);
            int shouldBeOn;
            weatherIndex = *(int *)((char *)weatherController + WEATHER_OFF_ACTIVE);
            shouldBeOn = nextForce || (weatherIndex == 1 || weatherIndex == 2);
            if (shouldBeOn) {
                if (*fadeState == 1 || (*fadeVal < 1.0f && *fadeState != 0)) {
                    *fadeRate = 0.2f;
                    *fadeState = 0;
                }
            } else if (nightChanged) {
                if (*fadeVal > 0.0f && *fadeState != 1) {
                    *fadeRate = 0.2f;
                    *fadeState = 1;
                }
            }
        }
    }
}

static int __fastcall Hook_WeatherUpdate(void *thisPtr, void *edx_unused, float deltaTime) {
    int result;
    (void)edx_unused;

    result = g_originalWeatherUpdate(thisPtr, NULL, deltaTime);
    SyncRemixWeather(thisPtr);
    SyncNumosNightFlares(thisPtr);
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

/* Allow only the first visible legacy grass draw per Flush so Remix keeps
 * receiving the live stage-1 grass coverage map while skipping hundreds of
 * redundant hidden programmable-VS grass patch draws. */
static void __fastcall Hook_GrassFlush(void *thisPtr, void *edx_unused) {
    int prevState = *g_grassFlushState;
    (void)edx_unused;

    *g_grassFlushState = 1;
    g_originalGrassFlush(thisPtr, NULL);
    *g_grassFlushState = prevState;
}

static void __fastcall Hook_GrassRender(void *thisPtr, void *edx_unused, void *packet) {
    (void)edx_unused;
    if (*g_grassFlushState >= 2)
        return;
    g_originalGrassRender(thisPtr, NULL, packet);
}

static int MatrixEquals16(const float *a, const float *b) {
    int i;
    for (i = 0; i < 16; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

static void PushContextViewMatrices(void *ctx) {
    void **ppIface;
    void *device;
    void **vtbl;
    FnSetTransform fnST;
    float *viewMatrix;
    float *projMatrix;
    float determinant;

    ppIface = *(void **)((char *)ctx + RCTX_OFF_IFACE);
    if (!ppIface) return;
    device  = *(void **)((char *)ppIface + IFACE_OFF_DEVICE);
    if (!device) return;
    vtbl    = *(void ***)device;
    fnST    = (FnSetTransform)vtbl[D3D8_VT_SETTRANSFORM / 4];

    viewMatrix = (float *)((char *)ctx + RCTX_OFF_WORLDVIEW);
    projMatrix = (float *)((char *)ctx + RCTX_OFF_PROJECTION);
    determinant =
        viewMatrix[0] * (viewMatrix[5] * viewMatrix[10] - viewMatrix[6] * viewMatrix[9]) -
        viewMatrix[1] * (viewMatrix[4] * viewMatrix[10] - viewMatrix[6] * viewMatrix[8]) +
        viewMatrix[2] * (viewMatrix[4] * viewMatrix[9] - viewMatrix[5] * viewMatrix[8]);
    if (determinant > 0.5f) {
        fnST(device, D3DTS_VIEW, viewMatrix);
        fnST(device, D3DTS_PROJECTION, projMatrix);
        CopyBytes(g_lastPushedView, viewMatrix, sizeof(g_lastPushedView));
        g_hasLastPushedView = 1;
    }
}

/* Sync the main camera's live View matrix before a shader flush/render runs so
 * the proxy's inv(View) never lags a frame behind packet ModelView matrices.
 * Returns 1 when drawing in the main camera pass (positive-determinant View),
 * 0 for auxiliary contexts or mirrored passes (e.g. water reflection, det < 0). */
static int SyncShaderMainCameraContext(void *shaderThis, int forcePush) {
    void *renderIface;
    void *curCtx;
    float *viewMatrix;
    float determinant;

    if (!g_getCurrentRenderContext || !g_mainCameraContext)
        return 1;
    renderIface = *(void **)((char *)shaderThis + 0x18);
    if (!renderIface)
        return 1;
    curCtx = g_getCurrentRenderContext(renderIface, NULL);
    if (curCtx != g_mainCameraContext)
        return 0;
    viewMatrix = (float *)((char *)curCtx + RCTX_OFF_WORLDVIEW);
    determinant =
        viewMatrix[0] * (viewMatrix[5] * viewMatrix[10] - viewMatrix[6] * viewMatrix[9]) -
        viewMatrix[1] * (viewMatrix[4] * viewMatrix[10] - viewMatrix[6] * viewMatrix[8]) +
        viewMatrix[2] * (viewMatrix[4] * viewMatrix[9] - viewMatrix[5] * viewMatrix[8]);
    if (determinant <= 0.5f)
        return 0;
    if (forcePush || !g_viewSyncedThisFrame || !*g_viewSyncedThisFrame ||
        !g_hasLastPushedView || !MatrixEquals16(g_lastPushedView, viewMatrix)) {
        PushContextViewMatrices(curCtx);
    }
    return 1;
}

static void __fastcall Hook_SysShaderFlush(void *thisPtr, void *edx_unused) {
    int wasActive = *g_mvWorldTransformActive;
    (void)edx_unused;

    /* Enforce FFP mode even if Validate() ran before capability patches installed. */
    *((BYTE *)thisPtr + SYSSHADER_OFF_HIGHEND) = 0;
    *((BYTE *)thisPtr + SYSSHADER_OFF_SCATTER) = 0;
    *g_mvWorldTransformActive = SyncShaderMainCameraContext(thisPtr, 1) ? (1 | 4) : 1;
    g_originalSysShaderFlush(thisPtr, NULL);
    *g_mvWorldTransformActive = wasActive;
}

static void __fastcall Hook_SysShaderRender(void *thisPtr, void *edx_unused, void *packet) {
    int wasActive = *g_mvWorldTransformActive;
    (void)edx_unused;

    *g_mvWorldTransformActive = SyncShaderMainCameraContext(thisPtr, 0) ? (1 | 4) : 1;
    g_originalSysShaderRender(thisPtr, NULL, packet);
    *g_mvWorldTransformActive = wasActive;
}

static void __fastcall Hook_SkinShaderFlush(void *thisPtr, void *edx_unused) {
    int wasActive = *g_mvWorldTransformActive;
    (void)edx_unused;

    /* Enforce FVF 0x1D2 FFP path and disable envmap/scattering PS even if
     * Validate() ran before capability patches installed. */
    *((BYTE *)thisPtr + SKINSHADER_OFF_ENVMAP) = 0;
    *((BYTE *)thisPtr + SKINSHADER_OFF_HIGHEND) = 0;
    *((BYTE *)thisPtr + SKINSHADER_OFF_SCATTER) = 0;
    *g_mvWorldTransformActive = SyncShaderMainCameraContext(thisPtr, 1) ? (2 | 4) : 2;
    g_originalSkinShaderFlush(thisPtr, NULL);
    *g_mvWorldTransformActive = wasActive;
}

static void __fastcall Hook_SkinShaderRender(void *thisPtr, void *edx_unused, void *packet) {
    int wasActive = *g_mvWorldTransformActive;
    (void)edx_unused;

    *g_mvWorldTransformActive = SyncShaderMainCameraContext(thisPtr, 0) ? (2 | 4) : 2;
    g_originalSkinShaderRender(thisPtr, NULL, packet);
    *g_mvWorldTransformActive = wasActive;
}

/* Switch 3D world-space TSpriteShaderD3D draws (particles, flares, rain, water rings)
 * to the built-in FVF 0x142 (XYZ | DIFFUSE | TEX1) fixed-function path so Remix
 * receives per-vertex COLOR0 and preserves each material's blend mode. 2D UI passes
 * on auxiliary render contexts keep the original VS so blocked identity D3DTS_VIEW
 * never transforms screen-space HUD elements by the 3D camera. */
static void __fastcall Hook_SpriteShaderFlush(void *thisPtr, void *edx_unused) {
    typedef unsigned char (__fastcall *FnValidate)(void *, void *);
    int wasActive = *g_mvWorldTransformActive;
    DWORD savedHandle;
    BYTE savedUseVs;
    int isMain3D;
    (void)edx_unused;

    savedHandle = *(DWORD *)((BYTE *)thisPtr + SPRITESHADER_OFF_VSHANDLE);
    savedUseVs = *(BYTE *)((BYTE *)thisPtr + SPRITESHADER_OFF_USEVS);
    if (savedHandle == 0) {
        ((FnValidate)(*(void ***)thisPtr)[4])(thisPtr, NULL);
        savedHandle = *(DWORD *)((BYTE *)thisPtr + SPRITESHADER_OFF_VSHANDLE);
        savedUseVs = *(BYTE *)((BYTE *)thisPtr + SPRITESHADER_OFF_USEVS);
    }

    isMain3D = SyncShaderMainCameraContext(thisPtr, 1);
    if (isMain3D) {
        *(BYTE *)((BYTE *)thisPtr + SPRITESHADER_OFF_USEVS) = 0;
        *(DWORD *)((BYTE *)thisPtr + SPRITESHADER_OFF_VSHANDLE) = 0x142u;
        *g_mvWorldTransformActive = 8;
    } else {
        *g_mvWorldTransformActive = 0;
    }

    g_originalSpriteShaderFlush(thisPtr, NULL);

    *(DWORD *)((BYTE *)thisPtr + SPRITESHADER_OFF_VSHANDLE) = savedHandle;
    *(BYTE *)((BYTE *)thisPtr + SPRITESHADER_OFF_USEVS) = savedUseVs;
    *g_mvWorldTransformActive = wasActive;
}

static void __fastcall Hook_SpriteShaderRender(void *thisPtr, void *edx_unused, void *packet) {
    int wasActive = *g_mvWorldTransformActive;
    (void)edx_unused;

    if (wasActive & 8) {
        g_originalSpriteShaderRender(thisPtr, NULL, packet);
        return;
    }
    *g_mvWorldTransformActive = SyncShaderMainCameraContext(thisPtr, 0) ? 8 : 0;
    g_originalSpriteShaderRender(thisPtr, NULL, packet);
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
        CopyBytes(g_lastPushedView, viewMatrix, sizeof(g_lastPushedView));
        g_hasLastPushedView = 1;
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
    CopyBytes(addr, data, len);
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
    CopyBytes(pTarget + 1, &rel, 4);
    FlushInstructionCache(GetCurrentProcess(), pTarget, 5);
    VirtualProtect(pTarget, 5, oldProt, &oldProt);
    return 1;
}

/* Emit a 36-byte gate stub that branches to onTarget whenever g_forceNightFlares
 * is non-zero, otherwise executing the original `cmp reg, 1/2` weather check. */
static int InstallNightFlareGateStub(BYTE *site, int siteLen, BYTE cmpModRM,
    BYTE *onTarget, BYTE *offTarget, BYTE *stub)
{
    BYTE patch[16];
    DWORD flagAddr = (DWORD)&g_forceNightFlares;
    int i;

    if (siteLen < 5 || siteLen > (int)sizeof(patch))
        return 0;

    stub[0] = 0x83;
    stub[1] = 0x3D;
    *(DWORD *)(stub + 2) = flagAddr;
    stub[6] = 0x00;

    stub[7] = 0x0F;
    stub[8] = 0x85;
    *(int *)(stub + 9) = (int)(onTarget - (stub + 13));

    stub[13] = 0x83;
    stub[14] = cmpModRM;
    stub[15] = 0x01;

    stub[16] = 0x0F;
    stub[17] = 0x84;
    *(int *)(stub + 18) = (int)(onTarget - (stub + 22));

    stub[22] = 0x83;
    stub[23] = cmpModRM;
    stub[24] = 0x02;

    stub[25] = 0x0F;
    stub[26] = 0x84;
    *(int *)(stub + 27) = (int)(onTarget - (stub + 31));

    stub[31] = 0xE9;
    *(int *)(stub + 32) = (int)(offTarget - (stub + 36));

    FlushInstructionCache(GetCurrentProcess(), stub, 36);

    patch[0] = 0xE9;
    *(int *)(patch + 1) = (int)(stub - site - 5);
    for (i = 5; i < siteLen; i++)
        ((volatile BYTE *)patch)[i] = 0x90;
    return PatchBytes(site, patch, siteLen);
}

/* Disable JPOG's lens-flare distance culling, zero-area quad collapse, global
 * 2000-vertex truncation, and 15-building flare pool cap in simjp.exe so light
 * flares (and Remix effect lights attached to them) stay alive reliably, and
 * install dynamic night-flare weather-gate stubs for buildings and vehicles. */
static void PatchFlareCulling(HMODULE hSimJp) {
    static const BYTE sigBldgWeather1[12] = {
        0x83, 0xFB, 0x01, 0x74, 0x07, 0x83, 0xFB, 0x02, 0x74, 0x02, 0xEB, 0x30
    };
    static const BYTE sigBldgWeather2[10] = {
        0x83, 0xFB, 0x01, 0x74, 0x05, 0x83, 0xFB, 0x02, 0x75, 0x4E
    };
    static const BYTE sigUnitWeather[10] = {
        0x83, 0xF8, 0x01, 0x74, 0x37, 0x83, 0xF8, 0x02, 0x74, 0x32
    };
    static const BYTE sigFarRecip[4] = { 0xD9, 0x41, 0x1C, 0xC3 };
    static const BYTE patFarRecip[4] = { 0xD9, 0xEE, 0xC3, 0x90 };

    static const BYTE sigBaseFade[15] = {
        0xD8, 0x1D, 0x38, 0xE3, 0x8A, 0x00, 0xDF, 0xE0, 0x9E,
        0x77, 0x02, 0xEB, 0x0A, 0xDD, 0xD8
    };
    static const BYTE patBaseFade[4] = { 0xEB, 0x24, 0x90, 0x90 };

    static const BYTE sigGroupFade[12] = {
        0xDE, 0xD9, 0xDF, 0xE0, 0x9E, 0x73, 0x0E, 0xE9, 0xA9, 0x00, 0x00, 0x00
    };
    static const BYTE patGroupFade[2] = { 0xEB, 0x0E };

    static const BYTE sigBldgDist[9] = {
        0xDE, 0xD9, 0xDF, 0xE0, 0x9E, 0x77, 0x02, 0xEB, 0x8D
    };
    static const BYTE patBldgDist[2] = { 0xEB, 0x02 };

    static const BYTE sigMaxVerts[14] = {
        0x8B, 0x15, 0x34, 0x30, 0x84, 0x00, 0x3B, 0xFA,
        0x0F, 0x86, 0x4D, 0x01, 0x00, 0x00
    };
    static const BYTE patMaxVerts[6] = { 0xE9, 0x4E, 0x01, 0x00, 0x00, 0x90 };

    static const BYTE sigPool15[10] = {
        0xC7, 0x05, 0xC4, 0xE1, 0x8A, 0x00, 0x0F, 0x00, 0x00, 0x00
    };
    static const BYTE immPool256[4] = { 0x00, 0x01, 0x00, 0x00 };

    static const BYTE sigChunkInit[39] = {
        0xBA, 0x20, 0x00, 0x00, 0x00,
        0xA1, 0x58, 0xE3, 0x8A, 0x00,
        0x83, 0xF8, 0x20,
        0xC7, 0x05, 0x40, 0x31, 0x84, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x0F, 0x8C, 0x1F, 0x03, 0x00, 0x00,
        0xC7, 0x05, 0x54, 0xE3, 0x8A, 0x00, 0x20, 0x00, 0x00, 0x00
    };
    static const BYTE sigChunkRealloc[17] = {
        0xC7, 0x44, 0x24, 0x0C, 0x20, 0x00, 0x00, 0x00,
        0x6A, 0xFF, 0x6A, 0x00,
        0x68, 0x80, 0x00, 0x00, 0x00
    };
    static const BYTE patAlwaysRealloc[6] = { 0xE9, 0x20, 0x03, 0x00, 0x00, 0x90 };
    static const BYTE immChunk300[4] = { 0x2C, 0x01, 0x00, 0x00 };
    static const BYTE immChunkBytes1200[4] = { 0xB0, 0x04, 0x00, 0x00 };
    BYTE *base = (BYTE *)hSimJp;

    if (MatchModuleBytes(hSimJp, FLARE_FAR_RECIP_RVA, sigFarRecip, sizeof(sigFarRecip)))
        PatchBytes(base + FLARE_FAR_RECIP_RVA, patFarRecip, sizeof(patFarRecip));

    if (MatchModuleBytes(hSimJp, FLARE_BASE_FADE_RVA, sigBaseFade, sizeof(sigBaseFade)))
        PatchBytes(base + FLARE_BASE_FADE_RVA + 9, patBaseFade, sizeof(patBaseFade));

    if (MatchModuleBytes(hSimJp, FLARE_GROUP_FADE_RVA, sigGroupFade, sizeof(sigGroupFade)))
        PatchBytes(base + FLARE_GROUP_FADE_RVA + 5, patGroupFade, sizeof(patGroupFade));

    if (MatchModuleBytes(hSimJp, FLARE_BLDG_DIST_RVA, sigBldgDist, sizeof(sigBldgDist)))
        PatchBytes(base + FLARE_BLDG_DIST_RVA + 5, patBldgDist, sizeof(patBldgDist));

    if (MatchModuleBytes(hSimJp, PARTICLE_MAXVERTS_RVA, sigMaxVerts, sizeof(sigMaxVerts)))
        PatchBytes(base + PARTICLE_MAXVERTS_RVA + 8, patMaxVerts, sizeof(patMaxVerts));

    if (MatchModuleBytes(hSimJp, FLARE_CHUNK_INIT_RVA, sigChunkInit, sizeof(sigChunkInit)) &&
        MatchModuleBytes(hSimJp, FLARE_CHUNK_REALLOC_RVA, sigChunkRealloc, sizeof(sigChunkRealloc)) &&
        MatchModuleBytes(hSimJp, FLARE_POOL_LOW_RVA, sigPool15, sizeof(sigPool15)) &&
        MatchModuleBytes(hSimJp, FLARE_POOL_MED_RVA, sigPool15, sizeof(sigPool15)) &&
        MatchModuleBytes(hSimJp, FLARE_POOL_HIGH_RVA, sigPool15, sizeof(sigPool15))) {
        PatchBytes(base + FLARE_CHUNK_INIT_RVA + 1, immChunk300, 4);
        PatchBytes(base + FLARE_CHUNK_INIT_RVA + 19, immChunk300, 4);
        PatchBytes(base + FLARE_CHUNK_INIT_RVA + 23, patAlwaysRealloc, sizeof(patAlwaysRealloc));
        PatchBytes(base + FLARE_CHUNK_INIT_RVA + 35, immChunk300, 4);
        PatchBytes(base + FLARE_CHUNK_REALLOC_RVA + 4, immChunk300, 4);
        PatchBytes(base + FLARE_CHUNK_REALLOC_RVA + 13, immChunkBytes1200, 4);
        PatchBytes(base + FLARE_POOL_LOW_RVA + 6, immPool256, 4);
        PatchBytes(base + FLARE_POOL_MED_RVA + 6, immPool256, 4);
        PatchBytes(base + FLARE_POOL_HIGH_RVA + 6, immPool256, 4);
    }

    g_hSimJp = hSimJp;
    g_nightFlareStubPage = (BYTE *)VirtualAlloc(NULL, 256,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (g_nightFlareStubPage) {
        if (MatchModuleBytes(hSimJp, FLARE_BLDG_WEATHER1_RVA, sigBldgWeather1, sizeof(sigBldgWeather1))) {
            InstallNightFlareGateStub(base + FLARE_BLDG_WEATHER1_RVA, sizeof(sigBldgWeather1), 0xFB,
                base + FLARE_BLDG_WEATHER1_RVA + 12, base + FLARE_BLDG_WEATHER1_RVA + 0x3C,
                g_nightFlareStubPage + 0);
        }
        if (MatchModuleBytes(hSimJp, FLARE_BLDG_WEATHER2_RVA, sigBldgWeather2, sizeof(sigBldgWeather2))) {
            InstallNightFlareGateStub(base + FLARE_BLDG_WEATHER2_RVA, sizeof(sigBldgWeather2), 0xFB,
                base + FLARE_BLDG_WEATHER2_RVA + 10, base + FLARE_BLDG_WEATHER2_RVA + 0x58,
                g_nightFlareStubPage + 64);
        }
        if (MatchModuleBytes(hSimJp, FLARE_HELI_WEATHER_RVA, sigUnitWeather, sizeof(sigUnitWeather))) {
            InstallNightFlareGateStub(base + FLARE_HELI_WEATHER_RVA, sizeof(sigUnitWeather), 0xF8,
                base + FLARE_HELI_WEATHER_RVA + 0x3C, base + FLARE_HELI_WEATHER_RVA + 10,
                g_nightFlareStubPage + 128);
        }
        if (MatchModuleBytes(hSimJp, FLARE_VEH_WEATHER_RVA, sigUnitWeather, sizeof(sigUnitWeather))) {
            InstallNightFlareGateStub(base + FLARE_VEH_WEATHER_RVA, sizeof(sigUnitWeather), 0xF8,
                base + FLARE_VEH_WEATHER_RVA + 0x3C, base + FLARE_VEH_WEATHER_RVA + 10,
                g_nightFlareStubPage + 192);
        }
    }
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

    CopyBytes(g_weatherTrampoline, target, 5);
    g_weatherTrampoline[5] = 0xE9;
    rel = (int)((target + 5) - (g_weatherTrampoline + 10));
    CopyBytes(g_weatherTrampoline + 6, &rel, 4);
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

    CopyBytes(g_terrainTrampoline, target, 7);
    g_terrainTrampoline[7] = 0xE9;
    rel = (int)((target + 7) - (g_terrainTrampoline + 12));
    CopyBytes(g_terrainTrampoline + 8, &rel, 4);
    FlushInstructionCache(GetCurrentProcess(), g_terrainTrampoline, 12);
    g_originalTerrainRender = (FnTerrainRender)g_terrainTrampoline;

    patch[0] = 0xE9;
    rel = (int)((BYTE*)Hook_TerrainRender - target - 5);
    CopyBytes(patch + 1, &rel, 4);
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

/* Expand TTerrainInterface::Create's 5 terrain cache pool caps (400/400/400/350/900 -> 4096)
 * so extended TerrainDrawDistance values do not exhaust terrain tile caches. */
static int PatchTerrainCacheLimits(HMODULE hMod) {
    static const BYTE movEdx400[5] = { 0xBA, 0x90, 0x01, 0x00, 0x00 };
    static const BYTE movEax400[5] = { 0xB8, 0x90, 0x01, 0x00, 0x00 };
    static const BYTE movEdi400[5] = { 0xBF, 0x90, 0x01, 0x00, 0x00 };
    static const BYTE storeOp[2]   = { 0xC7, 0x05 };
    static const BYTE imm350[4]    = { 0x5E, 0x01, 0x00, 0x00 };
    static const BYTE imm900[4]    = { 0x84, 0x03, 0x00, 0x00 };
    static const BYTE imm4096[4]   = { 0x00, 0x10, 0x00, 0x00 };
    BYTE *base = (BYTE *)hMod;

    if (!MatchModuleBytes(hMod, TERRAIN_CACHE_MOV1_RVA, movEdx400, 5) ||
        !MatchModuleBytes(hMod, TERRAIN_CACHE_MOV2_RVA, movEax400, 5) ||
        !MatchModuleBytes(hMod, TERRAIN_CACHE_MOV3_RVA, movEdi400, 5) ||
        !MatchModuleBytes(hMod, TERRAIN_CACHE_STORE1_RVA, storeOp, 2) ||
        !MatchModuleBytes(hMod, TERRAIN_CACHE_STORE1_RVA + 6, imm350, 4) ||
        !MatchModuleBytes(hMod, TERRAIN_CACHE_STORE2_RVA, storeOp, 2) ||
        !MatchModuleBytes(hMod, TERRAIN_CACHE_STORE2_RVA + 6, imm900, 4)) {
        return 0;
    }

    PatchBytes(base + TERRAIN_CACHE_MOV1_RVA + 1, imm4096, 4);
    PatchBytes(base + TERRAIN_CACHE_MOV2_RVA + 1, imm4096, 4);
    PatchBytes(base + TERRAIN_CACHE_MOV3_RVA + 1, imm4096, 4);
    PatchBytes(base + TERRAIN_CACHE_STORE1_RVA + 6, imm4096, 4);
    PatchBytes(base + TERRAIN_CACHE_STORE2_RVA + 6, imm4096, 4);
    return 1;
}

static int InstallTerrainLodHook(HMODULE hMod) {
    static const BYTE expectedPrologue[6] = { 0x56, 0x55, 0x53, 0x83, 0xEC, 0x30 };
    BYTE patch[6];
    BYTE *target;
    int rel;

    PatchTerrainCacheLimits(hMod);

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
    CopyBytes(patch + 1, &rel, 4);
    patch[5] = 0x90;
    return PatchBytes(target, patch, 6);
}

static int InstallTrampolineN(BYTE *target, int stolenLen, void *hook,
    BYTE **outTrampoline, void **outOriginal)
{
    BYTE patch[16];
    BYTE *trampoline;
    int rel;
    int i;

    if (stolenLen < 5 || stolenLen > (int)sizeof(patch))
        return 0;

    trampoline = (BYTE *)VirtualAlloc(NULL, (SIZE_T)(stolenLen + 5),
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline)
        return 0;

    CopyBytes(trampoline, target, (unsigned int)stolenLen);
    trampoline[stolenLen] = 0xE9;
    rel = (int)((target + stolenLen) - (trampoline + stolenLen + 5));
    CopyBytes(trampoline + stolenLen + 1, &rel, 4);
    FlushInstructionCache(GetCurrentProcess(), trampoline, (SIZE_T)(stolenLen + 5));

    /* Publish the trampoline before redirecting target so the render thread
     * can never invoke the hook while the original pointer is still NULL. */
    *outTrampoline = trampoline;
    *outOriginal = (void *)trampoline;
    MemoryBarrier();

    patch[0] = 0xE9;
    rel = (int)((BYTE *)hook - target - 5);
    CopyBytes(patch + 1, &rel, 4);
    for (i = 5; i < stolenLen; i++)
        ((volatile BYTE *)patch)[i] = 0x90;
    if (!PatchBytes(target, patch, stolenLen)) {
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
    if (!g_viewSyncedThisFrame) {
        g_viewSyncedThisFrame = (volatile int *)GetProcAddress(
            hProxy, "g_viewSyncedThisFrame");
    }
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

    if (!InstallTrampolineN((BYTE *)hMod + SYSSHADER_RENDER_RVA, 10,
            (void *)Hook_SysShaderRender, &g_sysShaderRenderTrampoline,
            (void **)&g_originalSysShaderRender))
        return 1;

    if (!InstallTrampolineN((BYTE *)hMod + SYSSHADER_FLUSH_RVA, 10,
            (void *)Hook_SysShaderFlush, &g_sysShaderFlushTrampoline,
            (void **)&g_originalSysShaderFlush))
        return 1;

    PatchBytes((BYTE *)hMod + SYSSHADER_CAP_HIGHEND_RVA, retZero, 3);
    PatchBytes((BYTE *)hMod + SYSSHADER_CAP_SCATTER_RVA, retZero, 3);
    return 1;
}

/* Switch TSkinShaderD3D (trees, buildings, fences, dinosaurs, visitors, vehicles)
 * to its built-in FVF 0x1D2 fixed-function path. 1-bone rigid meshes keep the
 * native static-VB path (zero CPU skinning; proxy strips bogus DIFFUSE/SPECULAR
 * via a POS/NORM/UV declaration) while multi-bone meshes use CPU skinning into
 * the expanded 2.5 MiB dynamic VB. */
static int InstallSkinShaderFfpHooks(HMODULE hMod) {
    static const BYTE flushPrologue[7] = {
        0x57, 0x56, 0x55, 0x53, 0x83, 0xEC, 0x6C
    };
    static const BYTE renderPrologue[9] = {
        0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x78, 0x05, 0x00, 0x00
    };
    static const BYTE vbCreateBytes[19] = {
        0x68, 0x12, 0x01, 0x00, 0x00,
        0x68, 0x08, 0x02, 0x00, 0x00,
        0x68, 0x00, 0x77, 0x01, 0x00,
        0x52, 0xFF, 0x51, 0x5C
    };
    static const BYTE capHighEndPrologue[6] = {
        0x81, 0xEC, 0xD4, 0x00, 0x00, 0x00
    };
    static const BYTE capScatterPrologue[7] = {
        0x57, 0x81, 0xEC, 0xD8, 0x00, 0x00, 0x00
    };
    static const BYTE retZero[3] = { 0x33, 0xC0, 0xC3 };
    static const BYTE maxSkinVbSize[4] = { 0x00, 0x00, 0x28, 0x00 }; /* 65536 * 40 bytes */
    HMODULE hProxy = GetModuleHandleA("d3d9.dll");

    if (!hProxy)
        return 0;
    if (!g_mvWorldTransformActive) {
        g_mvWorldTransformActive = (volatile int *)GetProcAddress(
            hProxy, "g_mvWorldTransformActive");
    }
    if (!g_viewSyncedThisFrame) {
        g_viewSyncedThisFrame = (volatile int *)GetProcAddress(
            hProxy, "g_viewSyncedThisFrame");
    }
    if (!g_mvWorldTransformActive) {
        OutputDebugStringA("JPOG ASI: proxy lacks g_mvWorldTransformActive; TSkinShader FFP skipped.\n");
        return 1;
    }
    if (!MatchModuleBytes(hMod, SKINSHADER_FLUSH_RVA, flushPrologue, 7) ||
        !MatchModuleBytes(hMod, SKINSHADER_RENDER_RVA, renderPrologue, 9) ||
        !MatchModuleBytes(hMod, SKINSHADER_VB_CREATE_RVA, vbCreateBytes, 19) ||
        !MatchModuleBytes(hMod, SKINSHADER_CAP_HIGHEND_RVA, capHighEndPrologue, 6) ||
        !MatchModuleBytes(hMod, SKINSHADER_CAP_SCATTER_RVA, capScatterPrologue, 7)) {
        OutputDebugStringA("JPOG ASI: unsupported TSkinShaderD3D signatures; FFP switch skipped.\n");
        return 1;
    }

    if (!InstallTrampolineN((BYTE *)hMod + SKINSHADER_RENDER_RVA, 9,
            (void *)Hook_SkinShaderRender, &g_skinShaderRenderTrampoline,
            (void **)&g_originalSkinShaderRender))
        return 1;

    if (!InstallTrampolineN((BYTE *)hMod + SKINSHADER_FLUSH_RVA, 7,
            (void *)Hook_SkinShaderFlush, &g_skinShaderFlushTrampoline,
            (void **)&g_originalSkinShaderFlush))
        return 1;

    /* Expand the dynamic CPU-skinning VB from 96,000 B (2,400 verts) to 2.5 MiB (65,536 verts)
     * so >2,400-vertex multi-bone meshes cannot overflow d3d9_remix's shadow buffer on Lock/skinning. */
    PatchBytes((BYTE *)hMod + SKINSHADER_VB_SIZE_RVA, maxSkinVbSize, 4);
    PatchBytes((BYTE *)hMod + SKINSHADER_CAP_HIGHEND_RVA, retZero, 3);
    PatchBytes((BYTE *)hMod + SKINSHADER_CAP_SCATTER_RVA, retZero, 3);
    return 1;
}

static int InstallSpriteShaderFfpHooks(HMODULE hMod) {
    static const BYTE flushPrologue[8] = {
        0x56, 0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x18
    };
    static const BYTE renderPrologue[10] = {
        0x57, 0x56, 0x55, 0x53, 0x81, 0xEC, 0xCC, 0x00, 0x00, 0x00
    };
    HMODULE hProxy = GetModuleHandleA("d3d9.dll");

    if (!hProxy)
        return 0;
    if (!g_mvWorldTransformActive) {
        g_mvWorldTransformActive = (volatile int *)GetProcAddress(
            hProxy, "g_mvWorldTransformActive");
    }
    if (!g_viewSyncedThisFrame) {
        g_viewSyncedThisFrame = (volatile int *)GetProcAddress(
            hProxy, "g_viewSyncedThisFrame");
    }
    if (!g_mvWorldTransformActive) {
        OutputDebugStringA("JPOG ASI: proxy lacks g_mvWorldTransformActive; TSpriteShader FFP skipped.\n");
        return 1;
    }
    if (!MatchModuleBytes(hMod, SPRITESHADER_FLUSH_RVA, flushPrologue, sizeof(flushPrologue)) ||
        !MatchModuleBytes(hMod, SPRITESHADER_RENDER_RVA, renderPrologue, sizeof(renderPrologue))) {
        OutputDebugStringA("JPOG ASI: unsupported TSpriteShaderD3D signatures; FFP switch skipped.\n");
        return 1;
    }

    if (!InstallTrampolineN((BYTE *)hMod + SPRITESHADER_RENDER_RVA, sizeof(renderPrologue),
            (void *)Hook_SpriteShaderRender, &g_spriteShaderRenderTrampoline,
            (void **)&g_originalSpriteShaderRender))
        return 1;

    if (!InstallTrampolineN((BYTE *)hMod + SPRITESHADER_FLUSH_RVA, sizeof(flushPrologue),
            (void *)Hook_SpriteShaderFlush, &g_spriteShaderFlushTrampoline,
            (void **)&g_originalSpriteShaderFlush))
        return 1;

    return 1;
}

static int InstallGrassSingleDrawHook(HMODULE hMod) {
    static const BYTE flushPrologue[10] = {
        0x57, 0x56, 0x55, 0x53, 0x81, 0xEC, 0xCC, 0x00, 0x00, 0x00
    };
    static const BYTE renderPrologue[10] = {
        0x57, 0x56, 0x55, 0x53, 0x81, 0xEC, 0xBC, 0x00, 0x00, 0x00
    };
    HMODULE hProxy = GetModuleHandleA("d3d9.dll");

    if (!hProxy)
        return 0;
    g_grassFlushState = (volatile int *)GetProcAddress(hProxy, "g_grassFlushState");
    if (!g_grassFlushState) {
        OutputDebugStringA("JPOG ASI: proxy lacks g_grassFlushState; legacy grass skip skipped.\n");
        return 1;
    }
    if (!MatchModuleBytes(hMod, GRASS_FLUSH_RVA, flushPrologue, 10) ||
        !MatchModuleBytes(hMod, GRASS_RENDER_RVA, renderPrologue, 10)) {
        OutputDebugStringA("JPOG ASI: unsupported TGrassShaderHAL signatures; legacy grass skip skipped.\n");
        return 1;
    }

    if (!InstallTrampolineN((BYTE *)hMod + GRASS_RENDER_RVA, 10,
            (void *)Hook_GrassRender, &g_grassRenderTrampoline,
            (void **)&g_originalGrassRender))
        return 1;

    if (!InstallTrampolineN((BYTE *)hMod + GRASS_FLUSH_RVA, 10,
            (void *)Hook_GrassFlush, &g_grassFlushTrampoline,
            (void **)&g_originalGrassFlush))
        return 1;

    return 1;
}

/* Lock low-end (reflection-off) water in TTerrainShaderHAL::RenderWater to
 * flipbook frame 2 (t_water0002_1555.dds, Remix hash 0xEFEC3F35D7660711) so
 * the translucent Remix water override stays bound on every frame while
 * rtx.animatedWaterTextures drives wave animation. */
static int PatchLowEndWaterFreeze(HMODULE hMod) {
    BYTE expectedLoad[13] = {
        0x8B, 0x15, 0, 0, 0, 0,
        0x8B, 0x94, 0x96, 0x10, 0x02, 0x00, 0x00
    };
    static const BYTE movEdxFrame2[6] = { 0xBA, 0x02, 0x00, 0x00, 0x00, 0x90 };
    DWORD frameIdxAddr = (DWORD)((BYTE *)hMod + TERRAIN_WATER_FRAME_IDX_RVA);

    CopyBytes(expectedLoad + 2, &frameIdxAddr, 4);
    if (!MatchModuleBytes(hMod, TERRAIN_WATER_FRAME_LOAD_RVA, expectedLoad, sizeof(expectedLoad))) {
        OutputDebugStringA("JPOG ASI: unsupported TTerrainShaderD3D water frame load signature; water freeze skipped.\n");
        return 1;
    }
    PatchBytes((BYTE *)hMod + TERRAIN_WATER_FRAME_LOAD_RVA, movEdxFrame2, sizeof(movEdxFrame2));
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

    CopyBytes(destroyPrologue + 2, &contextVtable, 4);
    if (!MatchModuleBytes(hMod, 0x7180, matrixPrologue, 6) ||
        !MatchModuleBytes(hMod, 0x71C0, destroyPrologue, 6)) {
        OutputDebugStringA("JPOG ASI: unsupported render-context signatures; camera hooks skipped.\n");
        return 0;
    }
    g_contextDestroyTrampoline = (BYTE*)VirtualAlloc(NULL, 11,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_contextDestroyTrampoline)
        return 0;
    CopyBytes(g_contextDestroyTrampoline, destroyPrologue, 6);
    g_contextDestroyTrampoline[6] = 0xE9;
    relative = (int)(destroyTarget + 6 - (g_contextDestroyTrampoline + 11));
    CopyBytes(g_contextDestroyTrampoline + 7, &relative, 4);
    FlushInstructionCache(GetCurrentProcess(), g_contextDestroyTrampoline, 11);
    g_originalDestroyRenderContext = (FnDestroyRenderContext)g_contextDestroyTrampoline;

    destroyPatch[0] = 0xE9;
    relative = (int)((BYTE*)Hook_DestroyRenderContext - destroyTarget - 5);
    CopyBytes(destroyPatch + 1, &relative, 4);
    destroyPatch[5] = 0x90;
    matrixPatch[0] = 0xE9;
    relative = (int)((BYTE*)Hook_SetRenderMatrices - ((BYTE*)hMod + 0x7180) - 5);
    CopyBytes(matrixPatch + 1, &relative, 4);
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
    HMODULE hSkinShader = NULL;
    HMODULE hSpriteShader = NULL;
    HMODULE hRenderIface = NULL;
    int renderAttempted = 0, terrainDone = 0, waterResetDone = 0, grassSkipDone = 0, waterFreezeDone = 0;
    int terrainLodAttempted = 0, sysShaderAttempted = 0, skinShaderAttempted = 0, spriteShaderAttempted = 0;
    int i;

    (void)param;
    hSimJp = GetModuleHandleA(NULL);
    if (hSimJp)
        InstallWeatherHook(hSimJp);

    for (i = 0; i < 300; i++) {  /* 30 seconds max */
        if (!g_getCurrentRenderContext) {
            hRenderIface = GetModuleHandleA("TRenderInterface.dll");
            if (hRenderIface) {
                g_getCurrentRenderContext = (FnGetCurrentRenderContext)GetProcAddress(
                    hRenderIface,
                    "?GetCurrentRenderContext@TRenderInterface@Toshi@@QAEPAVTRenderContext@2@XZ");
            }
        }
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
        if (!skinShaderAttempted) {
            hSkinShader = GetModuleHandleA("TSkinShaderD3D.dll");
            if (hSkinShader)
                skinShaderAttempted = InstallSkinShaderFfpHooks(hSkinShader);
        }
        if (!spriteShaderAttempted) {
            hSpriteShader = GetModuleHandleA("TSpriteShaderD3D.dll");
            if (hSpriteShader)
                spriteShaderAttempted = InstallSpriteShaderFfpHooks(hSpriteShader);
        }
        if (!terrainDone || !waterResetDone || !grassSkipDone || !waterFreezeDone) {
            hTerrainShader = GetModuleHandleA("TTerrainShaderD3D.dll");
            if (hTerrainShader) {
                if (!terrainDone)
                    terrainDone = InstallTerrainTagHook(hTerrainShader);
                if (!waterResetDone)
                    waterResetDone = InstallTerrainWaterResetHook(hTerrainShader);
                if (!grassSkipDone)
                    grassSkipDone = InstallGrassSingleDrawHook(hTerrainShader);
                if (!waterFreezeDone)
                    waterFreezeDone = PatchLowEndWaterFreeze(hTerrainShader);
            }
        }
        if (renderAttempted && terrainLodAttempted && sysShaderAttempted &&
            skinShaderAttempted && spriteShaderAttempted && terrainDone && waterResetDone &&
            grassSkipDone && waterFreezeDone)
            return 0;
        Sleep(100);
    }
    return 0;
}

/* ---- DLL entry point ---- */

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID reserved) {
    HANDLE thread;
    HMODULE pinnedModule;
    HMODULE hTerrainIface;
    HMODULE hSimJp;

    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        /* Game modules keep jumps into this ASI for the process lifetime. */
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCSTR)hDll, &pinnedModule))
            return FALSE;
        DisableThreadLibraryCalls(hDll);
        /* Patch statically imported modules synchronously before Create() runs. */
        hSimJp = GetModuleHandleA(NULL);
        if (hSimJp)
            PatchFlareCulling(hSimJp);
        hTerrainIface = GetModuleHandleA("TTerrainInterface.dll");
        if (hTerrainIface)
            PatchTerrainCacheLimits(hTerrainIface);
        thread = CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}

int _fltused = 0;
