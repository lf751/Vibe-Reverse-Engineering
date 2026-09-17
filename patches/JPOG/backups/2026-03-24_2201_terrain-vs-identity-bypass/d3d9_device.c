/*
 * JPOG Matrix-Capture Proxy for RTX Remix
 *
 * Supplements the Toshi engine's shader-based rendering with SetTransform
 * calls so RTX Remix can read geometry positions. Shaders stay active;
 * no FFP conversion is performed.
 *
 * VS constant layout (from RE of TSysShaderD3D.dll):
 *   c0-c3   = transpose(ModelView x Projection)  (per object)
 *   c17-c20 = transpose(ModelView)                (per object)
 *
 * Strategy:
 *   1. Capture c0-c3 writes  -> transpose to get MVP (D3D row-major)
 *   2. Capture c17-c20 writes -> transpose to get MV
 *   3. Real View from ASI hook (or rotation-only from first MV)
 *   4. Every draw: compute WORLD from MV or MVP, call SetTransform
 *   5. Forward draw unchanged (shaders stay set)
 *   6. Remix reads WORLD/VIEW from SetTransform for scene placement
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

/* No-CRT memcpy */
#pragma function(memcpy)
void * __cdecl memcpy(void *dst, const void *src, unsigned int n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

/* Logging (from d3d9_main.c) */

/* Runtime A/B toggles from d3d9_main.c */
extern int g_cfgWorldOverrideOnDP;
extern int g_cfgWorldOverrideOnDIP;
extern int g_cfgForceIdentityView;
extern int g_cfgRejectIdentityView;
extern int g_cfgPreserveWorldOnMvStall;
extern int g_cfgTerrainQ2LatchGuard;

#ifdef _MSC_VER
#pragma intrinsic(_ReturnAddress)
#endif

#define RVA_DXWRAP_DIP_CALLER     0x0004E75D
#define RVA_TERRAIN_SVCF_A        0x0001230B
#define RVA_TERRAIN_SVCF_B        0x000123EB
#define TERRAIN_Q2_BYPASS_DRAWS   256

static unsigned int g_dxwrapperBase = 0;
static unsigned int g_terrainShaderBase = 0;

/* ============================================================
 * JPOG VS Constant Register Layout
 *
 * c0-c3:   transpose(ModelView x Projection) — combined MVP
 * c17-c20: transpose(ModelView) — for VS lighting/normals
 * ============================================================ */
#define VS_REG_MV_START  17
#define VS_REG_MV_END    21

/* ---- D3D9 Constants ---- */

#define D3DTS_VIEW       2
#define D3DTS_PROJECTION 3
#define D3DTS_WORLD      256

/* ---- Device vtable slot indices ---- */
enum {
    SLOT_QueryInterface = 0,
    SLOT_AddRef = 1,
    SLOT_Release = 2,
    SLOT_CreateVertexBuffer = 26,
    SLOT_Reset = 16,
    SLOT_Present = 17,
    SLOT_BeginScene = 41,
    SLOT_EndScene = 42,
    SLOT_SetTransform = 44,
    SLOT_DrawPrimitive = 81,
    SLOT_DrawIndexedPrimitive = 82,
    SLOT_SetVertexShaderConstantF = 94,
    SLOT_SetStreamSource = 100,
    DEVICE_VTABLE_SIZE = 119
};

/* ---- WrappedDevice ---- */

typedef struct WrappedDevice {
    void **vtbl;
    void *pReal;
    int refCount;
    unsigned int frameCount;

    /* MV from c17-c20 (transposed to D3D row-major) */
    float mvMatrix[16];
    int   mvDirty;

    /* MVP from c0-c3 (transposed to D3D row-major) */
    float mvpMatrix[16];
    int   hasMVP;

    /* View matrix (real from ASI or rotation-only approximation) */
    float viewMatrix[16];
    float invViewMatrix[16];
    int   viewCapturedThisFrame;
    int   hasRealView;

    /* Projection from SetTransform(D3DTS_PROJECTION) */
    float projMatrix[16];

    /* Cached inv(Projection) for MVP decomposition */
    float invProjMatrix[16];
    int   hasProjection;
    int   invProjDirty;

    /* Cached per-draw World */
    float cachedWorld[16];
    int   hasCachedWorld;

    /* Set when game calls SetTransform(WORLD); cleared on new MVP write */
    int   gameWorldSet;
    int   terrainQ2BypassBudget;

    unsigned int drawCallCount;
    unsigned int sceneCount;
} WrappedDevice;

static __inline void** RealVtbl(WrappedDevice *self) {
    return *(void***)(self->pReal);
}

static __inline unsigned int caller_u32(void) {
    return (unsigned int)_ReturnAddress();
}

static void ensure_module_bases(void) {
    if (!g_dxwrapperBase) {
        HMODULE mod = GetModuleHandleA("dxwrapper.dll");
        if (mod) g_dxwrapperBase = (unsigned int)mod;
    }
    if (!g_terrainShaderBase) {
        HMODULE mod = GetModuleHandleA("TTerrainShaderD3D.dll");
        if (mod) g_terrainShaderBase = (unsigned int)mod;
    }
}

static int caller_matches_rva(unsigned int caller, unsigned int base, unsigned int rva) {
    if (!base || caller < base) return 0;
    return (caller - base) == rva;
}

/* ---- Matrix helpers ---- */

static void mat4_transpose(float *dst, const float *src) {
    dst[0]  = src[0];  dst[1]  = src[4];  dst[2]  = src[8];  dst[3]  = src[12];
    dst[4]  = src[1];  dst[5]  = src[5];  dst[6]  = src[9];  dst[7]  = src[13];
    dst[8]  = src[2];  dst[9]  = src[6];  dst[10] = src[10]; dst[11] = src[14];
    dst[12] = src[3];  dst[13] = src[7];  dst[14] = src[11]; dst[15] = src[15];
}

static void mat4_invOrthogonal(float *dst, const float *src) {
    float tx = src[12], ty = src[13], tz = src[14];
    dst[0]  = src[0]; dst[1]  = src[4]; dst[2]  = src[8];  dst[3]  = 0.0f;
    dst[4]  = src[1]; dst[5]  = src[5]; dst[6]  = src[9];  dst[7]  = 0.0f;
    dst[8]  = src[2]; dst[9]  = src[6]; dst[10] = src[10]; dst[11] = 0.0f;
    dst[12] = -(tx*src[0] + ty*src[1] + tz*src[2]);
    dst[13] = -(tx*src[4] + ty*src[5] + tz*src[6]);
    dst[14] = -(tx*src[8] + ty*src[9] + tz*src[10]);
    dst[15] = 1.0f;
}

static void mat4_multiply(float *dst, const float *a, const float *b) {
    float tmp[16];
    int i, j, k;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            float sum = 0.0f;
            for (k = 0; k < 4; k++)
                sum += a[i*4+k] * b[k*4+j];
            tmp[i*4+j] = sum;
        }
    }
    memcpy(dst, tmp, 16 * sizeof(float));
}

