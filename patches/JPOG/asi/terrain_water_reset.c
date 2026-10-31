#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef void (__fastcall *TerrainCreateShaders)(void*, void*);
typedef unsigned char (__fastcall *TerrainPredicate)(void*, void*);
typedef void* (__fastcall *TerrainGetRenderer)(void*, void*);
typedef HRESULT (__stdcall *CreateVertexShader)(void*, const DWORD*, const DWORD*, DWORD*, DWORD);
typedef HRESULT (__stdcall *CreatePixelShader)(void*, const DWORD*, DWORD*);

typedef struct WaterShader {
    DWORD handleOffset;
    DWORD declarationRva;
    DWORD bytecodeRva;
} WaterShader;

static const WaterShader g_waterShaders[] = {
    { 0x74,  0x14EAD8, 0x430C0 },
    { 0x84,  0,        0x42DC0 },
    { 0x68,  0,        0x43000 },
    { 0x35C, 0x14EAE4, 0x48AC0 },
    { 0x360, 0,        0x48C00 },
    { 0x368, 0,        0x48D80 },
    { 0x36C, 0,        0x48FC0 },
    { 0x364, 0,        0x491A0 }
};

static BYTE* g_terrainModule;
static TerrainCreateShaders g_createTerrainShaders;
static HANDLE g_resetLog = INVALID_HANDLE_VALUE;

static void LogWaterShader(DWORD offset, HRESULT result, DWORD handle) {
    char line[] = "water reset: offset=00000000 hr=00000000 handle=00000000\r\n";
    static const char digits[] = "0123456789ABCDEF";
    DWORD values[3];
    int positions[3] = { 20, 32, 48 };
    DWORD written;
    int i, j;
    values[0] = offset;
    values[1] = (DWORD)result;
    values[2] = handle;
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 8; ++j)
            line[positions[i] + j] = digits[(values[i] >> (28 - j * 4)) & 15];
    if (g_resetLog != INVALID_HANDLE_VALUE)
        WriteFile(g_resetLog, line, sizeof(line) - 1, &written, NULL);
}

/* Water resources are independent of the high-end terrain rendering path. */
static void RecreateWaterShaders(void* owner) {
    void** ownerVtable = *(void***)owner;
    TerrainPredicate isCreated = *(TerrainPredicate*)(g_terrainModule + 0x38200);
    TerrainPredicate highTerrain = (TerrainPredicate)ownerVtable[0xCC / 4];
    TerrainPredicate highWater = (TerrainPredicate)ownerVtable[0xD4 / 4];
    TerrainGetRenderer getRenderer = (TerrainGetRenderer)ownerVtable[0x134 / 4];
    void* renderer;
    void* device;
    void** deviceVtable;
    unsigned int i;

    if (!isCreated(owner, NULL) || highTerrain(owner, NULL) || !highWater(owner, NULL))
        return;
    renderer = getRenderer(owner, NULL);
    device = *(void**)((BYTE*)renderer + 0x178);
    deviceVtable = *(void***)device;
    for (i = 0; i < sizeof(g_waterShaders) / sizeof(g_waterShaders[0]); ++i) {
        const WaterShader* shader = &g_waterShaders[i];
        DWORD* stored = (DWORD*)((BYTE*)owner + shader->handleOffset);
        DWORD created = 0xFFFFFFFF;
        HRESULT result;
        if (*stored != 0xFFFFFFFF)
            continue;
        if (shader->declarationRva) {
            CreateVertexShader create = (CreateVertexShader)deviceVtable[0x12C / 4];
            result = create(device, (const DWORD*)(g_terrainModule + shader->declarationRva),
                (const DWORD*)(g_terrainModule + shader->bytecodeRva), &created, 0);
        } else {
            CreatePixelShader create = (CreatePixelShader)deviceVtable[0x15C / 4];
            result = create(device, (const DWORD*)(g_terrainModule + shader->bytecodeRva), &created);
        }
        if (SUCCEEDED(result))
            *stored = created;
        LogWaterShader(shader->handleOffset, result, *stored);
    }
}

static void __fastcall HookTerrainCreateShaders(void* owner, void* unused) {
    (void)unused;
    g_createTerrainShaders(owner, NULL);
    RecreateWaterShaders(owner);
}

int InstallTerrainWaterResetHook(HMODULE module) {
    static const BYTE expected[5] = { 0x57, 0x56, 0x56, 0x8B, 0xF9 };
    BYTE* target = (BYTE*)module + 0xC450;
    BYTE* trampoline;
    DWORD oldProtection;
    int relative;
    int i;
    if (g_createTerrainShaders)
        return 1;
    for (i = 0; i < 5; ++i)
        if (target[i] != expected[i])
            return 0;
    trampoline = (BYTE*)VirtualAlloc(NULL, 10, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline)
        return 0;
    for (i = 0; i < 5; ++i)
        trampoline[i] = target[i];
    trampoline[5] = 0xE9;
    relative = (int)(target + 5 - (trampoline + 10));
    *(int*)(trampoline + 6) = relative;
    FlushInstructionCache(GetCurrentProcess(), trampoline, 10);
    if (!VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtection)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return 0;
    }
    g_terrainModule = (BYTE*)module;
    g_createTerrainShaders = (TerrainCreateShaders)trampoline;
    g_resetLog = CreateFileA("jpog_terrain_water_reset.log", FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    relative = (int)((BYTE*)HookTerrainCreateShaders - target - 5);
    target[0] = 0xE9;
    *(int*)(target + 1) = relative;
    FlushInstructionCache(GetCurrentProcess(), target, 5);
    VirtualProtect(target, 5, oldProtection, &oldProtection);
    return 1;
}
