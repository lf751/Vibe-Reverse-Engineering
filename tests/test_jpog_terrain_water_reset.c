/* Build as x86: cl /W4 /I. tests\test_jpog_terrain_water_reset.c */
#include <stdio.h>
#include <string.h>
#include "../patches/JPOG/asi/terrain_water_reset.c"

static BYTE moduleBytes[0x155000];
static BYTE ownerBytes[0x400];
static BYTE rendererBytes[0x180];
static void* ownerMethods[0x138 / 4];
static void* deviceMethods[0x160 / 4];
static void** device = deviceMethods;
static unsigned int calls, failedCalls;
static int createdState = 1;

static unsigned char __fastcall Created(void* owner, void* unused) {
    (void)owner; (void)unused;
    return (unsigned char)createdState;
}
static unsigned char __fastcall HighTerrain(void* owner, void* unused) {
    (void)unused;
    return *((BYTE*)owner + 0x2B8);
}
static unsigned char __fastcall HighWater(void* owner, void* unused) {
    (void)unused;
    return *((BYTE*)owner + 0x2B9);
}
static void* __fastcall Renderer(void* owner, void* unused) {
    (void)owner; (void)unused;
    return rendererBytes;
}
static HRESULT CreateHandle(const DWORD* bytecode, DWORD* handle) {
    unsigned int i;
    ++calls;
    for (i = 0; i < 8; ++i) {
        if (bytecode == (const DWORD*)(moduleBytes + g_waterShaders[i].bytecodeRva)) {
            *handle = 0x80000001 + i;
            return failedCalls ? E_FAIL : S_OK;
        }
    }
    return E_INVALIDARG;
}
static HRESULT __stdcall Vertex(void* self, const DWORD* declaration, const DWORD* bytecode,
    DWORD* handle, DWORD usage) {
    if (self != &device || usage ||
        !((bytecode == (const DWORD*)(moduleBytes + 0x430C0) && declaration == (const DWORD*)(moduleBytes + 0x14EAD8)) ||
          (bytecode == (const DWORD*)(moduleBytes + 0x48AC0) && declaration == (const DWORD*)(moduleBytes + 0x14EAE4))))
        return E_INVALIDARG;
    return CreateHandle(bytecode, handle);
}
static HRESULT __stdcall Pixel(void* self, const DWORD* bytecode, DWORD* handle) {
    if (self != &device)
        return E_INVALIDARG;
    return CreateHandle(bytecode, handle);
}
static void Invalidate(void) {
    unsigned int i;
    calls = 0;
    for (i = 0; i < 8; ++i)
        *(DWORD*)(ownerBytes + g_waterShaders[i].handleOffset) = 0xFFFFFFFF;
}
#define CHECK(condition) do { if (!(condition)) { printf("FAIL line %d\n", __LINE__); return 1; } } while (0)

int main(void) {
    unsigned int i;
    g_terrainModule = moduleBytes;
    *(TerrainPredicate*)(moduleBytes + 0x38200) = Created;
    *(void***)ownerBytes = ownerMethods;
    ownerMethods[0xCC / 4] = (void*)HighTerrain;
    ownerMethods[0xD4 / 4] = (void*)HighWater;
    ownerMethods[0x134 / 4] = (void*)Renderer;
    *(void**)(rendererBytes + 0x178) = &device;
    deviceMethods[0x12C / 4] = (void*)Vertex;
    deviceMethods[0x15C / 4] = (void*)Pixel;
    ownerBytes[0x2B9] = 1;

    Invalidate();
    RecreateWaterShaders(ownerBytes);
    CHECK(calls == 8);
    for (i = 0; i < 8; ++i)
        CHECK(*(DWORD*)(ownerBytes + g_waterShaders[i].handleOffset) == 0x80000001 + i);
    RecreateWaterShaders(ownerBytes);
    CHECK(calls == 8); /* Repeated callbacks must preserve live resources. */

    Invalidate();
    RecreateWaterShaders(ownerBytes);
    CHECK(calls == 8); /* Device loss followed by reset recreates all resources. */

    Invalidate();
    *(DWORD*)(ownerBytes + 0x35C) = 0x81234567;
    RecreateWaterShaders(ownerBytes);
    CHECK(calls == 7);
    CHECK(*(DWORD*)(ownerBytes + 0x35C) == 0x81234567);

    Invalidate();
    failedCalls = 1;
    RecreateWaterShaders(ownerBytes);
    CHECK(calls == 8);
    for (i = 0; i < 8; ++i)
        CHECK(*(DWORD*)(ownerBytes + g_waterShaders[i].handleOffset) == 0xFFFFFFFF);
    failedCalls = 0;

    Invalidate();
    ownerBytes[0x2B8] = 1;
    RecreateWaterShaders(ownerBytes);
    CHECK(calls == 0);
    ownerBytes[0x2B8] = 0;
    ownerBytes[0x2B9] = 0;
    RecreateWaterShaders(ownerBytes);
    CHECK(calls == 0);
    ownerBytes[0x2B9] = 1;
    createdState = 0;
    RecreateWaterShaders(ownerBytes);
    CHECK(calls == 0);
    puts("PASS: quality1 water reset, repeat/reset lifecycle, failed creation, quality2 and disabled-water gates");
    return 0;
}