/* General 4x4 matrix inverse (cofactor method). Returns 0 if singular. */
static int mat4_invert(float *dst, const float *m) {
    float s0 = m[0]*m[5]  - m[1]*m[4];
    float s1 = m[0]*m[6]  - m[2]*m[4];
    float s2 = m[0]*m[7]  - m[3]*m[4];
    float s3 = m[1]*m[6]  - m[2]*m[5];
    float s4 = m[1]*m[7]  - m[3]*m[5];
    float s5 = m[2]*m[7]  - m[3]*m[6];

    float c5 = m[10]*m[15] - m[11]*m[14];
    float c4 = m[9]*m[15]  - m[11]*m[13];
    float c3 = m[9]*m[14]  - m[10]*m[13];
    float c2 = m[8]*m[15]  - m[11]*m[12];
    float c1 = m[8]*m[14]  - m[10]*m[12];
    float c0 = m[8]*m[13]  - m[9]*m[12];

    float det = s0*c5 - s1*c4 + s2*c3 + s3*c2 - s4*c1 + s5*c0;
    float invDet;

    if (det > -1e-6f && det < 1e-6f)
        return 0;

    invDet = 1.0f / det;

    dst[0]  = ( m[5]*c5  - m[6]*c4  + m[7]*c3)  * invDet;
    dst[1]  = (-m[1]*c5  + m[2]*c4  - m[3]*c3)  * invDet;
    dst[2]  = ( m[13]*s5 - m[14]*s4 + m[15]*s3) * invDet;
    dst[3]  = (-m[9]*s5  + m[10]*s4 - m[11]*s3) * invDet;

    dst[4]  = (-m[4]*c5  + m[6]*c2  - m[7]*c1)  * invDet;
    dst[5]  = ( m[0]*c5  - m[2]*c2  + m[3]*c1)  * invDet;
    dst[6]  = (-m[12]*s5 + m[14]*s2 - m[15]*s1) * invDet;
    dst[7]  = ( m[8]*s5  - m[10]*s2 + m[11]*s1) * invDet;

    dst[8]  = ( m[4]*c4  - m[5]*c2  + m[7]*c0)  * invDet;
    dst[9]  = (-m[0]*c4  + m[1]*c2  - m[3]*c0)  * invDet;
    dst[10] = ( m[12]*s4 - m[13]*s2 + m[15]*s0) * invDet;
    dst[11] = (-m[8]*s4  + m[9]*s2  - m[11]*s0) * invDet;

    dst[12] = (-m[4]*c3  + m[5]*c1  - m[6]*c0)  * invDet;
    dst[13] = ( m[0]*c3  - m[1]*c1  + m[2]*c0)  * invDet;
    dst[14] = (-m[12]*s3 + m[13]*s1 - m[14]*s0) * invDet;
    dst[15] = ( m[8]*s3  - m[9]*s1  + m[10]*s0) * invDet;

    return 1;
}

static const float s_identity[16] = {
    1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
};

#define CENTROID_MIN_DIST2 2500.0f

static int mat4_isRigid(const float *m) {
    float d3 = m[3]*m[3] + m[7]*m[7] + m[11]*m[11];
    float d15 = (m[15] - 1.0f) * (m[15] - 1.0f);
    return (d3 < 0.001f && d15 < 0.001f);
}

static int mat4_isIdentity(const float *m) {
    int i;
    for (i = 0; i < 16; i++) {
        float expected = (i % 5 == 0) ? 1.0f : 0.0f;
        if (m[i] != expected) return 0;
    }
    return 1;
}

/* ---- VB vtable hook: capture first-vertex XYZ at upload time ---- */

#define VB_SAMPLE_MAX 4096
#define VB_LOCK_SLOTS   64

typedef struct { void *pVB; void *pData; } VBLockSlot;
typedef struct { void *pVB; float x, y, z; } VBSample;

static VBLockSlot  g_vbLocks[VB_LOCK_SLOTS];
static VBSample    g_vbSamples[VB_SAMPLE_MAX];
static int         g_vbSampleCount  = 0;
static int         g_vbVtblHooked   = 0;
static void       *g_origVBLock     = NULL;
static void       *g_origVBUnlock   = NULL;
static void       *g_curStream0VB   = NULL;

