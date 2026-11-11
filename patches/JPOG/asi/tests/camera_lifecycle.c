#include "../jpog_view_fix.c"

static int g_viewWrites;
static int g_projectionWrites;
static int __stdcall CaptureTransform(void *device, int kind, void *matrix) {
    (void)device;
    (void)matrix;
    if (kind == D3DTS_VIEW) g_viewWrites++;
    if (kind == D3DTS_PROJECTION) g_projectionWrites++;
    return 0;
}

static void Require(int condition, DWORD code) {
    if (!condition) ExitProcess(code);
}

static void MakeContext(BYTE *context, void **renderer, void **device) {
    float *view = (float*)(context + RCTX_OFF_WORLDVIEW);
    float *projection = (float*)(context + RCTX_OFF_PROJECTION);
    *(void**)(context + RCTX_OFF_IFACE) = renderer;
    *(void**)((BYTE*)renderer + IFACE_OFF_DEVICE) = device;
    view[0] = view[5] = view[10] = view[15] = 1.0f;
    view[12] = 80.0f;
    projection[0] = projection[5] = projection[10] = projection[15] = 1.0f;
}

void TestMain(void) {
    static const BYTE prologue[6] = { 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x04 };
    BYTE *module = (BYTE*)VirtualAlloc(NULL, 0x80000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)module;
    IMAGE_NT_HEADERS *nt;
    BYTE destroy[6] = { 0xC7, 0x01, 0, 0, 0, 0 };
    DWORD table;
    static BYTE cameraA[0x600] = { 0 };
    static BYTE cameraB[0x600] = { 0 };
    static BYTE reflection[0x600] = { 0 };
    static void *renderer[100] = { 0 };
    static void *deviceTable[64] = { 0 };
    void **device = deviceTable;
    Require(module != NULL, 1);
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    nt = (IMAGE_NT_HEADERS*)(module + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->OptionalHeader.SizeOfImage = 0x80000;
    table = (DWORD)(module + 0x74480);
    memcpy(destroy + 2, &table, 4);
    memcpy(module + 0x71C0, destroy, 6);
    module[0x71C6] = 0xC3;
    Require(!InstallRenderHooks((HMODULE)module), 2);
    Require(module[0x71C0] == 0xC7, 3);
    memcpy(module + 0x7180, prologue, 6);
    Require(InstallRenderHooks((HMODULE)module), 4);
    Require(module[0x7180] == 0xE9 && module[0x7185] == 0x90, 5);
    deviceTable[D3D8_VT_SETTRANSFORM / 4] = (void*)CaptureTransform;
    MakeContext(cameraA, renderer, (void**)&device);
    MakeContext(cameraB, renderer, (void**)&device);
    MakeContext(reflection, renderer, (void**)&device);
    ((float*)(reflection + RCTX_OFF_WORLDVIEW))[0] = -1.0f;
    Hook_SetRenderMatrices(reflection, NULL);
    Require(g_mainCameraContext == NULL && g_viewWrites == 0, 6);
    Hook_SetRenderMatrices(cameraA, NULL);
    Require(g_mainCameraContext == cameraA && g_viewWrites == 1 && g_projectionWrites == 1, 7);
    Hook_DestroyRenderContext(cameraB, NULL);
    Require(g_mainCameraContext == cameraA, 8);
    Hook_SetRenderMatrices(cameraB, NULL);
    Require(g_mainCameraContext == cameraA && g_viewWrites == 1, 9);
    Hook_DestroyRenderContext(cameraA, NULL);
    Require(g_mainCameraContext == NULL, 10);
    Require(*(DWORD*)cameraA == table, 11);
    Hook_SetRenderMatrices(reflection, NULL);
    Require(g_mainCameraContext == NULL, 12);
    Hook_SetRenderMatrices(cameraB, NULL);
    Require(g_mainCameraContext == cameraB && g_viewWrites == 2, 13);
    Require(!MatchModuleBytes((HMODULE)module, 0x7FFFF, prologue, 6), 14);
    ExitProcess(0);
}
