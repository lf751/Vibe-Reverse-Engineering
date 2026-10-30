/*
 * Wrapped IDirect3D9 - intercepts CreateDevice to return a wrapped device.
 * All other methods forward to the real IDirect3D9.
 *
 * This file is game-agnostic. No modifications needed per-game.
 *
 * IDirect3D9 vtable (32-bit, 17 methods):
 *   0x00 QueryInterface
 *   0x04 AddRef
 *   0x08 Release
 *   0x0C RegisterSoftwareDevice
 *   0x10 GetAdapterCount
 *   0x14 GetAdapterIdentifier
 *   0x18 GetAdapterModeCount
 *   0x1C EnumAdapterModes
 *   0x20 GetAdapterDisplayMode
 *   0x24 CheckDeviceType
 *   0x28 CheckDeviceFormat
 *   0x2C CheckDeviceMultiSampleType
 *   0x30 CheckDepthStencilMatch
 *   0x34 CheckDeviceFormatConversion
 *   0x38 GetDeviceCaps
 *   0x3C GetAdapterMonitor
 *   0x40 CreateDevice  <-- intercepted
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Logging (from d3d9_main.c) */
extern void log_str(const char *s);
extern void log_hex(const char *prefix, unsigned int val);

/* From d3d9_device.c */
typedef struct WrappedDevice WrappedDevice;
WrappedDevice* WrappedDevice_Create(void* pRealDevice, void* pOuterDirect3D);

/* ---- WrappedD3D9 ---- */

typedef struct WrappedD3D9 {
    void **vtbl;
    void *pReal;
    int refCount;
} WrappedD3D9;

static void *s_d3d9_vtbl[17];