/* Per-frame counters */
static int         g_centroidOverrides = 0;
static int         g_centroidMisses    = 0;
static int         g_centroidNoVB      = 0;
static int         g_centroidNearOrigin= 0;
static int         g_gameWorldSets     = 0;
static int         g_svcfMvWrites      = 0;
static int         g_svcfMvpWrites     = 0;
static int         g_mvpWorldExtracts  = 0;

static float *vbsample_get(void *pVB) {
    int i;
    for (i = 0; i < g_vbSampleCount; i++)
        if (g_vbSamples[i].pVB == pVB) return &g_vbSamples[i].x;
    return NULL;
}

static void vbsample_store(void *pVB, float x, float y, float z) {
    int i;
    for (i = 0; i < g_vbSampleCount; i++) {
        if (g_vbSamples[i].pVB == pVB) {
            g_vbSamples[i].x = x; g_vbSamples[i].y = y; g_vbSamples[i].z = z;
            return;
        }
    }
    if (g_vbSampleCount >= VB_SAMPLE_MAX) return;
    g_vbSamples[g_vbSampleCount].pVB = pVB;
    g_vbSamples[g_vbSampleCount].x   = x;
    g_vbSamples[g_vbSampleCount].y   = y;
    g_vbSamples[g_vbSampleCount].z   = z;
    g_vbSampleCount++;
}

#define D3DLOCK_READONLY 0x0010

static int __stdcall VBLock_Hook(void *pVB,
    unsigned int offset, unsigned int size, void **ppData, unsigned int flags)
{
    typedef int (__stdcall *FN)(void*,unsigned int,unsigned int,void**,unsigned int);
    int hr = ((FN)g_origVBLock)(pVB, offset, size, ppData, flags);
    if (hr == 0 && ppData && offset == 0) {
        int i;
        for (i = 0; i < VB_LOCK_SLOTS; i++) {
            if (!g_vbLocks[i].pVB) {
                g_vbLocks[i].pVB  = pVB;
                g_vbLocks[i].pData = *ppData;
                break;
            }
        }
    }
    return hr;
}

static int __stdcall VBUnlock_Hook(void *pVB) {
    typedef int (__stdcall *FN)(void*);
    int i;
    for (i = 0; i < VB_LOCK_SLOTS; i++) {
        if (g_vbLocks[i].pVB == pVB) {
            float *f = (float *)g_vbLocks[i].pData;
            if (f) vbsample_store(pVB, f[0], f[1], f[2]);
            g_vbLocks[i].pVB  = NULL;
            g_vbLocks[i].pData = NULL;
            break;
        }
    }
    return ((FN)g_origVBUnlock)(pVB);
}

static void vb_hook_vtable(void *pVB) {
    void **vtbl;
    DWORD oldProt;
    if (g_vbVtblHooked) return;
    g_vbVtblHooked = 1;
    vtbl = *(void***)pVB;
    g_origVBLock   = vtbl[11];
    g_origVBUnlock = vtbl[12];
    VirtualProtect(&vtbl[11], sizeof(void*) * 2, PAGE_READWRITE, &oldProt);
    vtbl[11] = (void*)VBLock_Hook;
    vtbl[12] = (void*)VBUnlock_Hook;
    VirtualProtect(&vtbl[11], sizeof(void*) * 2, oldProt, &oldProt);
}

/* ---- Transform helpers ---- */

/* Lazily compute inv(Projection). Returns 1 on success, 0 on failure. */
static int ensureInvProj(WrappedDevice *self) {
    if (!self->hasProjection) return 0;
    if (!self->invProjDirty) return 1;

    if (!mat4_invert(self->invProjMatrix, self->projMatrix)) return 0;
    self->invProjDirty = 0;
    return 1;
}

/*
 * MV path: compute World = MV x inv(View) using the real camera View.
 * Runs unconditionally for every draw.
 */
static void applyMvPath(WrappedDevice *self) {
    typedef int (__stdcall *FN_SetTransform)(void*, unsigned int, float*);
    void **vt = RealVtbl(self);
    float world[16];

    if (!self->mvDirty) {
        if (!g_cfgPreserveWorldOnMvStall) {
            ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal,
                D3DTS_WORLD, (float*)s_identity);
        }
        return;
    }
    self->mvDirty = 0;

    if (!mat4_isRigid(self->mvMatrix)) {
        self->hasCachedWorld = 0;
        return;
    }

    if (!self->hasRealView) {
        self->hasCachedWorld = 0;
        return;
    }

    mat4_multiply(world, self->mvMatrix, self->invViewMatrix);
    world[3] = 0.0f; world[7] = 0.0f; world[11] = 0.0f; world[15] = 1.0f;
    memcpy(self->cachedWorld, world, 16 * sizeof(float));
    self->hasCachedWorld = 1;
    ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, world);
}

/*
 * Centroid / MVP path: override WORLD for objects without game-set transforms.
 * Skips when game already called SetTransform(WORLD) for this object.
 */
