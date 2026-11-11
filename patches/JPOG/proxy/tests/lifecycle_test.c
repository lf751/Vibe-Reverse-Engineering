#include "../d3d9_device.c"
void log_str(const char *s) { (void)s; }
void log_hex(const char *s, unsigned int v) { (void)s; (void)v; }
FARPROC Proxy_GetChainExport(const char *s) { (void)s; return NULL; }
int Proxy_IsWrapperInterface(const GUID *g, int d) { (void)g; (void)d; return 0; }
static void check(int value, const char *message) {
    DWORD count;
    if (!value) { WriteFile(GetStdHandle(STD_ERROR_HANDLE), message, lstrlenA(message), &count, NULL); ExitProcess(1); }
}
static int __stdcall reset(void *d, void *p) { (void)d; (void)p; return 0; }
static int __stdcall constants(void *d,unsigned int r,float *p,unsigned int n){(void)d;(void)r;(void)p;(void)n;return 0;}
static int __stdcall set_shader(void *d, void *p) { (void)d; (void)p; return 0; }
static int __stdcall fail_texture(void *d, unsigned int s, void *p) { (void)d; (void)s; (void)p; return -1; }
static int __stdcall shader_code(void *s, void *data, unsigned int *size) {
    unsigned int code[2] = {0xfffe0101, 0xffff};
    (void)s; if (data) memcpy(data, code, sizeof(code)); *size = sizeof(code); return 0;
}
typedef struct Resource { void **vtbl; void *lock; float sample[3]; int hasLock, hasSample, hasHash; TextureHashCache hash; } Resource;
static int __stdcall put(Resource *r, const GUID *key, const void *data, unsigned int size, unsigned int flags) {
    (void)flags;
    if (key->Data1 == g_vbSampleKey.Data1) { memcpy(r->sample, data, size); r->hasSample = 1; }
    else if (key->Data1 == g_textureHashKey.Data1) { memcpy(&r->hash,data,size);r->hasHash=1; }
    else { memcpy(&r->lock, data, size); r->hasLock = 1; } return 0;
}
static int __stdcall get(Resource *r, const GUID *key, void *data, unsigned int *size) {
    if (key->Data1 == g_vbSampleKey.Data1) { if (!r->hasSample) return -1; memcpy(data,r->sample,*size); }
    else if (key->Data1 == g_textureHashKey.Data1) { if(!r->hasHash)return -1;memcpy(data,&r->hash,*size); }
    else { if (!r->hasLock) return -1; memcpy(data,&r->lock,*size); } return 0;
}
static int __stdcall clear(Resource *r, const GUID *key) {
    if (key->Data1 == g_vbSampleKey.Data1) r->hasSample=0; else r->hasLock=0; return 0;
}
static float upload[3] = {100,200,300};
static int __stdcall lock(Resource *r,unsigned int o,unsigned int n,void **data,unsigned int flags) {
    (void)r;(void)o;(void)n;(void)flags;*data=upload;return 0;
}
static int __stdcall unlock(Resource *r) {(void)r;return 0;}
static int tags;
static remixapi_ErrorCode REMIXAPI_CALL add(const char *category,const char *hash) {
    (void)hash;check(lstrcmpA(category,"session:rtx.terrainTextures")==0,"Wrong tag layer\n");tags++;return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL texture_hash(IDirect3DTexture9 *texture,uint64_t *hash) {
    *hash=(unsigned int)texture;return REMIXAPI_ERROR_CODE_SUCCESS;
}
void main(void) {
    void *deviceTable[119]={0}, *shaderTable[5]={0}, *resourceTable[14]={0};
    void **inner=deviceTable, **shader=shaderTable;
    WrappedDevice device={0}; Resource resource={0}; static Resource textures[200]; float sample[3], row[4]={1,0,0,0}; void *data; int i;
    device.pReal=&inner;deviceTable[16]=reset;deviceTable[92]=set_shader;deviceTable[65]=fail_texture;
    device.hasMVP=device.hasRealView=device.hasProjection=1;device.texture0=(void*)1;
    device.vertexShader=(void*)2;device.pixelShader=(void*)3;device.currentVertexShaderHash=9;
    check(WD_Reset(&device,NULL)==0,"Reset failed\n");
    check(!device.hasMVP&&!device.hasRealView&&!device.hasProjection&&!device.texture0&&!device.vertexShader&&!device.pixelShader&&!device.currentVertexShaderHash,"Stale reset state\n");
    WD_SetTexture(&device,0,(void*)1);check(!device.texture0,"Failed setter changed cache\n");
    deviceTable[94]=constants;
    WD_SetVertexShaderConstantF(&device,0,row,1);
    check(!device.hasMVP,"Partial matrix was accepted after reset\n");
    for(i=1;i<4;i++)WD_SetVertexShaderConstantF(&device,i,row,1);
    check(device.hasMVP,"Complete matrix writes were not accepted\n");
    shaderTable[4]=shader_code;
    for(i=0;i<300;i++){WD_SetVertexShader(&device,&shader);check(device.currentVertexShaderHash!=0,"Shader tracking exhausted\n");WD_SetVertexShader(&device,NULL);}
    resource.vtbl=resourceTable;resourceTable[4]=put;resourceTable[5]=get;resourceTable[6]=clear;
    g_origVBLock=lock;g_origVBUnlock=unlock;
    for(i=0;i<5000;i++){
        resource.hasSample=resource.hasLock=0;
        check(!resource_get_data(&resource,&g_vbSampleKey,sample,sizeof(sample)),"Reused buffer inherited sample\n");
        VBLock_Hook(&resource,0,12,&data,0);VBUnlock_Hook(&resource);
        check(resource_get_data(&resource,&g_vbSampleKey,sample,sizeof(sample))&&sample[0]==100,"Buffer sample lost\n");
    }
    VBLock_Hook(&resource,0,4,&data,0);VBUnlock_Hook(&resource);
    check(!resource.hasSample,"Short upload kept invalid sample\n");
    g_remix.AddTextureHash=add;g_remix.dxvk_GetTextureHash=texture_hash;
    for(i=0;i<200;i++){textures[i].vtbl=resourceTable;g_textureHashFrame++;tag_terrain_texture(&textures[i]);}
    for(i=0;i<200;i++){textures[i].vtbl=resourceTable;g_textureHashFrame++;tag_terrain_texture(&textures[i]);}
    check(tags==200,"Terrain hash table overflow repeated tags\n");
    ExitProcess(0);
}
int _fltused = 0;
