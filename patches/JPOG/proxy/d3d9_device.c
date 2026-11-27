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
#define REMIX_ALLOW_X86
#define REMIX_WINAPI_NO_LIBRARY_LOADER
#include "remix/remix_c.h"

/* No-CRT memcpy */
#pragma function(memcpy)
void * __cdecl memcpy(void *dst, const void *src, unsigned int n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

/* No-CRT memset */
#pragma function(memset)
void * __cdecl memset(void *dst, int c, unsigned int n) {
    unsigned char *d = (unsigned char *)dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

/* Logging (from d3d9_main.c) */
extern void log_str(const char *s);
extern void log_hex(const char *prefix, unsigned int val);
extern FARPROC Proxy_GetChainExport(const char *name);
extern int Proxy_IsWrapperInterface(const GUID *iid, int isDevice);

typedef HRESULT (__stdcall *PFN_SetOuterDevice)(void *inner, void *outerDevice, void *outerDirect3D);

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

#define D3DLOCK_READONLY            0x00000010u

/* ---- Device vtable slot indices ---- */
enum {
    SLOT_QueryInterface = 0,
    SLOT_AddRef = 1,
    SLOT_Release = 2,
    SLOT_Reset = 16,
    SLOT_Present = 17,
    SLOT_CreateTexture = 23,
    SLOT_CreateVertexBuffer = 26,
    SLOT_UpdateSurface = 30,
    SLOT_UpdateTexture = 31,
    SLOT_StretchRect = 34,
    SLOT_ColorFill = 35,
    SLOT_SetRenderTarget = 37,
    SLOT_BeginScene = 41,
    SLOT_EndScene = 42,
    SLOT_SetTransform = 44,
    SLOT_GetTransform = 45,
    SLOT_SetRenderState = 57,
    SLOT_SetTexture = 65,
    SLOT_SetTextureStageState = 67,
    SLOT_DrawPrimitive = 81,
    SLOT_DrawIndexedPrimitive = 82,
    SLOT_SetVertexDeclaration = 87,
    SLOT_SetFVF = 89,
    SLOT_CreateVertexShader = 91,
    SLOT_SetVertexShader = 92,
    SLOT_SetVertexShaderConstantF = 94,
    SLOT_SetStreamSource = 100,
    SLOT_SetPixelShader = 107,
    SLOT_SetPixelShaderConstantF = 109,
    DEVICE_VTABLE_SIZE = 119
};

/* ---- WrappedDevice ---- */

typedef struct WrappedDevice {
    void *const *vtbl;
    void *pReal;
    int refCount;
    void *pOuterDirect3D;
    PFN_SetOuterDevice setOuterDevice;
    /* MV from c17-c20 (transposed to D3D row-major) */
    float mvMatrix[16];
    int   mvDirty;
    unsigned int mvValidRegisters;

    /* MVP from c0-c3 (transposed to D3D row-major) */
    float mvpMatrix[16];
    int   hasMVP;
    unsigned int mvpValidRegisters;

    /* View matrix (real from ASI or rotation-only approximation) */
    float viewMatrix[16];
    float invViewMatrix[16];
    int   hasRealView;

    /* Projection from SetTransform(D3DTS_PROJECTION) */
    float projMatrix[16];

    /* Cached inv(Projection) for MVP decomposition */
    float invProjMatrix[16];
    int   hasProjection;
    int   invProjDirty;

    /* Last WORLD submitted to the real device by any path. */
    float appliedWorld[16];
    int   hasAppliedWorld;

    /* Set when game calls SetTransform(WORLD); cleared on new MVP write */
    int   gameWorldSet;

    void *texture0;
    void *texture1;

    unsigned int stream0Stride;
    unsigned int stream0Offset;
    void *vertexDeclaration;
    unsigned int currentFVF;
    int usesFVF;
    void *vertexShader;
    void *pixelShader;
    void *skinDeclaration;
} WrappedDevice;

static __inline void** RealVtbl(WrappedDevice *self) {
    return *(void***)(self->pReal);
}

typedef struct VertexElement9 {
    unsigned short stream;
    unsigned short offset;
    unsigned char type;
    unsigned char method;
    unsigned char usage;
    unsigned char usageIndex;
} VertexElement9;

/* TSkinShaderD3D's 40-byte vertex layout stores blend weights/indices at +24/+28,
 * not DIFFUSE/SPECULAR. Binding POS/NORM/UV omits those bytes so FFP draws never
 * feed weight/index data as vertex colors into Remix. */
static int ensure_skin_declaration(WrappedDevice *self) {
    typedef int (__stdcall *FN)(void*, const VertexElement9*, void**);
    static const VertexElement9 elements[] = {
        { 0, 0,  2, 0, 0, 0 },
        { 0, 12, 2, 0, 3, 0 },
        { 0, 32, 1, 0, 5, 0 },
        { 0xFF, 0, 17, 0, 0, 0 }
    };

    if (self->skinDeclaration)
        return 1;
    if (((FN)RealVtbl(self)[86])(self->pReal, elements,
        &self->skinDeclaration) != 0)
        return 0;
    log_str("Skin POS/NORM/UV declaration created\r\n");
    return 1;
}

/* ---- Matrix helpers ---- */

static void mat4_transpose(float *dst, const float *src) {
    dst[0]  = src[0];  dst[1]  = src[4];  dst[2]  = src[8];  dst[3]  = src[12];
    dst[4]  = src[1];  dst[5]  = src[5];  dst[6]  = src[9];  dst[7]  = src[13];
    dst[8]  = src[2];  dst[9]  = src[6];  dst[10] = src[10]; dst[11] = src[14];
    dst[12] = src[3];  dst[13] = src[7];  dst[14] = src[11]; dst[15] = src[15];
}

static void mat4_invOrthogonal(float *dst, const float *src) {
    double tx = (double)src[12], ty = (double)src[13], tz = (double)src[14];
    dst[0]  = src[0]; dst[1]  = src[4]; dst[2]  = src[8];  dst[3]  = 0.0f;
    dst[4]  = src[1]; dst[5]  = src[5]; dst[6]  = src[9];  dst[7]  = 0.0f;
    dst[8]  = src[2]; dst[9]  = src[6]; dst[10] = src[10]; dst[11] = 0.0f;
    dst[12] = (float)(-(tx*(double)src[0] + ty*(double)src[1] + tz*(double)src[2]));
    dst[13] = (float)(-(tx*(double)src[4] + ty*(double)src[5] + tz*(double)src[6]));
    dst[14] = (float)(-(tx*(double)src[8] + ty*(double)src[9] + tz*(double)src[10]));
    dst[15] = 1.0f;
}

static void mat4_multiply(float *dst, const float *a, const float *b) {
    float tmp[16];
    int i, j, k;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            double sum = 0.0;
            for (k = 0; k < 4; k++)
                sum += (double)a[i*4+k] * (double)b[k*4+j];
            tmp[i*4+j] = (float)sum;
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
        float diff = m[i] - expected;
        if (diff > 1e-5f || diff < -1e-5f) return 0;
    }
    return 1;
}

static int mat4_equals(const float *a, const float *b) {
    int i;
    for (i = 0; i < 16; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

static void set_world_if_changed(WrappedDevice *self, const float *world) {
    typedef int (__stdcall *FN_SetTransform)(void*, unsigned int, float*);
    int hr;

    if (self->hasAppliedWorld && mat4_equals(self->appliedWorld, world))
        return;

    hr = ((FN_SetTransform)RealVtbl(self)[SLOT_SetTransform])(
        self->pReal, D3DTS_WORLD, (float*)world);
    if (hr == 0) {
        memcpy(self->appliedWorld, world, sizeof(self->appliedWorld));
        self->hasAppliedWorld = 1;
    }
}

/* ---- VB vtable hook: capture first-vertex XYZ at upload time ---- */

static const GUID g_vbSampleKey = {0x879712c1,0x4e79,0x49ac,{0x92,0x71,0x75,0x66,0x93,0x3a,0x42,0x11}};
static const GUID g_vbLockKey = {0x879712c2,0x4e79,0x49ac,{0x92,0x71,0x75,0x66,0x93,0x3a,0x42,0x11}};
static int g_vbVtblHooked;
static void *g_origVBLock, *g_origVBUnlock;
static void *g_curStream0VB;

/* Set by the ASI while TTerrainShaderD3D renders a terrain packet. */
__declspec(dllexport) volatile int g_terrainRenderActive = 0;

/* Bitflags set by the ASI while an FFP shader submits ModelView (World*View)
 * via D3DTS_WORLD: bit 0 (1) = TSysShaderD3D, bit 1 (2) = TSkinShaderD3D,
 * bit 2 (4) = main camera pass (stabilize World into g_staticWorldCache),
 * bit 3 (8) = TSpriteShaderD3D 3D pass (decompose without static-world snap). */
__declspec(dllexport) volatile int g_mvWorldTransformActive = 0;

/* Cleared on Present/Reset, set when a real non-identity D3DTS_VIEW is accepted. */
__declspec(dllexport) volatile int g_viewSyncedThisFrame = 0;

/* Spatial cache that snaps MV*inv(View) float32 round-trip jitter back to each
 * object's canonical World matrix so Remix's exact-position matcher hits.
 * POS_EPS (0.005) absorbs ~500-1000 unit translation round-trip noise (~0.0005);
 * ROT_EPS (5e-5, ~0.003 deg) absorbs [-1,1] rotation noise (~1e-6) without
 * quantizing gentle idle tree sway. When translation matches a cached anchor
 * but rotation moves (swaying tree), only translation snaps so the root stays
 * bit-exact while rotation passes through smoothly. */
#define STATIC_WORLD_BUCKETS     8192u
#define STATIC_WORLD_WAY         4u
#define STATIC_WORLD_INV_CELL    50.0f   /* 1 / 0.02 world units */
#define STATIC_WORLD_HALF_CELL   0.01f
#define STATIC_WORLD_POS_EPS     0.005f
#define STATIC_WORLD_ROT_EPS     0.00005f
#define STATIC_WORLD_SWAY_EPS    0.05f

typedef struct StaticWorldEntry {
    int cx, cy, cz;
    unsigned int lastFrame;
    unsigned char used;
    float mat[16];
} StaticWorldEntry;

static StaticWorldEntry g_staticWorldCache[STATIC_WORLD_BUCKETS * STATIC_WORLD_WAY];
static unsigned int g_staticWorldFrame = 1u;

static float abs_f(float v) {
    return v < 0.0f ? -v : v;
}

static int fast_floor_int(float x) {
    int i = (int)x;
    return (x < (float)i) ? (i - 1) : i;
}

static unsigned int hash_cell(int cx, int cy, int cz) {
    unsigned int h = (unsigned int)cx * 73856093u;
    h ^= (unsigned int)cy * 19349663u;
    h ^= (unsigned int)cz * 83492791u;
    h ^= h >> 16;
    return h & (STATIC_WORLD_BUCKETS - 1u);
}

static int rot3x3_within(const float *a, const float *b, float eps) {
    return abs_f(a[0]  - b[0])  < eps &&
           abs_f(a[1]  - b[1])  < eps &&
           abs_f(a[2]  - b[2])  < eps &&
           abs_f(a[4]  - b[4])  < eps &&
           abs_f(a[5]  - b[5])  < eps &&
           abs_f(a[6]  - b[6])  < eps &&
           abs_f(a[8]  - b[8])  < eps &&
           abs_f(a[9]  - b[9])  < eps &&
           abs_f(a[10] - b[10]) < eps;
}

static void stabilize_static_world_matrix(float w[16]) {
    float tx = w[12], ty = w[13], tz = w[14];
    int bx, by, bz, ox, oy, oz;
    int cx, cy, cz;
    unsigned int bucket, i, evictIdx;
    unsigned int oldestAge;
    StaticWorldEntry *posMatch = NULL;
    StaticWorldEntry *swayMatch = NULL;

    if (!(tx > -1.0e6f && tx < 1.0e6f &&
          ty > -1.0e6f && ty < 1.0e6f &&
          tz > -1.0e6f && tz < 1.0e6f))
        return;

    bx = fast_floor_int((tx - STATIC_WORLD_HALF_CELL) * STATIC_WORLD_INV_CELL);
    by = fast_floor_int((ty - STATIC_WORLD_HALF_CELL) * STATIC_WORLD_INV_CELL);
    bz = fast_floor_int((tz - STATIC_WORLD_HALF_CELL) * STATIC_WORLD_INV_CELL);

    for (ox = 0; ox <= 1; ox++) {
        for (oy = 0; oy <= 1; oy++) {
            for (oz = 0; oz <= 1; oz++) {
                int qx = bx + ox, qy = by + oy, qz = bz + oz;
                StaticWorldEntry *base = &g_staticWorldCache[hash_cell(qx, qy, qz) * STATIC_WORLD_WAY];
                for (i = 0; i < STATIC_WORLD_WAY; i++) {
                    StaticWorldEntry *e = &base[i];
                    if (!e->used || e->cx != qx || e->cy != qy || e->cz != qz)
                        continue;
                    if (abs_f(tx - e->mat[12]) < STATIC_WORLD_POS_EPS &&
                        abs_f(ty - e->mat[13]) < STATIC_WORLD_POS_EPS &&
                        abs_f(tz - e->mat[14]) < STATIC_WORLD_POS_EPS) {
                        if (rot3x3_within(w, e->mat, STATIC_WORLD_ROT_EPS)) {
                            e->lastFrame = g_staticWorldFrame;
                            memcpy(w, e->mat, 16 * sizeof(float));
                            return;
                        }
                        if (!posMatch)
                            posMatch = e;
                        if (!swayMatch && rot3x3_within(w, e->mat, STATIC_WORLD_SWAY_EPS))
                            swayMatch = e;
                    }
                }
            }
        }
    }

    if (swayMatch) {
        w[12] = swayMatch->mat[12];
        w[13] = swayMatch->mat[13];
        w[14] = swayMatch->mat[14];
        swayMatch->lastFrame = g_staticWorldFrame;
        memcpy(swayMatch->mat, w, 16 * sizeof(float));
        return;
    }

    if (posMatch) {
        w[12] = posMatch->mat[12];
        w[13] = posMatch->mat[13];
        w[14] = posMatch->mat[14];
    }

    cx = fast_floor_int(w[12] * STATIC_WORLD_INV_CELL);
    cy = fast_floor_int(w[13] * STATIC_WORLD_INV_CELL);
    cz = fast_floor_int(w[14] * STATIC_WORLD_INV_CELL);
    bucket = hash_cell(cx, cy, cz) * STATIC_WORLD_WAY;
    evictIdx = 0;
    oldestAge = 0;
    for (i = 0; i < STATIC_WORLD_WAY; i++) {
        StaticWorldEntry *e = &g_staticWorldCache[bucket + i];
        if (!e->used) {
            evictIdx = i;
            break;
        }
        if ((g_staticWorldFrame - e->lastFrame) >= oldestAge) {
            oldestAge = g_staticWorldFrame - e->lastFrame;
            evictIdx = i;
        }
    }
    {
        StaticWorldEntry *dst = &g_staticWorldCache[bucket + evictIdx];
        dst->cx = cx;
        dst->cy = cy;
        dst->cz = cz;
        dst->lastFrame = g_staticWorldFrame;
        dst->used = 1;
        memcpy(dst->mat, w, 16 * sizeof(float));
    }
}

/* Managed by the ASI around TGrassShaderHAL::Flush: 0=inactive, 1=awaiting
 * first grass draw (to feed Remix's grass coverage texture), 2=done for flush. */
__declspec(dllexport) volatile int g_grassFlushState = 0;

#define REMIX_TAG_HASH_MAX 256
typedef struct { unsigned __int64 hash; unsigned __int64 epoch; } TextureHashCache;
static const GUID g_textureHashKey = {0x879712c3,0x4e79,0x49ac,{0x92,0x71,0x75,0x66,0x93,0x3a,0x42,0x11}};
static const GUID g_iidIDirect3DTexture9 = {0x85c31227,0x3de5,0x4f00,{0x9b,0x3a,0xf1,0x1a,0xc3,0x8c,0x18,0xb5}};
static remixapi_Interface g_remix = { 0 };
static unsigned __int64 *g_terrainHashes;
static unsigned int g_terrainHashCapacity;
static int g_terrainHashCount = 0;
static unsigned __int64 g_grassCoverageHashes[REMIX_TAG_HASH_MAX];
static unsigned int g_grassCoverageHashCount = 0;
static FILETIME g_remixTagConfigWriteTime = { 0, 0 };
static int g_remixTagConfigLoaded = 0;
static int g_loggedTerrainScope = 0;
static int g_loggedGrassCoverageLatch = 0;
static int g_loggedRemixInitFailure = 0;
static int g_loggedTextureHashFailure = 0;
static int g_loggedTextureHashZero = 0;
static int g_loggedTerrainAddFailure = 0;
static unsigned __int64 g_textureHashEpoch = 1;
static int resource_set_data(void*, const GUID*, const void*, unsigned int);
static int resource_get_data(void*, const GUID*, void*, unsigned int);
static void resource_free_data(void*, const GUID*);
static DWORD g_nextRemixTagConfigPoll = 0;

static int initialize_remix_api(void) {
    HMODULE hRemix;
    PFN_remixapi_InitializeLibrary initializeLibrary;
    remixapi_InitializeLibraryInfo info;

    if (g_remix.AddTextureHash && g_remix.dxvk_GetTextureHash)
        return 1;

    hRemix = GetModuleHandleA("d3d9_remix.dll");
    if (!hRemix) {
        if (!g_loggedRemixInitFailure) {
            log_str("Terrain tag: d3d9_remix.dll is not loaded\r\n");
            g_loggedRemixInitFailure = 1;
        }
        return 0;
    }
    initializeLibrary = (PFN_remixapi_InitializeLibrary)GetProcAddress(
        hRemix, "remixapi_InitializeLibrary");
    if (!initializeLibrary) {
        if (!g_loggedRemixInitFailure) {
            log_str("Terrain tag: remixapi_InitializeLibrary is unavailable\r\n");
            g_loggedRemixInitFailure = 1;
        }
        return 0;
    }

    info.sType = REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
    info.pNext = NULL;
    info.version = REMIXAPI_VERSION_MAKE(
        REMIXAPI_VERSION_MAJOR,
        REMIXAPI_VERSION_MINOR,
        REMIXAPI_VERSION_PATCH);
    if (initializeLibrary(&info, &g_remix) != REMIXAPI_ERROR_CODE_SUCCESS) {
        if (!g_loggedRemixInitFailure) {
            log_str("Terrain tag: Remix API initialization failed\r\n");
            g_loggedRemixInitFailure = 1;
        }
        return 0;
    }
    if (!g_remix.AddTextureHash || !g_remix.dxvk_GetTextureHash) {
        if (!g_loggedRemixInitFailure) {
            log_str("Terrain tag: required Remix API functions are unavailable\r\n");
            g_loggedRemixInitFailure = 1;
        }
        return 0;
    }
    return 1;
}

static void hash_to_hex(char out[17], unsigned __int64 hash) {
    static const char digits[] = "0123456789abcdef";
    int i;
    for (i = 15; i >= 0; i--) {
        out[i] = digits[(unsigned int)(hash & 0xF)];
        hash >>= 4;
    }
    out[16] = '\0';
}

static int get_texture_hash(void *texture, unsigned __int64 *hash);

static void tag_terrain_texture(void *texture) {
    unsigned __int64 hash;
    char hashText[17];
    int i;

    if (!texture) {
        if (!g_loggedTextureHashFailure) {
            log_str("Terrain tag: terrain draw has no stage-0 texture\r\n");
            g_loggedTextureHashFailure = 1;
        }
        return;
    }
    if (!initialize_remix_api())
        return;
    if (!get_texture_hash(texture, &hash)) {
        if (!g_loggedTextureHashFailure) {
            log_str("Terrain tag: texture hash lookup failed\r\n");
            g_loggedTextureHashFailure = 1;
        }
        return;
    }
    for (i = 0; i < g_terrainHashCount; i++) {
        if (g_terrainHashes[i] == hash)
            return;
    }

    hash_to_hex(hashText, hash);
    if ((unsigned int)g_terrainHashCount == g_terrainHashCapacity) {
        unsigned int capacity = g_terrainHashCapacity ? g_terrainHashCapacity * 2 : 64;
        void *grown = g_terrainHashes
            ? HeapReAlloc(GetProcessHeap(), 0, g_terrainHashes, capacity * sizeof(*g_terrainHashes))
            : HeapAlloc(GetProcessHeap(), 0, capacity * sizeof(*g_terrainHashes));
        if (!grown) return;
        g_terrainHashes = grown;
        g_terrainHashCapacity = capacity;
    }
    if (g_remix.AddTextureHash("session:rtx.terrainTextures", hashText) !=
        REMIXAPI_ERROR_CODE_SUCCESS) {
        if (!g_loggedTerrainAddFailure) {
            log_str("Terrain tag: AddTextureHash failed\r\n");
            g_loggedTerrainAddFailure = 1;
        }
        return;
    }
    g_terrainHashes[g_terrainHashCount++] = hash;
    log_str("Auto-tagged terrain texture: 0x");
    log_str(hashText);
    log_str("\r\n");
}

static int hex_digit_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int text_matches(const char *text, unsigned int available,
    const char *expected, unsigned int expectedLength)
{
    unsigned int i;
    if (available < expectedLength)
        return 0;
    for (i = 0; i < expectedLength; i++) {
        if (text[i] != expected[i])
            return 0;
    }
    return 1;
}

static void remove_tag_hash(unsigned __int64 *hashes, unsigned int *count,
    unsigned __int64 hash)
{
    unsigned int i;
    for (i = 0; i < *count; i++) {
        if (hashes[i] == hash) {
            hashes[i] = hashes[*count - 1];
            (*count)--;
            return;
        }
    }
}

static void add_tag_hash(unsigned __int64 *hashes, unsigned int *count,
    unsigned __int64 hash)
{
    unsigned int i;
    for (i = 0; i < *count; i++) {
        if (hashes[i] == hash)
            return;
    }
    if (*count < REMIX_TAG_HASH_MAX)
        hashes[(*count)++] = hash;
}

static void parse_tag_option(const char *data, unsigned int size,
    const char *key, unsigned int keyLength,
    unsigned __int64 *hashes, unsigned int *count)
{
    unsigned int lineStart = 0;
    *count = 0;

    while (lineStart < size) {
        unsigned int position = lineStart;
        unsigned int lineEnd = lineStart;
        while (lineEnd < size && data[lineEnd] != '\r' && data[lineEnd] != '\n')
            lineEnd++;
        while (position < lineEnd && (data[position] == ' ' || data[position] == '\t'))
            position++;

        if (text_matches(data + position, lineEnd - position, key, keyLength)) {
            position += keyLength;
            while (position < lineEnd && (data[position] == ' ' || data[position] == '\t'))
                position++;
            if (position < lineEnd && data[position] == '=') {
                position++;
                while (position < lineEnd) {
                    unsigned __int64 hash = 0;
                    unsigned int digits = 0;
                    int remove = 0;
                    int digit;

                    while (position < lineEnd &&
                        (data[position] == ' ' || data[position] == '\t' ||
                         data[position] == ','))
                        position++;
                    if (position < lineEnd && data[position] == '-') {
                        remove = 1;
                        position++;
                    }
                    if (position + 1 < lineEnd && data[position] == '0' &&
                        (data[position + 1] == 'x' || data[position + 1] == 'X'))
                        position += 2;
                    while (position < lineEnd && digits < 16 &&
                        (digit = hex_digit_value(data[position])) >= 0) {
                        hash = (hash << 4) | (unsigned int)digit;
                        digits++;
                        position++;
                    }
                    if (digits) {
                        if (remove)
                            remove_tag_hash(hashes, count, hash);
                        else
                            add_tag_hash(hashes, count, hash);
                    } else if (position < lineEnd) {
                        position++;
                    }
                }
            }
        }

        lineStart = lineEnd;
        while (lineStart < size && (data[lineStart] == '\r' || data[lineStart] == '\n'))
            lineStart++;
    }
}

static void reload_remix_tag_config(void) {
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    HANDLE file;
    DWORD size;
    DWORD bytesRead;
    char *data;
    unsigned int grassCoverageCount;

    DWORD now = GetTickCount();
    if ((LONG)(now - g_nextRemixTagConfigPoll) < 0)
        return;
    g_nextRemixTagConfigPoll = now + 1000;
    if (!GetFileAttributesExA("rtx.conf", GetFileExInfoStandard, &attributes))
        return;
    if (g_remixTagConfigLoaded &&
        CompareFileTime(&attributes.ftLastWriteTime, &g_remixTagConfigWriteTime) == 0)
        return;

    file = CreateFileA("rtx.conf", GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return;
    size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size > 1024 * 1024) {
        CloseHandle(file);
        return;
    }
    data = (char*)HeapAlloc(GetProcessHeap(), 0, size ? size : 1);
    if (!data) {
        CloseHandle(file);
        return;
    }
    if (!ReadFile(file, data, size, &bytesRead, NULL) || bytesRead != size) {
        HeapFree(GetProcessHeap(), 0, data);
        CloseHandle(file);
        return;
    }
    CloseHandle(file);

    parse_tag_option(data, size,
        "rtx.legacyMaterial.worldSpaceSecondaryOpacityTextures",
        sizeof("rtx.legacyMaterial.worldSpaceSecondaryOpacityTextures") - 1,
        g_grassCoverageHashes, &grassCoverageCount);
    g_grassCoverageHashCount = grassCoverageCount;
    g_remixTagConfigWriteTime = attributes.ftLastWriteTime;
    g_remixTagConfigLoaded = 1;
    HeapFree(GetProcessHeap(), 0, data);
    log_hex("Reloaded grass coverage tags=", grassCoverageCount);
}

static int get_texture_hash(void *texture, unsigned __int64 *hash) {
    TextureHashCache cached;
    if (!texture || !hash || !initialize_remix_api()) return 0;
    if (resource_get_data(texture, &g_textureHashKey, &cached, sizeof(cached)) && cached.epoch == g_textureHashEpoch) {
        *hash = cached.hash;
        return 1;
    }
    if (g_remix.dxvk_GetTextureHash((IDirect3DTexture9*)texture, hash) != REMIXAPI_ERROR_CODE_SUCCESS || !*hash) return 0;
    cached.hash = *hash;
    cached.epoch = g_textureHashEpoch;
    resource_set_data(texture, &g_textureHashKey, &cached, sizeof(cached));
    return 1;
}

static int texture_hash_is_tagged(unsigned __int64 hash,
    const unsigned __int64 *hashes, unsigned int count)
{
    unsigned int i;
    for (i = 0; i < count; i++) {
        if (hashes[i] == hash)
            return 1;
    }
    return 0;
}

static int resource_set_data(void *resource, const GUID *key, const void *data, unsigned int size) {
    typedef int (__stdcall *FN)(void*, const GUID*, const void*, unsigned int, unsigned int);
    return ((FN)(*(void***)resource)[4])(resource, key, data, size, 0);
}

static int resource_get_data(void *resource, const GUID *key, void *data, unsigned int size) {
    typedef int (__stdcall *FN)(void*, const GUID*, void*, unsigned int*);
    return ((FN)(*(void***)resource)[5])(resource, key, data, &size) >= 0;
}

static void resource_free_data(void *resource, const GUID *key) {
    typedef int (__stdcall *FN)(void*, const GUID*);
    if (resource)
        ((FN)(*(void***)resource)[6])(resource, key);
}

static void invalidate_texture_hash(void *texture) {
    resource_free_data(texture, &g_textureHashKey);
}

static void invalidate_surface_texture_hash(void *surface) {
    typedef int (__stdcall *FN_GetContainer)(void*, const GUID*, void**);
    typedef unsigned long (__stdcall *FN_Release)(void*);
    void *container = NULL;
    if (!surface) return;
    if (((FN_GetContainer)(*(void***)surface)[11])(
            surface, &g_iidIDirect3DTexture9, &container) >= 0 && container) {
        invalidate_texture_hash(container);
        ((FN_Release)(*(void***)container)[2])(container);
    }
}

/* ---- Texture / Surface vtable hooks: invalidate cached hash on pixel mutation ---- */

static int g_texVtblHooked;
static void *g_origTexGetSurfaceLevel, *g_origTexLockRect;
static int g_surfVtblHooked;
static void *g_origSurfLockRect;

static int __stdcall SurfLockRect_Hook(void *pSurf,
    void *pLockedRect, const void *pRect, unsigned int flags) {
    typedef int (__stdcall *FN)(void*, void*, const void*, unsigned int);
    int hr = ((FN)g_origSurfLockRect)(pSurf, pLockedRect, pRect, flags);
    if (hr >= 0 && !(flags & D3DLOCK_READONLY))
        invalidate_surface_texture_hash(pSurf);
    return hr;
}

static void surf_hook_vtable(void *pSurf) {
    void **vtbl;
    DWORD oldProt;
    if (g_surfVtblHooked || !pSurf) return;
    vtbl = *(void***)pSurf;
    if (!VirtualProtect(&vtbl[13], sizeof(void*), PAGE_READWRITE, &oldProt)) return;
    g_origSurfLockRect = vtbl[13];
    vtbl[13] = (void*)SurfLockRect_Hook;
    VirtualProtect(&vtbl[13], sizeof(void*), oldProt, &oldProt);
    g_surfVtblHooked = 1;
}

static int __stdcall TexGetSurfaceLevel_Hook(void *pTex,
    unsigned int level, void **ppSurface) {
    typedef int (__stdcall *FN)(void*, unsigned int, void**);
    int hr = ((FN)g_origTexGetSurfaceLevel)(pTex, level, ppSurface);
    if (hr >= 0 && ppSurface && *ppSurface)
        surf_hook_vtable(*ppSurface);
    return hr;
}

static int __stdcall TexLockRect_Hook(void *pTex,
    unsigned int level, void *pLockedRect, const void *pRect, unsigned int flags) {
    typedef int (__stdcall *FN)(void*, unsigned int, void*, const void*, unsigned int);
    int hr = ((FN)g_origTexLockRect)(pTex, level, pLockedRect, pRect, flags);
    if (hr >= 0 && !(flags & D3DLOCK_READONLY))
        invalidate_texture_hash(pTex);
    return hr;
}

static void tex_hook_vtable(void *pTex) {
    void **vtbl;
    DWORD oldProt;
    if (g_texVtblHooked || !pTex) return;
    vtbl = *(void***)pTex;
    if (!VirtualProtect(&vtbl[18], sizeof(void*) * 2, PAGE_READWRITE, &oldProt)) return;
    g_origTexGetSurfaceLevel = vtbl[18];
    g_origTexLockRect = vtbl[19];
    vtbl[18] = (void*)TexGetSurfaceLevel_Hook;
    vtbl[19] = (void*)TexLockRect_Hook;
    VirtualProtect(&vtbl[18], sizeof(void*) * 2, oldProt, &oldProt);
    g_texVtblHooked = 1;
}

static int __stdcall VBLock_Hook(void *pVB,
    unsigned int offset, unsigned int size, void **ppData, unsigned int flags) {
    typedef int (__stdcall *FN)(void*, unsigned int, unsigned int, void**, unsigned int);
    int hr = ((FN)g_origVBLock)(pVB, offset, size, ppData, flags);
    if (hr >= 0) {
        typedef int (__stdcall *FreeData)(void*, const GUID*);
        ((FreeData)(*(void***)pVB)[6])(pVB, &g_vbSampleKey);
        if (size == 0) {
            typedef int (__stdcall *GetDesc)(void*, unsigned int*);
            unsigned int desc[6];
            if (((GetDesc)(*(void***)pVB)[13])(pVB, desc) >= 0 && desc[4] >= offset)
                size = desc[4] - offset;
        }
        if (ppData && offset == 0 && size >= 12) {
            void *data = *ppData;
            resource_set_data(pVB, &g_vbLockKey, &data, sizeof(data));
        }
    }
    return hr;
}

static int __stdcall VBUnlock_Hook(void *pVB) {
    typedef int (__stdcall *FN)(void*);
    typedef int (__stdcall *FreeData)(void*, const GUID*);
    float *data = NULL;
    if (resource_get_data(pVB, &g_vbLockKey, &data, sizeof(data))) {
        if (data) resource_set_data(pVB, &g_vbSampleKey, data, 3 * sizeof(float));
        ((FreeData)(*(void***)pVB)[6])(pVB, &g_vbLockKey);
    }
    return ((FN)g_origVBUnlock)(pVB);
}

static void vb_hook_vtable(void *pVB) {
    void **vtbl;
    DWORD oldProt;
    if (g_vbVtblHooked) return;
    vtbl = *(void***)pVB;
    if (!VirtualProtect(&vtbl[11], sizeof(void*) * 2, PAGE_READWRITE, &oldProt)) return;
    g_origVBLock = vtbl[11];
    g_origVBUnlock = vtbl[12];
    vtbl[11] = (void*)VBLock_Hook;
    vtbl[12] = (void*)VBUnlock_Hook;
    VirtualProtect(&vtbl[11], sizeof(void*) * 2, oldProt, &oldProt);
    g_vbVtblHooked = 1;
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
 * Runs unconditionally for every draw. Returns 1 when it derived a
 * rotation-correct WORLD from the game's real per-object matrix, so the
 * centroid/MVP path below knows not to clobber it with an identity rotation.
 */
static int applyMvPath(WrappedDevice *self) {
    float world[16];

    if (!self->mvDirty) {
        /* MV unchanged — default WORLD to identity (world-space geometry).
         * Model-space objects will be overridden by centroid/MVP path after. */
        set_world_if_changed(self, s_identity);
        return 0;
    }
    self->mvDirty = 0;

    if (!mat4_isRigid(self->mvMatrix)) {
        return 0;
    }

    if (!self->hasRealView) {
        return 0;
    }

    mat4_multiply(world, self->mvMatrix, self->invViewMatrix);
    world[3] = 0.0f; world[7] = 0.0f; world[11] = 0.0f; world[15] = 1.0f;
    set_world_if_changed(self, world);
    return 1;
}

/*
 * Centroid / MVP path: override WORLD for objects without game-set transforms.
 * Skips when game already called SetTransform(WORLD) for this object.
 */
static void applyCentroidOrMvpPath(WrappedDevice *self) {
    float world[16];
    float centroid[3];

    if (self->gameWorldSet) return;

    /* MVP decomposition: World = MVP × inv(P) × inv(V). Exact for any VS-based
     * draw that uploaded c0-c3 (carries the object's real rotation), so it
     * always takes priority over the centroid guess below, which cannot
     * represent rotation at all.
     * Split into two steps to avoid precision loss from inverting View×Proj
     * as a single matrix (large camera translations make V×P ill-conditioned). */
    if (self->hasMVP && self->hasRealView && ensureInvProj(self)) {
        float mv[16];
        mat4_multiply(mv, self->mvpMatrix, self->invProjMatrix);
        mat4_multiply(world, mv, self->invViewMatrix);
        world[3] = 0.0f; world[7] = 0.0f; world[11] = 0.0f; world[15] = 1.0f;
        set_world_if_changed(self, world);
        return;
    }

    /* Centroid lookup from VB sample table — last-resort placement guess for
     * FFP draws with no shader constants at all; identity rotation only. */
    if (g_curStream0VB && self->stream0Offset == 0) {
        if (resource_get_data(g_curStream0VB, &g_vbSampleKey, centroid, sizeof(centroid))) {
            float dist2 = centroid[0]*centroid[0] +
                           centroid[1]*centroid[1] +
                           centroid[2]*centroid[2];
            if (dist2 > CENTROID_MIN_DIST2) {
                memcpy(world, s_identity, sizeof(world));
                world[12] = centroid[0];
                world[13] = centroid[1];
                world[14] = centroid[2];
                set_world_if_changed(self, world);
            }
        }
    }
}

/* ---- Vtable method implementations ---- */

static unsigned long __stdcall WD_AddRef(WrappedDevice *self);

/* 0: QueryInterface */
static int __stdcall WD_QueryInterface(WrappedDevice *self, void *riid, void **ppv) {
    typedef int (__stdcall *FN)(void*, void*, void**);
    if (!ppv) return (int)0x80004003;
    if (Proxy_IsWrapperInterface((const GUID*)riid, 1)) {
        WD_AddRef(self);
        *ppv = self;
        return 0;
    }
    return ((FN)RealVtbl(self)[0])(self->pReal, riid, ppv);
}

/* 1: AddRef */
static unsigned long __stdcall WD_AddRef(WrappedDevice *self) {
    typedef unsigned long (__stdcall *FN)(void*);
    InterlockedIncrement((LONG*)&self->refCount);
    return ((FN)RealVtbl(self)[1])(self->pReal);
}

/* 2: Release */
static unsigned long __stdcall WD_Release(WrappedDevice *self) {
    typedef unsigned long (__stdcall *FN)(void*);
    unsigned long rc;
    const int finalRelease = InterlockedDecrement((LONG*)&self->refCount) == 0;
    if (finalRelease && self->skinDeclaration) {
        ((FN)(*(void***)self->skinDeclaration)[2])(self->skinDeclaration);
        self->skinDeclaration = NULL;
    }
    if (finalRelease && self->setOuterDevice) {
        // The bridge borrows these pointers. Clear them while the inner device is still alive.
        self->setOuterDevice(self->pReal, NULL, NULL);
    }
    rc = ((FN)RealVtbl(self)[2])(self->pReal);
    if (finalRelease) {
        void **ownerVtable = *(void***)self->pOuterDirect3D;
        ((FN)ownerVtable[2])(self->pOuterDirect3D);
        HeapFree(GetProcessHeap(), 0, self);
    }
    return rc;
}

static int __stdcall WD_GetDirect3D(WrappedDevice *self, void **result) {
    typedef unsigned long (__stdcall *FN)(void*);
    void **ownerVtable;
    if (!result) return (int)0x80004003;
    ownerVtable = *(void***)self->pOuterDirect3D;
    ((FN)ownerVtable[1])(self->pOuterDirect3D);
    *result = self->pOuterDirect3D;
    return 0;
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
RELAY_THUNK(Relay_24, 24)
RELAY_THUNK(Relay_25, 25)
RELAY_THUNK(Relay_27, 27)
RELAY_THUNK(Relay_28, 28)
RELAY_THUNK(Relay_29, 29)
RELAY_THUNK(Relay_32, 32)
RELAY_THUNK(Relay_33, 33)
RELAY_THUNK(Relay_36, 36)
RELAY_THUNK(Relay_38, 38)
RELAY_THUNK(Relay_39, 39)
RELAY_THUNK(Relay_40, 40)
RELAY_THUNK(Relay_41, 41)
RELAY_THUNK(Relay_42, 42)
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

/* 23: CreateTexture — hook vtable of first Remix texture */
static int __stdcall WD_CreateTexture(WrappedDevice *self,
    unsigned int width, unsigned int height, unsigned int levels,
    unsigned int usage, unsigned int format, unsigned int pool,
    void **ppTexture, void *pSharedHandle)
{
    typedef int (__stdcall *FN)(void*, unsigned int, unsigned int, unsigned int,
                                unsigned int, unsigned int, unsigned int, void**, void*);
    int hr = ((FN)RealVtbl(self)[SLOT_CreateTexture])(
        self->pReal, width, height, levels, usage, format, pool, ppTexture, pSharedHandle);
    if (hr == 0 && ppTexture && *ppTexture) tex_hook_vtable(*ppTexture);
    return hr;
}

/* 26: CreateVertexBuffer — hook vtable of first Remix VB */
static int __stdcall WD_CreateVertexBuffer(WrappedDevice *self,
    unsigned int length, unsigned int usage, unsigned int fvf, unsigned int pool,
    void **ppVB, void *pSharedHandle)
{
    typedef int (__stdcall *FN)(void*,unsigned int,unsigned int,unsigned int,
                                unsigned int,void**,void*);
    int hr = ((FN)RealVtbl(self)[SLOT_CreateVertexBuffer])(
        self->pReal, length, usage, fvf, pool, ppVB, pSharedHandle);
    if (hr == 0 && ppVB && *ppVB)
        vb_hook_vtable(*ppVB);
    return hr;
}

/* 30: UpdateSurface — invalidate cached hash of destination texture */
static int __stdcall WD_UpdateSurface(WrappedDevice *self,
    void *pSrcSurface, const void *pSrcRect, void *pDstSurface, const void *pDestPoint)
{
    typedef int (__stdcall *FN)(void*, void*, const void*, void*, const void*);
    int hr = ((FN)RealVtbl(self)[SLOT_UpdateSurface])(
        self->pReal, pSrcSurface, pSrcRect, pDstSurface, pDestPoint);
    if (hr >= 0) invalidate_surface_texture_hash(pDstSurface);
    return hr;
}

/* 31: UpdateTexture — invalidate cached hash of destination texture */
static int __stdcall WD_UpdateTexture(WrappedDevice *self,
    void *pSrcTexture, void *pDstTexture)
{
    typedef int (__stdcall *FN)(void*, void*, void*);
    int hr = ((FN)RealVtbl(self)[SLOT_UpdateTexture])(self->pReal, pSrcTexture, pDstTexture);
    if (hr >= 0) invalidate_texture_hash(pDstTexture);
    return hr;
}

/* 34: StretchRect — invalidate cached hash of destination texture */
static int __stdcall WD_StretchRect(WrappedDevice *self,
    void *pSrcSurface, const void *pSrcRect, void *pDstSurface,
    const void *pDstRect, unsigned int filter)
{
    typedef int (__stdcall *FN)(void*, void*, const void*, void*, const void*, unsigned int);
    int hr = ((FN)RealVtbl(self)[SLOT_StretchRect])(
        self->pReal, pSrcSurface, pSrcRect, pDstSurface, pDstRect, filter);
    if (hr >= 0) invalidate_surface_texture_hash(pDstSurface);
    return hr;
}

/* 35: ColorFill — invalidate cached hash of destination texture */
static int __stdcall WD_ColorFill(WrappedDevice *self,
    void *pSurface, const void *pRect, unsigned int color)
{
    typedef int (__stdcall *FN)(void*, void*, const void*, unsigned int);
    int hr = ((FN)RealVtbl(self)[SLOT_ColorFill])(self->pReal, pSurface, pRect, color);
    if (hr >= 0) invalidate_surface_texture_hash(pSurface);
    return hr;
}

/* 37: SetRenderTarget — invalidate cached hash when rendering into a texture */
static int __stdcall WD_SetRenderTarget(WrappedDevice *self,
    unsigned int renderTargetIndex, void *pRenderTarget)
{
    typedef int (__stdcall *FN)(void*, unsigned int, void*);
    int hr = ((FN)RealVtbl(self)[SLOT_SetRenderTarget])(
        self->pReal, renderTargetIndex, pRenderTarget);
    if (hr >= 0) invalidate_surface_texture_hash(pRenderTarget);
    return hr;
}

/* 100: SetStreamSource — track stream-0 VB for centroid lookup */
static int __stdcall WD_SetStreamSource(WrappedDevice *self,
    unsigned int stream, void *pVB, unsigned int offset, unsigned int stride)
{
    typedef int (__stdcall *FN)(void*,unsigned int,void*,unsigned int,unsigned int);
    int hr = ((FN)RealVtbl(self)[SLOT_SetStreamSource])(
        self->pReal, stream, pVB, offset, stride);
    if (hr >= 0 && stream == 0) {
        g_curStream0VB = pVB;
        self->stream0Stride = stride;
        self->stream0Offset = offset;
    }
    return hr;
}

static int __stdcall WD_SetVertexDeclaration(WrappedDevice *self, void *declaration) {
    typedef int (__stdcall *FN)(void*, void*);
    int hr = ((FN)RealVtbl(self)[SLOT_SetVertexDeclaration])(self->pReal, declaration);
    if (hr >= 0) {
        self->vertexDeclaration = declaration;
        self->usesFVF = 0;
    }
    return hr;
}

static int __stdcall WD_SetFVF(WrappedDevice *self, unsigned int fvf) {
    typedef int (__stdcall *FN)(void*, unsigned int);
    typedef int (__stdcall *FN_SetDecl)(void*, void*);
    int hr;
    if ((g_mvWorldTransformActive & 2) && fvf == 0x1D2u && ensure_skin_declaration(self)) {
        hr = ((FN_SetDecl)RealVtbl(self)[SLOT_SetVertexDeclaration])(
            self->pReal, self->skinDeclaration);
        if (hr >= 0) {
            self->vertexDeclaration = self->skinDeclaration;
            self->currentFVF = fvf;
            self->usesFVF = 0;
        }
        return hr;
    }
    hr = ((FN)RealVtbl(self)[SLOT_SetFVF])(self->pReal, fvf);
    if (hr >= 0) { self->currentFVF = fvf; self->usesFVF = 1; }
    return hr;
}

static int __stdcall WD_SetVertexShader(WrappedDevice *self, void *shader) {
    typedef int (__stdcall *FN)(void*, void*);
    int hr = ((FN)RealVtbl(self)[SLOT_SetVertexShader])(self->pReal, shader);
    if (hr >= 0) self->vertexShader = shader;
    return hr;
}

static int __stdcall WD_SetPixelShader(WrappedDevice *self, void *shader) {
    typedef int (__stdcall *FN)(void*, void*);
    int hr = ((FN)RealVtbl(self)[SLOT_SetPixelShader])(self->pReal, shader);
    if (hr >= 0) self->pixelShader = shader;
    return hr;
}

/* 16: Reset */
static int __stdcall WD_Reset(WrappedDevice *self, void *pPresentParams) {
    typedef int (__stdcall *FN)(void*, void*);
    int hr;
    self->mvDirty = 0;
    self->hasMVP = self->hasRealView = self->hasProjection = 0;
    self->mvpValidRegisters = self->mvValidRegisters = 0;
    self->invProjDirty = 1;
    self->hasAppliedWorld = self->gameWorldSet = 0;
    self->texture0 = self->texture1 = self->vertexShader = self->pixelShader = NULL;
    self->vertexDeclaration = NULL;
    self->currentFVF = self->stream0Stride = self->stream0Offset = 0;
    self->usesFVF = 0;
    g_curStream0VB = NULL;
    ++g_textureHashEpoch;
    memset(g_staticWorldCache, 0, sizeof(g_staticWorldCache));
    g_staticWorldFrame = 1u;
    g_viewSyncedThisFrame = 0;
    hr = ((FN)RealVtbl(self)[SLOT_Reset])(self->pReal, pPresentParams);
    return hr;
}

/* 17: Present — end of frame */
static int __stdcall WD_Present(WrappedDevice *self, void *a, void *b, void *c, void *d) {
    typedef int (__stdcall *FN)(void*, void*, void*, void*, void*);
    int hr;

    hr = ((FN)RealVtbl(self)[SLOT_Present])(self->pReal, a, b, c, d);
    reload_remix_tag_config();
    ++g_staticWorldFrame;
    g_viewSyncedThisFrame = 0;

    self->mvDirty = 0;
    self->hasAppliedWorld = 0;
    self->gameWorldSet = 0;

    return hr;
}

/* 44: SetTransform — capture real View, store Projection, forward WORLD */
static int __stdcall WD_SetTransform(WrappedDevice *self, unsigned int state, float *pMatrix) {
    typedef int (__stdcall *FN)(void*, unsigned int, float*);

    int hr;
    if (state == D3DTS_VIEW && pMatrix) {
        /* Block identity VIEW from reaching Remix. Other engine code paths
         * besides SetRenderMatrices send identity VIEW. If Remix sees it, the
         * camera resets to origin and terrain/light/assets break. */
        if (mat4_isIdentity(pMatrix))
            return 0;

        if (self->hasRealView && mat4_equals(self->viewMatrix, pMatrix)) {
            g_viewSyncedThisFrame = 1;
            return 0;
        }

        hr = ((FN)RealVtbl(self)[SLOT_SetTransform])(self->pReal, state, pMatrix);
        if (hr < 0) return hr;

        /* Cache for WORLD decomposition when translation is present. */
        {
            float txRow = pMatrix[12], tyRow = pMatrix[13], tzRow = pMatrix[14];
            float txCol = pMatrix[3],  tyCol = pMatrix[7],  tzCol = pMatrix[11];
            float tRow2 = txRow*txRow + tyRow*tyRow + tzRow*tzRow;
            float tCol2 = txCol*txCol + tyCol*tyCol + tzCol*tzCol;

            if (tRow2 > 1.0f || tCol2 > 1.0f) {
                memcpy(self->viewMatrix, pMatrix, 16 * sizeof(float));
                mat4_invOrthogonal(self->invViewMatrix, self->viewMatrix);
                self->hasRealView = 1;
                g_viewSyncedThisFrame = 1;
            }
        }

        return hr;
    }

    /* Keep synthetic-WORLD deduplication coherent with game WORLD writes.
     * Inside g_mvWorldTransformActive (Toshi FFP shaders that pass ModelView
     * as D3DTS_WORLD with identity VIEW), decompose World = MV * inv(View). */
    if (state >= 256 && state < 512) {
        float decomposed[16];
        float *submitMatrix = pMatrix;
        if (state == D3DTS_WORLD && pMatrix && g_mvWorldTransformActive && self->hasRealView) {
            mat4_multiply(decomposed, pMatrix, self->invViewMatrix);
            decomposed[3] = 0.0f; decomposed[7] = 0.0f;
            decomposed[11] = 0.0f; decomposed[15] = 1.0f;
            if (g_mvWorldTransformActive & 4)
                stabilize_static_world_matrix(decomposed);
            submitMatrix = decomposed;
        }
        hr = ((FN)RealVtbl(self)[SLOT_SetTransform])(
            self->pReal, state, submitMatrix);
        if (hr == 0) {
            self->gameWorldSet = 1;
            if (state == D3DTS_WORLD && submitMatrix) {
                memcpy(self->appliedWorld, submitMatrix, sizeof(self->appliedWorld));
                self->hasAppliedWorld = 1;
                if (g_mvWorldTransformActive)
                    self->mvDirty = 0;
            }
        }
        return hr;
    }

    hr = ((FN)RealVtbl(self)[SLOT_SetTransform])(self->pReal, state, pMatrix);
    /* Capture Projection matrix */
    if (hr >= 0 && state == D3DTS_PROJECTION && pMatrix) {
        memcpy(self->projMatrix, pMatrix, 16 * sizeof(float));
        self->hasProjection = 1;
        self->invProjDirty = 1;
    }

    return hr;
}

/* 65: SetTexture — retain stage 0/1 textures for terrain tagging and grass coverage gating. */
static int __stdcall WD_SetTexture(WrappedDevice *self, unsigned int stage, void *texture) {
    typedef int (__stdcall *FN)(void*, unsigned int, void*);
    int hr = ((FN)RealVtbl(self)[SLOT_SetTexture])(self->pReal, stage, texture);
    if (hr >= 0) {
        if (stage == 0) self->texture0 = texture;
        else if (stage == 1) self->texture1 = texture;
    }
    return hr;
}

/* 94: SetVertexShaderConstantF — capture c0-c3 (MVP) and c17-c20 (MV) */
static int __stdcall WD_SetVertexShaderConstantF(WrappedDevice *self,
    unsigned int startReg, float *pData, unsigned int count)
{
    typedef int (__stdcall *FN)(void*, unsigned int, float*, unsigned int);

    int hr = ((FN)RealVtbl(self)[SLOT_SetVertexShaderConstantF])(self->pReal, startReg, pData, count);
    if (hr < 0) return hr;
    if (pData) {
        unsigned int endReg = startReg + count;

        /* Capture c0-c3 range (MVP): transpose column-major -> row-major */
        if (startReg < 4 && endReg > 0) {
            if (startReg == 0 && endReg >= 4) {
                mat4_transpose(self->mvpMatrix, pData);
                self->mvpValidRegisters = 15;
            } else {
                float raw[16];
                unsigned int r;
                mat4_transpose(raw, self->mvpMatrix);
                for (r = startReg; r < endReg && r < 4; r++) {
                    unsigned int off = r * 4;
                    self->mvpValidRegisters |= 1u << r;
                    unsigned int src = (r - startReg) * 4;
                    raw[off+0] = pData[src+0];
                    raw[off+1] = pData[src+1];
                    raw[off+2] = pData[src+2];
                    raw[off+3] = pData[src+3];
                }
                mat4_transpose(self->mvpMatrix, raw);
            }
            self->hasMVP = self->mvpValidRegisters == 15;
            self->gameWorldSet = 0;
        }

        /* Capture c17-c20 range (MV): transpose column-major -> row-major */
        if (endReg > VS_REG_MV_START && startReg < VS_REG_MV_END) {
            if (startReg <= VS_REG_MV_START && endReg >= VS_REG_MV_END) {
                float *mvSrc = pData + (VS_REG_MV_START - startReg) * 4;
                mat4_transpose(self->mvMatrix, mvSrc);
                self->mvValidRegisters = 15;
            } else {
                float raw[16];
                unsigned int r;
                mat4_transpose(raw, self->mvMatrix);
                for (r = startReg; r < endReg; r++) {
                    if (r >= VS_REG_MV_START && r < VS_REG_MV_END) {
                        unsigned int off = (r - VS_REG_MV_START) * 4;
                        self->mvValidRegisters |= 1u << (r - VS_REG_MV_START);
                        unsigned int src = (r - startReg) * 4;
                        raw[off+0] = pData[src+0];
                        raw[off+1] = pData[src+1];
                        raw[off+2] = pData[src+2];
                        raw[off+3] = pData[src+3];
                    }
                }
                mat4_transpose(self->mvMatrix, raw);
            }
            self->mvDirty = self->mvValidRegisters == 15;
        }
    }

    return hr;
}

/* 81: DrawPrimitive */
static int __stdcall WD_DrawPrimitive(WrappedDevice *self,
    unsigned int pt, unsigned int sv, unsigned int pc)
{
    typedef int (__stdcall *FN)(void*, unsigned int, unsigned int, unsigned int);
    typedef int (__stdcall *FN_SetDeclaration)(void*, void*);
    int hr;

    if (!g_mvWorldTransformActive && !applyMvPath(self))
        applyCentroidOrMvpPath(self);
    if ((g_mvWorldTransformActive & 2) && self->usesFVF && self->currentFVF == 0x1D2u &&
        self->stream0Stride == 40u && !self->vertexShader && ensure_skin_declaration(self)) {
        if (((FN_SetDeclaration)RealVtbl(self)[SLOT_SetVertexDeclaration])(
            self->pReal, self->skinDeclaration) >= 0) {
            self->vertexDeclaration = self->skinDeclaration;
            self->usesFVF = 0;
        }
    }

    if (g_terrainRenderActive) {
        if (!g_loggedTerrainScope) {
            log_str("Terrain render scope observed\r\n");
            g_loggedTerrainScope = 1;
        }
        tag_terrain_texture(self->texture0);
    }

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
    typedef int (__stdcall *FN_SetDeclaration)(void*, void*);
    int hr;
    unsigned __int64 textureHash;

    if (g_grassFlushState >= 2)
        return 0;

    if (!g_mvWorldTransformActive && !applyMvPath(self))
        applyCentroidOrMvpPath(self);

    if ((g_mvWorldTransformActive & 2) && self->usesFVF && self->currentFVF == 0x1D2u &&
        self->stream0Stride == 40u && !self->vertexShader && ensure_skin_declaration(self)) {
        if (((FN_SetDeclaration)RealVtbl(self)[SLOT_SetVertexDeclaration])(
            self->pReal, self->skinDeclaration) >= 0) {
            self->vertexDeclaration = self->skinDeclaration;
            self->usesFVF = 0;
        }
    }

    if (g_terrainRenderActive) {
        if (!g_loggedTerrainScope) {
            log_str("Terrain render scope observed\r\n");
            g_loggedTerrainScope = 1;
        }
        tag_terrain_texture(self->texture0);
    }

    hr = ((FN)RealVtbl(self)[SLOT_DrawIndexedPrimitive])(self->pReal,
        pt, bvi, mi, nv, si, pc);
    if (g_grassFlushState == 1 && hr >= 0 && g_grassCoverageHashCount &&
        self->texture0 && self->texture1 &&
        get_texture_hash(self->texture0, &textureHash) &&
        texture_hash_is_tagged(textureHash, g_grassCoverageHashes, g_grassCoverageHashCount)) {
        g_grassFlushState = 2;
        if (!g_loggedGrassCoverageLatch) {
            log_str("Grass coverage draw latched\r\n");
            g_loggedGrassCoverageLatch = 1;
        }
    }
    return hr;
}

/* ---- Build vtable ---- */

static void *const s_device_vtbl[DEVICE_VTABLE_SIZE] = {
    (void*)WD_QueryInterface, /* 0 */
    (void*)WD_AddRef, /* 1 */
    (void*)WD_Release, /* 2 */
    (void*)Relay_03, /* 3 */
    (void*)Relay_04, /* 4 */
    (void*)Relay_05, /* 5 */
    (void*)WD_GetDirect3D, /* 6 */
    (void*)Relay_07, /* 7 */
    (void*)Relay_08, /* 8 */
    (void*)Relay_09, /* 9 */
    (void*)Relay_10, /* 10 */
    (void*)Relay_11, /* 11 */
    (void*)Relay_12, /* 12 */
    (void*)Relay_13, /* 13 */
    (void*)Relay_14, /* 14 */
    (void*)Relay_15, /* 15 */
    (void*)WD_Reset, /* 16 */
    (void*)WD_Present, /* 17 */
    (void*)Relay_18, /* 18 */
    (void*)Relay_19, /* 19 */
    (void*)Relay_20, /* 20 */
    (void*)Relay_21, /* 21 */
    (void*)Relay_22, /* 22 */
    (void*)WD_CreateTexture, /* 23 */
    (void*)Relay_24, /* 24 */
    (void*)Relay_25, /* 25 */
    (void*)WD_CreateVertexBuffer, /* 26 */
    (void*)Relay_27, /* 27 */
    (void*)Relay_28, /* 28 */
    (void*)Relay_29, /* 29 */
    (void*)WD_UpdateSurface, /* 30 */
    (void*)WD_UpdateTexture, /* 31 */
    (void*)Relay_32, /* 32 */
    (void*)Relay_33, /* 33 */
    (void*)WD_StretchRect, /* 34 */
    (void*)WD_ColorFill, /* 35 */
    (void*)Relay_36, /* 36 */
    (void*)WD_SetRenderTarget, /* 37 */
    (void*)Relay_38, /* 38 */
    (void*)Relay_39, /* 39 */
    (void*)Relay_40, /* 40 */
    (void*)Relay_41, /* 41 */
    (void*)Relay_42, /* 42 */
    (void*)Relay_43, /* 43 */
    (void*)WD_SetTransform, /* 44 */
    (void*)Relay_45, /* 45 */
    (void*)Relay_46, /* 46 */
    (void*)Relay_47, /* 47 */
    (void*)Relay_48, /* 48 */
    (void*)Relay_49, /* 49 */
    (void*)Relay_50, /* 50 */
    (void*)Relay_51, /* 51 */
    (void*)Relay_52, /* 52 */
    (void*)Relay_53, /* 53 */
    (void*)Relay_54, /* 54 */
    (void*)Relay_55, /* 55 */
    (void*)Relay_56, /* 56 */
    (void*)Relay_57, /* 57 */
    (void*)Relay_58, /* 58 */
    (void*)Relay_59, /* 59 */
    (void*)Relay_60, /* 60 */
    (void*)Relay_61, /* 61 */
    (void*)Relay_62, /* 62 */
    (void*)Relay_63, /* 63 */
    (void*)Relay_64, /* 64 */
    (void*)WD_SetTexture, /* 65 */
    (void*)Relay_66, /* 66 */
    (void*)Relay_67, /* 67 */
    (void*)Relay_68, /* 68 */
    (void*)Relay_69, /* 69 */
    (void*)Relay_70, /* 70 */
    (void*)Relay_71, /* 71 */
    (void*)Relay_72, /* 72 */
    (void*)Relay_73, /* 73 */
    (void*)Relay_74, /* 74 */
    (void*)Relay_75, /* 75 */
    (void*)Relay_76, /* 76 */
    (void*)Relay_77, /* 77 */
    (void*)Relay_78, /* 78 */
    (void*)Relay_79, /* 79 */
    (void*)Relay_80, /* 80 */
    (void*)WD_DrawPrimitive, /* 81 */
    (void*)WD_DrawIndexedPrimitive, /* 82 */
    (void*)Relay_83, /* 83 */
    (void*)Relay_84, /* 84 */
    (void*)Relay_85, /* 85 */
    (void*)Relay_86, /* 86 */
    (void*)WD_SetVertexDeclaration, /* 87 */
    (void*)Relay_88, /* 88 */
    (void*)WD_SetFVF, /* 89 */
    (void*)Relay_90, /* 90 */
    (void*)Relay_91, /* 91 */
    (void*)WD_SetVertexShader, /* 92 */
    (void*)Relay_93, /* 93 */
    (void*)WD_SetVertexShaderConstantF, /* 94 */
    (void*)Relay_95, /* 95 */
    (void*)Relay_96, /* 96 */
    (void*)Relay_97, /* 97 */
    (void*)Relay_98, /* 98 */
    (void*)Relay_99, /* 99 */
    (void*)WD_SetStreamSource, /* 100 */
    (void*)Relay_101, /* 101 */
    (void*)Relay_102, /* 102 */
    (void*)Relay_103, /* 103 */
    (void*)Relay_104, /* 104 */
    (void*)Relay_105, /* 105 */
    (void*)Relay_106, /* 106 */
    (void*)WD_SetPixelShader, /* 107 */
    (void*)Relay_108, /* 108 */
    (void*)Relay_109, /* 109 */
    (void*)Relay_110, /* 110 */
    (void*)Relay_111, /* 111 */
    (void*)Relay_112, /* 112 */
    (void*)Relay_113, /* 113 */
    (void*)Relay_114, /* 114 */
    (void*)Relay_115, /* 115 */
    (void*)Relay_116, /* 116 */
    (void*)Relay_117, /* 117 */
    (void*)Relay_118, /* 118 */
};

WrappedDevice* WrappedDevice_Create(void *pRealDevice, void *pOuterDirect3D) {
    WrappedDevice *w;
    int i;

    w = (WrappedDevice*)HeapAlloc(GetProcessHeap(), 8 /*HEAP_ZERO_MEMORY*/,
                                  sizeof(WrappedDevice));
    if (!w) return NULL;

    w->vtbl = s_device_vtbl;
    w->pReal = pRealDevice;
    w->refCount = 1;
    w->pOuterDirect3D = pOuterDirect3D;
    {
        typedef unsigned long (__stdcall *FN)(void*);
        void **ownerVtable = *(void***)pOuterDirect3D;
        ((FN)ownerVtable[1])(pOuterDirect3D);
        w->setOuterDevice = (PFN_SetOuterDevice)Proxy_GetChainExport("BridgeClient_SetOuterDevice");
        if (w->setOuterDevice && w->setOuterDevice(pRealDevice, w, pOuterDirect3D) < 0) {
            log_str("Bridge outer-device registration failed\r\n");
            w->setOuterDevice = NULL;
        }
    }
    w->mvDirty = 0;
    w->hasMVP = 0;
    w->hasRealView = 0;
    w->hasProjection = 0;
    w->invProjDirty = 1;
    w->gameWorldSet = 0;
    w->texture0 = NULL;

    for (i = 0; i < 16; i++) {
        float v = (i % 5 == 0) ? 1.0f : 0.0f;
        w->mvMatrix[i] = v;
        w->mvpMatrix[i] = v;
        w->viewMatrix[i] = v;
        w->invViewMatrix[i] = v;
        w->projMatrix[i] = v;
        w->invProjMatrix[i] = v;
    }

    log_str("WrappedDevice_Create: JPOG Matrix-Capture proxy ready\r\n");
    return w;
}