static void applyCentroidOrMvpPath(WrappedDevice *self) {
    typedef int (__stdcall *FN_SetTransform)(void*, unsigned int, float*);
    void **vt = RealVtbl(self);
    float world[16];
    float *centroid;

    if (self->gameWorldSet) return;

    /* Centroid lookup from VB sample table */
    if (g_curStream0VB) {
        centroid = vbsample_get(g_curStream0VB);
        if (centroid) {
            float dist2 = centroid[0]*centroid[0] +
                           centroid[1]*centroid[1] +
                           centroid[2]*centroid[2];
            if (dist2 > CENTROID_MIN_DIST2) {
                memcpy(world, s_identity, sizeof(world));
                world[12] = centroid[0];
                world[13] = centroid[1];
                world[14] = centroid[2];
                ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal,
                    D3DTS_WORLD, world);
                memcpy(self->cachedWorld, world, 16 * sizeof(float));
                self->hasCachedWorld = 1;
                g_centroidOverrides++;
                return;
            }
            g_centroidNearOrigin++;
        } else {
            g_centroidMisses++;
        }
    } else {
        g_centroidNoVB++;
    }

    /* MVP decomposition: World = MVP × inv(P) × inv(V)
     * Split into two steps to avoid precision loss from inverting View×Proj
     * as a single matrix (large camera translations make V×P ill-conditioned). */
    if (self->hasMVP && self->hasRealView && ensureInvProj(self)) {
        float mv[16];
        mat4_multiply(mv, self->mvpMatrix, self->invProjMatrix);
        mat4_multiply(world, mv, self->invViewMatrix);
        world[3] = 0.0f; world[7] = 0.0f; world[11] = 0.0f; world[15] = 1.0f;
        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal,
            D3DTS_WORLD, world);
        memcpy(self->cachedWorld, world, 16 * sizeof(float));
        self->hasCachedWorld = 1;
        g_mvpWorldExtracts++;
    }
}

/* ---- Vtable method implementations ---- */

static void *s_device_vtbl[DEVICE_VTABLE_SIZE];

/* 0: QueryInterface */
static int __stdcall WD_QueryInterface(WrappedDevice *self, void *riid, void **ppv) {
    typedef int (__stdcall *FN)(void*, void*, void**);
    return ((FN)RealVtbl(self)[0])(self->pReal, riid, ppv);
}

/* 1: AddRef */
static unsigned long __stdcall WD_AddRef(WrappedDevice *self) {
    typedef unsigned long (__stdcall *FN)(void*);
    self->refCount++;
    return ((FN)RealVtbl(self)[1])(self->pReal);
}

/* 2: Release */
static unsigned long __stdcall WD_Release(WrappedDevice *self) {
    typedef unsigned long (__stdcall *FN)(void*);
    unsigned long rc = ((FN)RealVtbl(self)[2])(self->pReal);
    self->refCount--;
    if (self->refCount <= 0) {
        HeapFree(GetProcessHeap(), 0, self);
    }
    return rc;
}

/* ---- Relay thunks for non-intercepted methods ---- */

#ifdef _MSC_VER
#define RELAY_THUNK(name, slot) \
    static __declspec(naked) void __stdcall name(void) { \
        __asm { mov eax, [esp+4] } \
        __asm { mov ecx, [eax+4] } \
        __asm { mov [esp+4], ecx } \
        __asm { mov eax, [ecx] } \
        __asm { jmp dword ptr [eax + slot*4] } \
    }

