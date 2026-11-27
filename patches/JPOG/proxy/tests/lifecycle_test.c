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
    if (key->Data1 == g_vbSampleKey.Data1) r->hasSample=0;
    else if (key->Data1 == g_textureHashKey.Data1) r->hasHash=0;
    else r->hasLock=0;
    return 0;
}
static unsigned long __stdcall release_noop(void *r) { (void)r; return 0; }
static int __stdcall present_ok(void *d, void *a, void *b, void *c, void *e) { (void)d;(void)a;(void)b;(void)c;(void)e; return 0; }
static int __stdcall update_texture_ok(void *d, void *s, void *dst) { (void)d;(void)s;(void)dst; return 0; }
static int __stdcall tex_lock_ok(void *t, unsigned int l, void *lr, const void *r, unsigned int f) { (void)t;(void)l;(void)lr;(void)r;(void)f; return 0; }
static int __stdcall surf_lock_ok(void *s, void *lr, const void *r, unsigned int f) { (void)s;(void)lr;(void)r;(void)f; return 0; }
typedef struct Surface { void **vtbl; Resource *parent; } Surface;
static int __stdcall get_container(Surface *s, const GUID *iid, void **out) {
    if (iid->Data1 != g_iidIDirect3DTexture9.Data1 || !s->parent) return -1;
    *out = s->parent; return 0;
}
static float upload[3] = {100,200,300};
static int __stdcall lock(Resource *r,unsigned int o,unsigned int n,void **data,unsigned int flags) {
    (void)r;(void)o;(void)n;(void)flags;*data=upload;return 0;
}
static int __stdcall unlock(Resource *r) {(void)r;return 0;}
static int tags;
static int hash_queries;
static float last_world[16];
static int world_sets;
static int draw_count;
static int __stdcall record_transform(void *d, unsigned int state, float *m) {
    (void)d;
    if (state == D3DTS_WORLD && m) { memcpy(last_world, m, sizeof(last_world)); world_sets++; }
    return 0;
}
static int __stdcall draw_indexed_ok(void *d, unsigned int pt, int bvi, unsigned int mi,
    unsigned int nv, unsigned int si, unsigned int pc) {
    (void)d;(void)pt;(void)bvi;(void)mi;(void)nv;(void)si;(void)pc; draw_count++; return 0;
}
static int mat4_near(const float *a, const float *b) {
    int i;
    for (i = 0; i < 16; i++) {
        float diff = a[i] - b[i];
        if (diff < 0.0f) diff = -diff;
        if (diff > 1e-4f) return 0;
    }
    return 1;
}
static remixapi_ErrorCode REMIXAPI_CALL add(const char *category,const char *hash) {
    (void)hash;check(lstrcmpA(category,"session:rtx.terrainTextures")==0,"Wrong tag layer\n");tags++;return REMIXAPI_ERROR_CODE_SUCCESS;
}
static remixapi_ErrorCode REMIXAPI_CALL texture_hash(IDirect3DTexture9 *texture,uint64_t *hash) {
    hash_queries++;*hash=(unsigned int)texture;return REMIXAPI_ERROR_CODE_SUCCESS;
}
void main(void) {
    void *deviceTable[119]={0}, *shaderTable[5]={0}, *resourceTable[14]={0}, *surfaceTable[15]={0};
    void **inner=deviceTable, **shader=shaderTable;
    WrappedDevice device={0}; Resource resource={0}; Surface surface={0}; static Resource textures[200]; float sample[3], row[4]={1,0,0,0}; void *data; int i;
    unsigned __int64 h=0;
    device.pReal=&inner;deviceTable[16]=reset;deviceTable[17]=present_ok;deviceTable[31]=update_texture_ok;deviceTable[92]=set_shader;deviceTable[65]=fail_texture;
    device.hasMVP=device.hasRealView=device.hasProjection=1;device.texture0=(void*)1;
    device.vertexShader=(void*)2;device.pixelShader=(void*)3;
    check(WD_Reset(&device,NULL)==0,"Reset failed\n");
    check(!device.hasMVP&&!device.hasRealView&&!device.hasProjection&&!device.texture0&&!device.vertexShader&&!device.pixelShader,"Stale reset state\n");
    WD_SetTexture(&device,0,(void*)1);check(!device.texture0,"Failed setter changed cache\n");
    deviceTable[94]=constants;
    WD_SetVertexShaderConstantF(&device,0,row,1);
    check(!device.hasMVP,"Partial matrix was accepted after reset\n");
    for(i=1;i<4;i++)WD_SetVertexShaderConstantF(&device,i,row,1);
    check(device.hasMVP,"Complete matrix writes were not accepted\n");
    shaderTable[4]=shader_code;
    for(i=0;i<300;i++){WD_SetVertexShader(&device,&shader);check(device.vertexShader==&shader,"Shader tracking failed\n");WD_SetVertexShader(&device,NULL);}
    resource.vtbl=resourceTable;resourceTable[2]=release_noop;resourceTable[4]=put;resourceTable[5]=get;resourceTable[6]=clear;
    surface.vtbl=surfaceTable;surface.parent=&textures[0];surfaceTable[11]=get_container;
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
    for(i=0;i<200;i++){textures[i].vtbl=resourceTable;tag_terrain_texture(&textures[i]);}
    check(hash_queries==200,"Initial terrain hash queries mismatch\n");
    WD_Present(&device,NULL,NULL,NULL,NULL);
    for(i=0;i<200;i++){tag_terrain_texture(&textures[i]);}
    check(hash_queries==200,"Present invalidated steady texture hash cache\n");
    check(tags==200,"Terrain hash table overflow repeated tags\n");
    g_origTexLockRect=tex_lock_ok;g_origSurfLockRect=surf_lock_ok;
    TexLockRect_Hook(&textures[0],0,NULL,NULL,D3DLOCK_READONLY);
    check(get_texture_hash(&textures[0],&h)&&hash_queries==200,"Read-only texture lock invalidated hash\n");
    TexLockRect_Hook(&textures[0],0,NULL,NULL,0);
    check(get_texture_hash(&textures[0],&h)&&hash_queries==201,"Write texture lock failed to invalidate hash\n");
    SurfLockRect_Hook(&surface,NULL,NULL,D3DLOCK_READONLY);
    check(get_texture_hash(&textures[0],&h)&&hash_queries==201,"Read-only surface lock invalidated parent hash\n");
    SurfLockRect_Hook(&surface,NULL,NULL,0);
    check(get_texture_hash(&textures[0],&h)&&hash_queries==202,"Write surface lock failed to invalidate parent hash\n");
    WD_UpdateTexture(&device,NULL,&textures[0]);
    check(get_texture_hash(&textures[0],&h)&&hash_queries==203,"UpdateTexture failed to invalidate hash\n");
    WD_Reset(&device,NULL);
    check(get_texture_hash(&textures[0],&h)&&hash_queries==204,"Reset failed to invalidate hash epoch\n");
    {
        /* Verify scoped MV-as-WORLD decomposition for native FFP shaders. */
        float view[16] = {
            0, 0, -1, 0,
            0, 1,  0, 0,
            1, 0,  0, 0,
            100, 20, -50, 1
        };
        float expectedWorld[16] = {
            2, 0, 0, 0,
            0, 2, 0, 0,
            0, 0, 2, 0,
            42, -7, 13, 1
        };
        float mv[16];
        deviceTable[44] = record_transform;
        deviceTable[82] = draw_indexed_ok;
        mat4_multiply(mv, expectedWorld, view);
        check(WD_SetTransform(&device, D3DTS_VIEW, view) == 0 && device.hasRealView, "View capture failed\n");
        g_mvWorldTransformActive = 1;
        check(WD_SetTransform(&device, D3DTS_WORLD, mv) == 0, "Scoped MV SetTransform failed\n");
        check(mat4_near(last_world, expectedWorld), "Scoped MV SetTransform did not recover World\n");
        check(WD_DrawIndexedPrimitive(&device, 4, 0, 0, 3, 0, 1) == 0, "Scoped draw failed\n");
        check(mat4_near(last_world, expectedWorld), "Scoped draw clobbered World matrix\n");
        g_mvWorldTransformActive = 0;
        check(WD_DrawIndexedPrimitive(&device, 4, 0, 0, 3, 0, 1) == 0, "Unscoped draw failed\n");
        check(mat4_near(last_world, s_identity), "Unscoped draw did not reset World to identity\n");

        /* Verify single-draw grass skip latch requires tagged stage-0 + non-null stage-1 coverage texture. */
        draw_count = 0;
        g_grassCoverageHashes[0] = (unsigned int)&textures[1];
        g_grassCoverageHashCount = 1;
        g_grassFlushState = 1;
        device.texture0 = &textures[0];
        device.texture1 = &textures[2];
        check(WD_DrawIndexedPrimitive(&device, 4, 0, 0, 3, 0, 1) == 0, "Untagged grass draw failed\n");
        check(draw_count == 1 && g_grassFlushState == 1, "Untagged texture should not latch grass skip\n");
        device.texture0 = &textures[1];
        device.texture1 = NULL;
        check(WD_DrawIndexedPrimitive(&device, 4, 0, 0, 3, 0, 1) == 0, "No-stage1 grass draw failed\n");
        check(draw_count == 2 && g_grassFlushState == 1, "Missing stage-1 coverage texture should not latch grass skip\n");
        device.texture1 = &textures[2];
        check(WD_DrawIndexedPrimitive(&device, 4, 0, 0, 3, 0, 1) == 0, "Tagged coverage grass draw failed\n");
        check(draw_count == 3 && g_grassFlushState == 2, "Tagged coverage grass draw did not latch state to 2\n");
        check(WD_DrawIndexedPrimitive(&device, 4, 0, 0, 3, 0, 1) == 0, "Skipped grass draw failed\n");
        check(draw_count == 3, "Subsequent grass draw was not skipped\n");
        g_grassFlushState = 0;
        check(WD_DrawIndexedPrimitive(&device, 4, 0, 0, 3, 0, 1) == 0, "Post-grass draw failed\n");
        check(draw_count == 4, "Post-grass draw did not execute\n");
    }
    ExitProcess(0);
}
int _fltused = 0;