int Proxy_IsWrapperInterface(const GUID *iid, int isDevice) {
    static const GUID unknown = {0, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
    static const GUID direct3D9 = {0x81bdcbca, 0x64d4, 0x426d, {0xae, 0x8d, 0xad, 1, 0x47, 0xf4, 0x27, 0x5c}};
    static const GUID device9 = {0xd0223b96, 0xbf7a, 0x43fd, {0x92, 0xbd, 0xa4, 0x3b, 0x0d, 0x82, 0xb9, 0xeb}};
    const GUID *supported = isDevice ? &device9 : &direct3D9;
    const unsigned char *bytes = (const unsigned char*)iid;
    const unsigned char *unknownBytes = (const unsigned char*)&unknown;
    const unsigned char *supportedBytes = (const unsigned char*)supported;
    int matchesUnknown = 1, matchesSupported = 1, i;
    if (!iid) return 0;
    for (i = 0; i < sizeof(GUID); ++i) {
        matchesUnknown &= bytes[i] == unknownBytes[i];
        matchesSupported &= bytes[i] == supportedBytes[i];
    }
    return matchesUnknown || matchesSupported;
}

static unsigned long __stdcall W9_AddRef(WrappedD3D9 *self);

#define REAL_VTBL(self) (*(void***)(((WrappedD3D9*)(self))->pReal))

static int __stdcall W9_QueryInterface(WrappedD3D9 *self, void *riid, void **ppv) {
    typedef int (__stdcall *FN)(void*, void*, void**);
    if (!ppv) return (int)0x80004003;
    if (Proxy_IsWrapperInterface((const GUID*)riid, 0)) {
        W9_AddRef(self);
        *ppv = self;
        return 0;
    }
    return ((FN)REAL_VTBL(self)[0])(self->pReal, riid, ppv);
}

static unsigned long __stdcall W9_AddRef(WrappedD3D9 *self) {
    InterlockedIncrement((LONG*)&self->refCount);
    typedef unsigned long (__stdcall *FN)(void*);
    return ((FN)REAL_VTBL(self)[1])(self->pReal);
}

static unsigned long __stdcall W9_Release(WrappedD3D9 *self) {
    typedef unsigned long (__stdcall *FN)(void*);
    LONG references = InterlockedDecrement((LONG*)&self->refCount);
    unsigned long rc = ((FN)REAL_VTBL(self)[2])(self->pReal);
    if (references == 0) {
        HeapFree(GetProcessHeap(), 0, self);
    }
    return rc;
}

static int __stdcall W9_RegisterSoftwareDevice(WrappedD3D9 *self, void *p) {
    typedef int (__stdcall *FN)(void*, void*);
    return ((FN)REAL_VTBL(self)[3])(self->pReal, p);
}

static unsigned int __stdcall W9_GetAdapterCount(WrappedD3D9 *self) {
    typedef unsigned int (__stdcall *FN)(void*);
    return ((FN)REAL_VTBL(self)[4])(self->pReal);
}

static int __stdcall W9_GetAdapterIdentifier(WrappedD3D9 *self, unsigned int a, unsigned long f, void *p) {
    typedef int (__stdcall *FN)(void*, unsigned int, unsigned long, void*);
    return ((FN)REAL_VTBL(self)[5])(self->pReal, a, f, p);
}

static unsigned int __stdcall W9_GetAdapterModeCount(WrappedD3D9 *self, unsigned int a, unsigned int fmt) {
    typedef unsigned int (__stdcall *FN)(void*, unsigned int, unsigned int);
    return ((FN)REAL_VTBL(self)[6])(self->pReal, a, fmt);
}

static int __stdcall W9_EnumAdapterModes(WrappedD3D9 *self, unsigned int a, unsigned int fmt, unsigned int m, void *p) {
    typedef int (__stdcall *FN)(void*, unsigned int, unsigned int, unsigned int, void*);
    return ((FN)REAL_VTBL(self)[7])(self->pReal, a, fmt, m, p);
}

static int __stdcall W9_GetAdapterDisplayMode(WrappedD3D9 *self, unsigned int a, void *p) {
    typedef int (__stdcall *FN)(void*, unsigned int, void*);
    return ((FN)REAL_VTBL(self)[8])(self->pReal, a, p);
}

static int __stdcall W9_CheckDeviceType(WrappedD3D9 *self, unsigned int a, unsigned int t, unsigned int f1, unsigned int f2, int w) {
    typedef int (__stdcall *FN)(void*, unsigned int, unsigned int, unsigned int, unsigned int, int);
    return ((FN)REAL_VTBL(self)[9])(self->pReal, a, t, f1, f2, w);
}

static int __stdcall W9_CheckDeviceFormat(WrappedD3D9 *self, unsigned int a, unsigned int t, unsigned int f, unsigned long u, unsigned int rt, unsigned int cf) {
    typedef int (__stdcall *FN)(void*, unsigned int, unsigned int, unsigned int, unsigned long, unsigned int, unsigned int);
    return ((FN)REAL_VTBL(self)[10])(self->pReal, a, t, f, u, rt, cf);
}

static int __stdcall W9_CheckDeviceMultiSampleType(WrappedD3D9 *self, unsigned int a, unsigned int t, unsigned int f, int w, unsigned int ms, unsigned long *q) {
    typedef int (__stdcall *FN)(void*, unsigned int, unsigned int, unsigned int, int, unsigned int, unsigned long*);
    return ((FN)REAL_VTBL(self)[11])(self->pReal, a, t, f, w, ms, q);
}

static int __stdcall W9_CheckDepthStencilMatch(WrappedD3D9 *self, unsigned int a, unsigned int t, unsigned int af, unsigned int rf, unsigned int df) {
    typedef int (__stdcall *FN)(void*, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
    return ((FN)REAL_VTBL(self)[12])(self->pReal, a, t, af, rf, df);
}

static int __stdcall W9_CheckDeviceFormatConversion(WrappedD3D9 *self, unsigned int a, unsigned int t, unsigned int sf, unsigned int tf) {
    typedef int (__stdcall *FN)(void*, unsigned int, unsigned int, unsigned int, unsigned int);
    return ((FN)REAL_VTBL(self)[13])(self->pReal, a, t, sf, tf);
}

static int __stdcall W9_GetDeviceCaps(WrappedD3D9 *self, unsigned int a, unsigned int t, void *caps) {
    typedef int (__stdcall *FN)(void*, unsigned int, unsigned int, void*);
    return ((FN)REAL_VTBL(self)[14])(self->pReal, a, t, caps);
}

static void* __stdcall W9_GetAdapterMonitor(WrappedD3D9 *self, unsigned int a) {
    typedef void* (__stdcall *FN)(void*, unsigned int);
    return ((FN)REAL_VTBL(self)[15])(self->pReal, a);
}

/* CreateDevice: wrap the returned device with our FFP interceptor */
static int __stdcall W9_CreateDevice(WrappedD3D9 *self, unsigned int adapter,
    unsigned int devType, void *hWnd, unsigned long behFlags,
    void *pPresentParams, void **ppDevice)
{
    typedef int (__stdcall *FN)(void*, unsigned int, unsigned int, void*, unsigned long, void*, void**);
    int hr;
    void *pRealDevice = NULL;

    hr = ((FN)REAL_VTBL(self)[16])(self->pReal, adapter, devType, hWnd, behFlags, pPresentParams, &pRealDevice);

    if (hr < 0 || !pRealDevice) {
        log_hex("  CreateDevice FAILED, hr=", (unsigned int)hr);
        if (ppDevice) *ppDevice = NULL;
        return hr;
    }

    {
        WrappedDevice *wrapped = WrappedDevice_Create(pRealDevice, self);
        if (!wrapped) {
            log_str("  ERROR: Failed to create wrapped device\r\n");
            *ppDevice = pRealDevice;
            return hr;
        }
        *ppDevice = (void*)wrapped;
    }

    return hr;
}

/* ---- Build and populate the vtable ---- */

WrappedD3D9* WrappedD3D9_Create(void *pRealD3D9) {
    WrappedD3D9 *w = (WrappedD3D9*)HeapAlloc(GetProcessHeap(), 0, sizeof(WrappedD3D9));
    if (!w) return NULL;

    s_d3d9_vtbl[0]  = (void*)W9_QueryInterface;
    s_d3d9_vtbl[1]  = (void*)W9_AddRef;
    s_d3d9_vtbl[2]  = (void*)W9_Release;
    s_d3d9_vtbl[3]  = (void*)W9_RegisterSoftwareDevice;
    s_d3d9_vtbl[4]  = (void*)W9_GetAdapterCount;
    s_d3d9_vtbl[5]  = (void*)W9_GetAdapterIdentifier;
    s_d3d9_vtbl[6]  = (void*)W9_GetAdapterModeCount;
    s_d3d9_vtbl[7]  = (void*)W9_EnumAdapterModes;
    s_d3d9_vtbl[8]  = (void*)W9_GetAdapterDisplayMode;
    s_d3d9_vtbl[9]  = (void*)W9_CheckDeviceType;
    s_d3d9_vtbl[10] = (void*)W9_CheckDeviceFormat;
    s_d3d9_vtbl[11] = (void*)W9_CheckDeviceMultiSampleType;
    s_d3d9_vtbl[12] = (void*)W9_CheckDepthStencilMatch;
    s_d3d9_vtbl[13] = (void*)W9_CheckDeviceFormatConversion;
    s_d3d9_vtbl[14] = (void*)W9_GetDeviceCaps;
    s_d3d9_vtbl[15] = (void*)W9_GetAdapterMonitor;
    s_d3d9_vtbl[16] = (void*)W9_CreateDevice;

    w->vtbl = s_d3d9_vtbl;
    w->pReal = pRealD3D9;
    w->refCount = 1;

    return w;
}