RELAY_THUNK(Relay_03, 3)
RELAY_THUNK(Relay_04, 4)
RELAY_THUNK(Relay_05, 5)
RELAY_THUNK(Relay_06, 6)
RELAY_THUNK(Relay_07, 7)
RELAY_THUNK(Relay_08, 8)
RELAY_THUNK(Relay_09, 9)
RELAY_THUNK(Relay_10, 10)
RELAY_THUNK(Relay_11, 11)
RELAY_THUNK(Relay_12, 12)
RELAY_THUNK(Relay_13, 13)
RELAY_THUNK(Relay_14, 14)
RELAY_THUNK(Relay_15, 15)
RELAY_THUNK(Relay_18, 18)
RELAY_THUNK(Relay_19, 19)
RELAY_THUNK(Relay_20, 20)
RELAY_THUNK(Relay_21, 21)
RELAY_THUNK(Relay_22, 22)
RELAY_THUNK(Relay_23, 23)
RELAY_THUNK(Relay_24, 24)
RELAY_THUNK(Relay_25, 25)
RELAY_THUNK(Relay_27, 27)
RELAY_THUNK(Relay_28, 28)
RELAY_THUNK(Relay_29, 29)
RELAY_THUNK(Relay_30, 30)
RELAY_THUNK(Relay_31, 31)
RELAY_THUNK(Relay_32, 32)
RELAY_THUNK(Relay_33, 33)
RELAY_THUNK(Relay_34, 34)
RELAY_THUNK(Relay_35, 35)
RELAY_THUNK(Relay_36, 36)
RELAY_THUNK(Relay_37, 37)
RELAY_THUNK(Relay_38, 38)
RELAY_THUNK(Relay_39, 39)
RELAY_THUNK(Relay_40, 40)
RELAY_THUNK(Relay_43, 43)
RELAY_THUNK(Relay_45, 45)
RELAY_THUNK(Relay_46, 46)
RELAY_THUNK(Relay_47, 47)
RELAY_THUNK(Relay_48, 48)
RELAY_THUNK(Relay_49, 49)
RELAY_THUNK(Relay_50, 50)
RELAY_THUNK(Relay_51, 51)
RELAY_THUNK(Relay_52, 52)
RELAY_THUNK(Relay_53, 53)
RELAY_THUNK(Relay_54, 54)
RELAY_THUNK(Relay_55, 55)
RELAY_THUNK(Relay_56, 56)
RELAY_THUNK(Relay_57, 57)
RELAY_THUNK(Relay_58, 58)
RELAY_THUNK(Relay_59, 59)
RELAY_THUNK(Relay_60, 60)
RELAY_THUNK(Relay_61, 61)
RELAY_THUNK(Relay_62, 62)
RELAY_THUNK(Relay_63, 63)
RELAY_THUNK(Relay_64, 64)
RELAY_THUNK(Relay_65, 65)
RELAY_THUNK(Relay_66, 66)
RELAY_THUNK(Relay_67, 67)
RELAY_THUNK(Relay_68, 68)
RELAY_THUNK(Relay_69, 69)
RELAY_THUNK(Relay_70, 70)
RELAY_THUNK(Relay_71, 71)
RELAY_THUNK(Relay_72, 72)
RELAY_THUNK(Relay_73, 73)
RELAY_THUNK(Relay_74, 74)
RELAY_THUNK(Relay_75, 75)
RELAY_THUNK(Relay_76, 76)
RELAY_THUNK(Relay_77, 77)
RELAY_THUNK(Relay_78, 78)
RELAY_THUNK(Relay_79, 79)
RELAY_THUNK(Relay_80, 80)
RELAY_THUNK(Relay_83, 83)
RELAY_THUNK(Relay_84, 84)
RELAY_THUNK(Relay_85, 85)
RELAY_THUNK(Relay_86, 86)
RELAY_THUNK(Relay_87, 87)
RELAY_THUNK(Relay_88, 88)
RELAY_THUNK(Relay_89, 89)
RELAY_THUNK(Relay_90, 90)
RELAY_THUNK(Relay_91, 91)
RELAY_THUNK(Relay_92, 92)
RELAY_THUNK(Relay_93, 93)
RELAY_THUNK(Relay_95, 95)
RELAY_THUNK(Relay_96, 96)
RELAY_THUNK(Relay_97, 97)
RELAY_THUNK(Relay_98, 98)
RELAY_THUNK(Relay_99, 99)
RELAY_THUNK(Relay_101, 101)
RELAY_THUNK(Relay_102, 102)
RELAY_THUNK(Relay_103, 103)
RELAY_THUNK(Relay_104, 104)
RELAY_THUNK(Relay_105, 105)
RELAY_THUNK(Relay_106, 106)
RELAY_THUNK(Relay_107, 107)
RELAY_THUNK(Relay_108, 108)
RELAY_THUNK(Relay_109, 109)
RELAY_THUNK(Relay_110, 110)
RELAY_THUNK(Relay_111, 111)
RELAY_THUNK(Relay_112, 112)
RELAY_THUNK(Relay_113, 113)
RELAY_THUNK(Relay_114, 114)
RELAY_THUNK(Relay_115, 115)
RELAY_THUNK(Relay_116, 116)
RELAY_THUNK(Relay_117, 117)
RELAY_THUNK(Relay_118, 118)

#else
#error "Only MSVC x86 is supported"
#endif

/* ---- Intercepted methods ---- */

/* 26: CreateVertexBuffer — hook vtable of first Remix VB */
static int __stdcall WD_CreateVertexBuffer(WrappedDevice *self,
    unsigned int length, unsigned int usage, unsigned int fvf, unsigned int pool,
    void **ppVB, void *pSharedHandle)
{
    typedef int (__stdcall *FN)(void*,unsigned int,unsigned int,unsigned int,
                                unsigned int,void**,void*);
    int hr = ((FN)RealVtbl(self)[SLOT_CreateVertexBuffer])(
        self->pReal, length, usage, fvf, pool, ppVB, pSharedHandle);
    if (hr == 0 && ppVB && *ppVB) vb_hook_vtable(*ppVB);
    return hr;
}

/* 100: SetStreamSource — track stream-0 VB for centroid lookup */
static int __stdcall WD_SetStreamSource(WrappedDevice *self,
    unsigned int stream, void *pVB, unsigned int offset, unsigned int stride)
{
    typedef int (__stdcall *FN)(void*,unsigned int,void*,unsigned int,unsigned int);
    if (stream == 0)
        g_curStream0VB = pVB;
    return ((FN)RealVtbl(self)[SLOT_SetStreamSource])(
        self->pReal, stream, pVB, offset, stride);
}

/* 16: Reset */
static int __stdcall WD_Reset(WrappedDevice *self, void *pPresentParams) {
    typedef int (__stdcall *FN)(void*, void*);
    self->mvDirty = 0;
    self->viewCapturedThisFrame = 0;
    self->hasCachedWorld = 0;
    self->gameWorldSet = 0;
    self->terrainQ2BypassBudget = 0;
    return ((FN)RealVtbl(self)[SLOT_Reset])(self->pReal, pPresentParams);
}

/* 17: Present — end of frame */
static int __stdcall WD_Present(WrappedDevice *self, void *a, void *b, void *c, void *d) {
    typedef int (__stdcall *FN)(void*, void*, void*, void*, void*);
    int hr;

    hr = ((FN)RealVtbl(self)[SLOT_Present])(self->pReal, a, b, c, d);

    self->frameCount++;
    self->drawCallCount = 0;
    self->mvDirty = 0;
    self->viewCapturedThisFrame = 0;
    self->hasCachedWorld = 0;
    self->terrainQ2BypassBudget = 0;
    g_centroidOverrides = 0;
    g_centroidMisses    = 0;
    g_centroidNoVB      = 0;
    g_centroidNearOrigin= 0;
    g_svcfMvWrites      = 0;
    g_svcfMvpWrites     = 0;
    g_gameWorldSets     = 0;
    g_mvpWorldExtracts  = 0;

    return hr;
}

/* 41: BeginScene */
static int __stdcall WD_BeginScene(WrappedDevice *self) {
    typedef int (__stdcall *FN)(void*);
    self->sceneCount++;
    return ((FN)RealVtbl(self)[SLOT_BeginScene])(self->pReal);
}

/* 42: EndScene */
static int __stdcall WD_EndScene(WrappedDevice *self) {
    typedef int (__stdcall *FN)(void*);
    return ((FN)RealVtbl(self)[SLOT_EndScene])(self->pReal);
}

/* 44: SetTransform — capture real View, store Projection, forward WORLD */
static int __stdcall WD_SetTransform(WrappedDevice *self, unsigned int state, float *pMatrix) {
    typedef int (__stdcall *FN)(void*, unsigned int, float*);

    if (state == D3DTS_VIEW && pMatrix) {
        if (g_cfgForceIdentityView) {
            float viewIdentity[16];
            memcpy(viewIdentity, s_identity, sizeof(viewIdentity));
            return ((FN)RealVtbl(self)[SLOT_SetTransform])(self->pReal,
                D3DTS_VIEW, viewIdentity);
        }

        /* Block identity VIEW from reaching Remix. Other engine code paths
         * besides SetRenderMatrices send identity VIEW. If Remix sees it, the
         * camera resets to origin and terrain/light/assets break. */
        if (mat4_isIdentity(pMatrix))
            return 0;

        /* Cache for WORLD decomposition when translation is present. */
        {
            float txRow = pMatrix[12], tyRow = pMatrix[13], tzRow = pMatrix[14];
            float txCol = pMatrix[3],  tyCol = pMatrix[7],  tzCol = pMatrix[11];
            float tRow2 = txRow*txRow + tyRow*tyRow + tzRow*tzRow;
            float tCol2 = txCol*txCol + tyCol*tyCol + tzCol*tzCol;

            if (tRow2 > 1.0f || tCol2 > 1.0f) {
                memcpy(self->viewMatrix, pMatrix, 16 * sizeof(float));
                mat4_invOrthogonal(self->invViewMatrix, self->viewMatrix);
                self->viewCapturedThisFrame = 1;
                self->hasRealView = 1;
            }
        }

        /* Forward non-identity VIEW to Remix. */
        {
            float copy[16];
            memcpy(copy, pMatrix, 16 * sizeof(float));
            return ((FN)RealVtbl(self)[SLOT_SetTransform])(self->pReal,
                D3DTS_VIEW, copy);
        }
    }

    /* Track game WORLD writes — forward to real device AND set flag */
    if (state >= 256 && state < 512) {
        g_gameWorldSets++;
        self->gameWorldSet = 1;
    }

    /* Capture Projection matrix */
    if (state == D3DTS_PROJECTION && pMatrix) {
        memcpy(self->projMatrix, pMatrix, 16 * sizeof(float));
        self->hasProjection = 1;
        self->invProjDirty = 1;
    }

    return ((FN)RealVtbl(self)[SLOT_SetTransform])(self->pReal, state, pMatrix);
}

/* 94: SetVertexShaderConstantF — capture c0-c3 (MVP) and c17-c20 (MV) */
static int __stdcall WD_SetVertexShaderConstantF(WrappedDevice *self,
    unsigned int startReg, float *pData, unsigned int count)
{
    typedef int (__stdcall *FN)(void*, unsigned int, float*, unsigned int);
    unsigned int caller = caller_u32();

    ensure_module_bases();

    if (pData) {
        unsigned int endReg = startReg + count;

        /* Capture c0-c3 range (MVP): transpose column-major -> row-major */
        if (startReg < 4 && endReg > 0) {
            if (startReg == 0 && endReg >= 4) {
                mat4_transpose(self->mvpMatrix, pData);
            } else {
                float raw[16];
                unsigned int r;
                mat4_transpose(raw, self->mvpMatrix);
                for (r = startReg; r < endReg && r < 4; r++) {
                    unsigned int off = r * 4;
                    unsigned int src = (r - startReg) * 4;
                    raw[off+0] = pData[src+0];
                    raw[off+1] = pData[src+1];
                    raw[off+2] = pData[src+2];
                    raw[off+3] = pData[src+3];
                }
                mat4_transpose(self->mvpMatrix, raw);
            }
            g_svcfMvpWrites++;
            self->hasMVP = 1;
            self->gameWorldSet = 0;
        }

        /* Capture c17-c20 range (MV): transpose column-major -> row-major */
        if (endReg > VS_REG_MV_START && startReg < VS_REG_MV_END) {
            if (startReg <= VS_REG_MV_START && endReg >= VS_REG_MV_END) {
                float *mvSrc = pData + (VS_REG_MV_START - startReg) * 4;
                mat4_transpose(self->mvMatrix, mvSrc);
            } else {
                float raw[16];
                unsigned int r;
                mat4_transpose(raw, self->mvMatrix);
                for (r = startReg; r < endReg; r++) {
                    if (r >= VS_REG_MV_START && r < VS_REG_MV_END) {
                        unsigned int off = (r - VS_REG_MV_START) * 4;
                        unsigned int src = (r - startReg) * 4;
                        raw[off+0] = pData[src+0];
                        raw[off+1] = pData[src+1];
                        raw[off+2] = pData[src+2];
                        raw[off+3] = pData[src+3];
                    }
                }
                mat4_transpose(self->mvMatrix, raw);
            }
            self->mvDirty = 1;
            g_svcfMvWrites++;
        }

        if (g_cfgTerrainQ2LatchGuard &&
            (caller_matches_rva(caller, g_terrainShaderBase, RVA_TERRAIN_SVCF_A) ||
             caller_matches_rva(caller, g_terrainShaderBase, RVA_TERRAIN_SVCF_B))) {
            self->terrainQ2BypassBudget = TERRAIN_Q2_BYPASS_DRAWS;
        }
    }

    return ((FN)RealVtbl(self)[SLOT_SetVertexShaderConstantF])(self->pReal,
        startReg, pData, count);
}

/* 81: DrawPrimitive */
static int __stdcall WD_DrawPrimitive(WrappedDevice *self,
    unsigned int pt, unsigned int sv, unsigned int pc)
{
    typedef int (__stdcall *FN)(void*, unsigned int, unsigned int, unsigned int);
    int hr;

    if (g_cfgTerrainQ2LatchGuard && self->terrainQ2BypassBudget > 0) {
        self->terrainQ2BypassBudget--;
        return ((FN)RealVtbl(self)[SLOT_DrawPrimitive])(self->pReal, pt, sv, pc);
    }

    self->drawCallCount++;
    applyMvPath(self);
    if (g_cfgWorldOverrideOnDP)
        applyCentroidOrMvpPath(self);

    hr = ((FN)RealVtbl(self)[SLOT_DrawPrimitive])(self->pReal, pt, sv, pc);
    return hr;
}

/* 82: DrawIndexedPrimitive */
static int __stdcall WD_DrawIndexedPrimitive(WrappedDevice *self,
    unsigned int pt, int bvi, unsigned int mi, unsigned int nv,
    unsigned int si, unsigned int pc)
{
    typedef int (__stdcall *FN)(void*, unsigned int, int, unsigned int,
                                unsigned int, unsigned int, unsigned int);
    int hr;
    unsigned int caller = caller_u32();

    ensure_module_bases();

    if (g_cfgTerrainQ2LatchGuard && self->terrainQ2BypassBudget > 0 &&
        caller_matches_rva(caller, g_dxwrapperBase, RVA_DXWRAP_DIP_CALLER)) {
        self->terrainQ2BypassBudget--;
        return ((FN)RealVtbl(self)[SLOT_DrawIndexedPrimitive])(self->pReal,
            pt, bvi, mi, nv, si, pc);
    }

    self->drawCallCount++;
    applyMvPath(self);
    if (g_cfgWorldOverrideOnDIP)
        applyCentroidOrMvpPath(self);

    hr = ((FN)RealVtbl(self)[SLOT_DrawIndexedPrimitive])(self->pReal,
        pt, bvi, mi, nv, si, pc);
    return hr;
}

/* ---- Build vtable ---- */

WrappedDevice* WrappedDevice_Create(void *pRealDevice) {
    WrappedDevice *w;
    int i;

    w = (WrappedDevice*)HeapAlloc(GetProcessHeap(), 8 /*HEAP_ZERO_MEMORY*/,
                                  sizeof(WrappedDevice));
    if (!w) return NULL;

    s_device_vtbl[0]  = (void*)WD_QueryInterface;
    s_device_vtbl[1]  = (void*)WD_AddRef;
    s_device_vtbl[2]  = (void*)WD_Release;
    s_device_vtbl[3]  = (void*)Relay_03;
    s_device_vtbl[4]  = (void*)Relay_04;
    s_device_vtbl[5]  = (void*)Relay_05;
    s_device_vtbl[6]  = (void*)Relay_06;
    s_device_vtbl[7]  = (void*)Relay_07;
    s_device_vtbl[8]  = (void*)Relay_08;
    s_device_vtbl[9]  = (void*)Relay_09;
    s_device_vtbl[10] = (void*)Relay_10;
    s_device_vtbl[11] = (void*)Relay_11;
    s_device_vtbl[12] = (void*)Relay_12;
    s_device_vtbl[13] = (void*)Relay_13;
    s_device_vtbl[14] = (void*)Relay_14;
    s_device_vtbl[15] = (void*)Relay_15;
    s_device_vtbl[16] = (void*)WD_Reset;
    s_device_vtbl[17] = (void*)WD_Present;
    s_device_vtbl[18] = (void*)Relay_18;
    s_device_vtbl[19] = (void*)Relay_19;
    s_device_vtbl[20] = (void*)Relay_20;
    s_device_vtbl[21] = (void*)Relay_21;
    s_device_vtbl[22] = (void*)Relay_22;
    s_device_vtbl[23] = (void*)Relay_23;
    s_device_vtbl[24] = (void*)Relay_24;
    s_device_vtbl[25] = (void*)Relay_25;
    s_device_vtbl[26] = (void*)WD_CreateVertexBuffer;
    s_device_vtbl[27] = (void*)Relay_27;
    s_device_vtbl[28] = (void*)Relay_28;
    s_device_vtbl[29] = (void*)Relay_29;
    s_device_vtbl[30] = (void*)Relay_30;
    s_device_vtbl[31] = (void*)Relay_31;
    s_device_vtbl[32] = (void*)Relay_32;
    s_device_vtbl[33] = (void*)Relay_33;
    s_device_vtbl[34] = (void*)Relay_34;
    s_device_vtbl[35] = (void*)Relay_35;
    s_device_vtbl[36] = (void*)Relay_36;
    s_device_vtbl[37] = (void*)Relay_37;
    s_device_vtbl[38] = (void*)Relay_38;
    s_device_vtbl[39] = (void*)Relay_39;
    s_device_vtbl[40] = (void*)Relay_40;
    s_device_vtbl[41] = (void*)WD_BeginScene;
    s_device_vtbl[42] = (void*)WD_EndScene;
    s_device_vtbl[43] = (void*)Relay_43;
    s_device_vtbl[44] = (void*)WD_SetTransform;
    s_device_vtbl[45] = (void*)Relay_45;
    s_device_vtbl[46] = (void*)Relay_46;
    s_device_vtbl[47] = (void*)Relay_47;
    s_device_vtbl[48] = (void*)Relay_48;
    s_device_vtbl[49] = (void*)Relay_49;
    s_device_vtbl[50] = (void*)Relay_50;
    s_device_vtbl[51] = (void*)Relay_51;
    s_device_vtbl[52] = (void*)Relay_52;
    s_device_vtbl[53] = (void*)Relay_53;
    s_device_vtbl[54] = (void*)Relay_54;
    s_device_vtbl[55] = (void*)Relay_55;
    s_device_vtbl[56] = (void*)Relay_56;
    s_device_vtbl[57] = (void*)Relay_57;
    s_device_vtbl[58] = (void*)Relay_58;
    s_device_vtbl[59] = (void*)Relay_59;
    s_device_vtbl[60] = (void*)Relay_60;
    s_device_vtbl[61] = (void*)Relay_61;
    s_device_vtbl[62] = (void*)Relay_62;
    s_device_vtbl[63] = (void*)Relay_63;
    s_device_vtbl[64] = (void*)Relay_64;
    s_device_vtbl[65] = (void*)Relay_65;
    s_device_vtbl[66] = (void*)Relay_66;
    s_device_vtbl[67] = (void*)Relay_67;
    s_device_vtbl[68] = (void*)Relay_68;
    s_device_vtbl[69] = (void*)Relay_69;
    s_device_vtbl[70] = (void*)Relay_70;
    s_device_vtbl[71] = (void*)Relay_71;
    s_device_vtbl[72] = (void*)Relay_72;
    s_device_vtbl[73] = (void*)Relay_73;
    s_device_vtbl[74] = (void*)Relay_74;
    s_device_vtbl[75] = (void*)Relay_75;
    s_device_vtbl[76] = (void*)Relay_76;
    s_device_vtbl[77] = (void*)Relay_77;
    s_device_vtbl[78] = (void*)Relay_78;
    s_device_vtbl[79] = (void*)Relay_79;
    s_device_vtbl[80] = (void*)Relay_80;
    s_device_vtbl[81] = (void*)WD_DrawPrimitive;
    s_device_vtbl[82] = (void*)WD_DrawIndexedPrimitive;
    s_device_vtbl[83] = (void*)Relay_83;
    s_device_vtbl[84] = (void*)Relay_84;
    s_device_vtbl[85] = (void*)Relay_85;
    s_device_vtbl[86] = (void*)Relay_86;
    s_device_vtbl[87] = (void*)Relay_87;
    s_device_vtbl[88] = (void*)Relay_88;
    s_device_vtbl[89] = (void*)Relay_89;
    s_device_vtbl[90] = (void*)Relay_90;
    s_device_vtbl[91] = (void*)Relay_91;
    s_device_vtbl[92] = (void*)Relay_92;
    s_device_vtbl[93] = (void*)Relay_93;
    s_device_vtbl[94] = (void*)WD_SetVertexShaderConstantF;
    s_device_vtbl[95] = (void*)Relay_95;
    s_device_vtbl[96] = (void*)Relay_96;
    s_device_vtbl[97] = (void*)Relay_97;
    s_device_vtbl[98] = (void*)Relay_98;
    s_device_vtbl[99] = (void*)Relay_99;
    s_device_vtbl[100] = (void*)WD_SetStreamSource;
    s_device_vtbl[101] = (void*)Relay_101;
    s_device_vtbl[102] = (void*)Relay_102;
    s_device_vtbl[103] = (void*)Relay_103;
    s_device_vtbl[104] = (void*)Relay_104;
    s_device_vtbl[105] = (void*)Relay_105;
    s_device_vtbl[106] = (void*)Relay_106;
    s_device_vtbl[107] = (void*)Relay_107;
    s_device_vtbl[108] = (void*)Relay_108;
    s_device_vtbl[109] = (void*)Relay_109;
    s_device_vtbl[110] = (void*)Relay_110;
    s_device_vtbl[111] = (void*)Relay_111;
    s_device_vtbl[112] = (void*)Relay_112;
    s_device_vtbl[113] = (void*)Relay_113;
    s_device_vtbl[114] = (void*)Relay_114;
    s_device_vtbl[115] = (void*)Relay_115;
    s_device_vtbl[116] = (void*)Relay_116;
    s_device_vtbl[117] = (void*)Relay_117;
    s_device_vtbl[118] = (void*)Relay_118;

    w->vtbl = s_device_vtbl;
    w->pReal = pRealDevice;
    w->refCount = 1;
    w->frameCount = 0;
    w->mvDirty = 0;
    w->hasMVP = 0;
    w->viewCapturedThisFrame = 0;
    w->hasRealView = 0;
    w->hasProjection = 0;
    w->invProjDirty = 1;
    w->hasCachedWorld = 0;
    w->gameWorldSet = 0;
    w->terrainQ2BypassBudget = 0;
    w->drawCallCount = 0;
    w->sceneCount = 0;

    for (i = 0; i < 16; i++) {
        float v = (i % 5 == 0) ? 1.0f : 0.0f;
        w->mvMatrix[i] = v;
        w->mvpMatrix[i] = v;
        w->viewMatrix[i] = v;
        w->invViewMatrix[i] = v;
        w->projMatrix[i] = v;
        w->invProjMatrix[i] = v;
        w->cachedWorld[i] = v;
    }

    return w;
}
