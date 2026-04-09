"""
Patch d3d9_device.c:
1. Add SLOT_GetTransform = 45 to the enum
2. Replace draw_terrain_with_world_dp with enhanced version (stage 1 disable + PROJ decompose)
3. Replace draw_terrain_with_world_dip with enhanced version (full diag + stage 1 disable + PROJ decompose)
"""
import sys, os

FPATH = os.path.join(os.path.dirname(__file__), 'd3d9_device.c')

with open(FPATH, 'r') as f:
    content = f.read()

# --- 1. Add SLOT_GetTransform = 45 after SLOT_SetTransform = 44 ---
old_slot = '    SLOT_SetTransform = 44,\n'
new_slot = '    SLOT_SetTransform = 44,\n    SLOT_GetTransform = 45,\n'
if 'SLOT_GetTransform' not in content:
    content = content.replace(old_slot, new_slot)
    print("Added SLOT_GetTransform = 45")
else:
    print("SLOT_GetTransform already exists")

# --- 2. Replace draw_terrain_with_world_dp ---
DP_START = '\nstatic int draw_terrain_with_world_dp('
DP_END = '\nstatic int draw_terrain_with_world_dip('

NEW_DP = r'''
static int draw_terrain_with_world_dp(WrappedDevice *self,
    unsigned int pt, unsigned int sv, unsigned int pc)
{
    typedef int (__stdcall *FN_Draw)(void*, unsigned int, unsigned int, unsigned int);
    typedef int (__stdcall *FN_SetTransform)(void*, unsigned int, float*);
    typedef int (__stdcall *FN_SetVS)(void*, void*);
    typedef int (__stdcall *FN_SetPS)(void*, void*);
    typedef int (__stdcall *FN_SetFVF)(void*, unsigned int);
    typedef int (__stdcall *FN_SetVDecl)(void*, void*);
    typedef int (__stdcall *FN_GetVDecl)(void*, void**);
    typedef int (__stdcall *FN_SetTex)(void*, unsigned int, void*);
    typedef int (__stdcall *FN_GetTex)(void*, unsigned int, void**);
    typedef int (__stdcall *FN_SetTSS)(void*, unsigned int, unsigned int, unsigned int);
    void **vt = RealVtbl(self);
    void *savedVS   = self->currentVertexShader;
    void *savedPS   = self->currentPixelShader;
    void *savedVDecl = NULL;
    float savedProj[16];
    int hr;

    self->terrainDrawPending = 0;
    if (!g_terrainFFPFired) {
        g_terrainFFPFired = 1;
        log_str("terrain_ffp_dp\r\n");
        log_int("  hasRealView=", self->hasRealView);
        log_int("  hasProjection=", self->hasProjection);
    }

    ((FN_GetVDecl)vt[SLOT_GetVertexDeclaration])(self->pReal, &savedVDecl);
    ((FN_SetVS)vt[SLOT_SetVertexShader])(self->pReal, NULL);
    ((FN_SetPS)vt[SLOT_SetPixelShader])(self->pReal, NULL);
    ((FN_SetVDecl)vt[SLOT_SetVertexDeclaration])(self->pReal, NULL);
    ((FN_SetFVF)vt[SLOT_SetFVF])(self->pReal, TERRAIN_FVF);
    ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, (float*)s_identity);

    /* Find + bind terrain texture to stage 0, terminate at stage 1 */
    {
        void *pFoundTex = NULL;
        unsigned int ts;
        for (ts = 0; ts < 8; ts++) {
            if (g_boundTextures[ts]) { pFoundTex = g_boundTextures[ts]; break; }
        }
        if (!pFoundTex) {
            for (ts = 0; ts < 8; ts++) {
                ((FN_GetTex)vt[SLOT_GetTexture])(self->pReal, ts, &pFoundTex);
                if (pFoundTex) {
                    { typedef unsigned long (__stdcall *FN_Rel)(void*);
                      ((FN_Rel)(*(void***)pFoundTex)[2])(pFoundTex); }
                    break;
                }
            }
        }
        if (pFoundTex) {
            ((FN_SetTex)vt[SLOT_SetTexture])(self->pReal, 0, pFoundTex);
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 1, 2);  /* COLOROP = SELECTARG1 */
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 2, 2);  /* COLORARG1 = TEXTURE */
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 4, 2);  /* ALPHAOP = SELECTARG1 */
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 5, 2);  /* ALPHAARG1 = TEXTURE */
        }
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 1, 1, 1);  /* Stage 1 COLOROP = DISABLE */
    }

    /* Decompose PROJ: realP = inv(V) * VP */
    if (g_cfgTerrainDecomposeProj && self->hasRealView && self->hasProjection) {
        float realProj[16];
        memcpy(savedProj, self->projMatrix, sizeof(savedProj));
        mat4_multiply(realProj, self->invViewMatrix, self->projMatrix);
        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_PROJECTION, realProj);
    }

    hr = ((FN_Draw)vt[SLOT_DrawPrimitive])(self->pReal, pt, sv, pc);

    /* Restore PROJ */
    if (g_cfgTerrainDecomposeProj && self->hasRealView && self->hasProjection) {
        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_PROJECTION, savedProj);
    }

    ((FN_SetVDecl)vt[SLOT_SetVertexDeclaration])(self->pReal, savedVDecl);
    ((FN_SetVS)vt[SLOT_SetVertexShader])(self->pReal, savedVS);
    ((FN_SetPS)vt[SLOT_SetPixelShader])(self->pReal, savedPS);
    return hr;
}
'''

# --- 3. Replace draw_terrain_with_world_dip ---
DIP_START = '\nstatic int draw_terrain_with_world_dip('
DIP_END = '\n/* ---- Matrix helpers ---- */'

NEW_DIP = r'''
static int draw_terrain_with_world_dip(WrappedDevice *self,
    unsigned int pt, int bvi, unsigned int mi, unsigned int nv,
    unsigned int si, unsigned int pc)
{
    typedef int (__stdcall *FN_Draw)(void*, unsigned int, int, unsigned int,
        unsigned int, unsigned int, unsigned int);
    typedef int (__stdcall *FN_SetTransform)(void*, unsigned int, float*);
    typedef int (__stdcall *FN_GetTransform)(void*, unsigned int, float*);
    typedef int (__stdcall *FN_SetVS)(void*, void*);
    typedef int (__stdcall *FN_SetPS)(void*, void*);
    typedef int (__stdcall *FN_SetFVF)(void*, unsigned int);
    typedef int (__stdcall *FN_SetVDecl)(void*, void*);
    typedef int (__stdcall *FN_GetVDecl)(void*, void**);
    typedef int (__stdcall *FN_GetSS)(void*, unsigned int, void**, unsigned int*, unsigned int*);
    typedef int (__stdcall *FN_SetTex)(void*, unsigned int, void*);
    typedef int (__stdcall *FN_GetTex)(void*, unsigned int, void**);
    typedef int (__stdcall *FN_SetTSS)(void*, unsigned int, unsigned int, unsigned int);
    typedef int (__stdcall *FN_GetTSS)(void*, unsigned int, unsigned int, unsigned int*);
    void **vt = RealVtbl(self);
    void *savedVS    = self->currentVertexShader;
    void *savedPS    = self->currentPixelShader;
    void *savedVDecl = NULL;
    float savedProj[16];
    int hr;

    self->terrainDrawPending = 0;
    if (!g_terrainFFPFired) {
        void *ssVB = NULL; unsigned int ssOff = 0, ssStride = 0;
        int hrVS, hrPS, hrVD, hrFVF, hrWT;
        float deviceView[16], deviceProj[16];
        g_terrainFFPFired = 1;
        log_str("terrain_ffp_dip DIAG4\r\n");
        log_int("  hasRealView=", self->hasRealView);
        log_int("  hasProjection=", self->hasProjection);
        log_int("  decomposeProj=", g_cfgTerrainDecomposeProj);
        log_hex("  pt=", pt);
        log_int("  bvi=", bvi);
        log_hex("  nv=", nv);
        log_hex("  pc=", pc);
        log_hex("  savedVS=", (unsigned int)savedVS);
        log_hex("  savedPS=", (unsigned int)savedPS);
        ((FN_GetSS)vt[SLOT_GetStreamSource])(self->pReal, 0, &ssVB, &ssOff, &ssStride);
        log_hex("  stream0_stride=", ssStride);
        log_hex("  stream0_vb=", (unsigned int)ssVB);

        /* Query device VIEW and PROJ that Remix will see */
        ((FN_GetTransform)vt[SLOT_GetTransform])(self->pReal, D3DTS_VIEW, deviceView);
        ((FN_GetTransform)vt[SLOT_GetTransform])(self->pReal, D3DTS_PROJECTION, deviceProj);
        log_str("  == Device VIEW (GetTransform) ==\r\n");
        log_floats_dec("    row0: ", &deviceView[0], 4);
        log_floats_dec("    row1: ", &deviceView[4], 4);
        log_floats_dec("    row2: ", &deviceView[8], 4);
        log_floats_dec("    row3: ", &deviceView[12], 4);
        log_str("  == Device PROJ (GetTransform) ==\r\n");
        log_floats_dec("    row0: ", &deviceProj[0], 4);
        log_floats_dec("    row1: ", &deviceProj[4], 4);
        log_floats_dec("    row2: ", &deviceProj[8], 4);
        log_floats_dec("    row3: ", &deviceProj[12], 4);
        log_str("  == Cached VIEW ==\r\n");
        log_floats_dec("    row0: ", &self->viewMatrix[0], 4);
        log_floats_dec("    row1: ", &self->viewMatrix[4], 4);
        log_floats_dec("    row2: ", &self->viewMatrix[8], 4);
        log_floats_dec("    row3: ", &self->viewMatrix[12], 4);
        log_str("  == Cached PROJ ==\r\n");
        log_floats_dec("    row0: ", &self->projMatrix[0], 4);
        log_floats_dec("    row1: ", &self->projMatrix[4], 4);
        log_floats_dec("    row2: ", &self->projMatrix[8], 4);
        log_floats_dec("    row3: ", &self->projMatrix[12], 4);
        log_str("  == Cached invVIEW ==\r\n");
        log_floats_dec("    row0: ", &self->invViewMatrix[0], 4);
        log_floats_dec("    row1: ", &self->invViewMatrix[4], 4);
        log_floats_dec("    row2: ", &self->invViewMatrix[8], 4);
        log_floats_dec("    row3: ", &self->invViewMatrix[12], 4);

        /* TSS state for stages 0-3 */
        {
            unsigned int s;
            for (s = 0; s < 4; s++) {
                unsigned int colorop = 0, colorarg1 = 0, colorarg2 = 0;
                ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, s, 1, &colorop);
                ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, s, 2, &colorarg1);
                ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, s, 3, &colorarg2);
                log_hex("  tss_stage=", s);
                log_hex("  colorop=", colorop);
                log_hex("  colorarg1=", colorarg1);
                log_hex("  colorarg2=", colorarg2);
            }
        }

        ((FN_GetVDecl)vt[SLOT_GetVertexDeclaration])(self->pReal, &savedVDecl);
        hrVS  = ((FN_SetVS)vt[SLOT_SetVertexShader])(self->pReal, NULL);
        hrPS  = ((FN_SetPS)vt[SLOT_SetPixelShader])(self->pReal, NULL);
        hrVD  = ((FN_SetVDecl)vt[SLOT_SetVertexDeclaration])(self->pReal, NULL);
        hrFVF = ((FN_SetFVF)vt[SLOT_SetFVF])(self->pReal, TERRAIN_FVF);
        hrWT  = ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, (float*)s_identity);

        /* Find + bind terrain texture to stage 0, disable stage 1 */
        {
            void *pFoundTex = NULL;
            unsigned int ts;
            for (ts = 0; ts < 8; ts++) {
                if (g_boundTextures[ts]) { pFoundTex = g_boundTextures[ts]; break; }
            }
            if (!pFoundTex) {
                for (ts = 0; ts < 8; ts++) {
                    ((FN_GetTex)vt[SLOT_GetTexture])(self->pReal, ts, &pFoundTex);
                    if (pFoundTex) {
                        { typedef unsigned long (__stdcall *FN_Rel)(void*);
                          ((FN_Rel)(*(void***)pFoundTex)[2])(pFoundTex); }
                        break;
                    }
                }
            }
            log_hex("  foundTex=", (unsigned int)pFoundTex);
            if (pFoundTex) {
                ((FN_SetTex)vt[SLOT_SetTexture])(self->pReal, 0, pFoundTex);
                ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 1, 2);  /* COLOROP = SELECTARG1 */
                ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 2, 2);  /* COLORARG1 = TEXTURE */
                ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 4, 2);  /* ALPHAOP = SELECTARG1 */
                ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 5, 2);  /* ALPHAARG1 = TEXTURE */
                log_str("  tex0 bound for FFP\r\n");
            }
            /* Terminate FFP stage iteration at stage 1 */
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 1, 1, 1);  /* Stage 1 COLOROP = DISABLE */
        }

        /* Decompose PROJ: if VP combined, set realP = inv(V) * VP */
        if (g_cfgTerrainDecomposeProj && self->hasRealView && self->hasProjection) {
            float realProj[16];
            memcpy(savedProj, self->projMatrix, sizeof(savedProj));
            mat4_multiply(realProj, self->invViewMatrix, self->projMatrix);
            ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_PROJECTION, realProj);
            log_str("  == Decomposed PROJ (inv(V)*VP) ==\r\n");
            log_floats_dec("    row0: ", &realProj[0], 4);
            log_floats_dec("    row1: ", &realProj[4], 4);
            log_floats_dec("    row2: ", &realProj[8], 4);
            log_floats_dec("    row3: ", &realProj[12], 4);
        }

        hr = ((FN_Draw)vt[SLOT_DrawIndexedPrimitive])(self->pReal,
            pt, bvi, mi, nv, si, pc);
        log_hex("  hrDIP=", (unsigned int)hr);

        /* Restore PROJ if we decomposed */
        if (g_cfgTerrainDecomposeProj && self->hasRealView && self->hasProjection) {
            ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_PROJECTION, savedProj);
        }

        /* Verify render target is primary */
        {
            typedef int (__stdcall *FN_GetRT)(void*, unsigned int, void**);
            typedef int (__stdcall *FN_GetBB)(void*, unsigned int, unsigned int, unsigned int, void**);
            typedef int (__stdcall *FN_GetDesc)(void*, void*);
            typedef unsigned long (__stdcall *FN_SurfRelease)(void*);
            void *pRT = NULL, *pBB = NULL;
            unsigned int rtDesc[8], bbDesc[8];
            ((FN_GetRT)vt[SLOT_GetRenderTarget])(self->pReal, 0, &pRT);
            if (pRT) {
                void **rtVt = *(void***)pRT;
                ((FN_GetDesc)rtVt[12])(pRT, rtDesc);
                log_hex("  rt_w=", rtDesc[6]);
                log_hex("  rt_h=", rtDesc[7]);
                ((FN_SurfRelease)rtVt[2])(pRT);
            }
            ((FN_GetBB)vt[SLOT_GetBackBuffer])(self->pReal, 0, 0, 0, &pBB);
            if (pBB) {
                void **bbVt = *(void***)pBB;
                ((FN_GetDesc)bbVt[12])(pBB, bbDesc);
                log_int("  isPrimary=", (rtDesc[6]==bbDesc[6] && rtDesc[7]==bbDesc[7]) ? 1 : 0);
                ((FN_SurfRelease)bbVt[2])(pBB);
            }
        }
        /* Key render states */
        {
            typedef int (__stdcall *FN_GetRS)(void*, unsigned int, unsigned int*);
            unsigned int rsColorWrite = 0, rsAlphaBlend = 0, rsZWrite = 0;
            ((FN_GetRS)vt[SLOT_GetRenderState])(self->pReal, 168, &rsColorWrite);
            ((FN_GetRS)vt[SLOT_GetRenderState])(self->pReal, 27, &rsAlphaBlend);
            ((FN_GetRS)vt[SLOT_GetRenderState])(self->pReal, 14, &rsZWrite);
            log_hex("  colorWrite=", rsColorWrite);
            log_hex("  alphaBlend=", rsAlphaBlend);
            log_hex("  zWrite=", rsZWrite);
        }

        ((FN_SetVDecl)vt[SLOT_SetVertexDeclaration])(self->pReal, savedVDecl);
        ((FN_SetVS)vt[SLOT_SetVertexShader])(self->pReal, savedVS);
        ((FN_SetPS)vt[SLOT_SetPixelShader])(self->pReal, savedPS);
        return hr;
    }

    /* ---- Production path ---- */
    ((FN_GetVDecl)vt[SLOT_GetVertexDeclaration])(self->pReal, &savedVDecl);
    ((FN_SetVS)vt[SLOT_SetVertexShader])(self->pReal, NULL);
    ((FN_SetPS)vt[SLOT_SetPixelShader])(self->pReal, NULL);
    ((FN_SetVDecl)vt[SLOT_SetVertexDeclaration])(self->pReal, NULL);
    ((FN_SetFVF)vt[SLOT_SetFVF])(self->pReal, TERRAIN_FVF);
    ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, (float*)s_identity);

    /* Ensure stage 0 has a texture, terminate at stage 1 */
    {
        void *pFoundTex = NULL;
        unsigned int ts;
        for (ts = 0; ts < 8; ts++) {
            if (g_boundTextures[ts]) { pFoundTex = g_boundTextures[ts]; break; }
        }
        if (!pFoundTex) {
            for (ts = 0; ts < 8; ts++) {
                ((FN_GetTex)vt[SLOT_GetTexture])(self->pReal, ts, &pFoundTex);
                if (pFoundTex) {
                    { typedef unsigned long (__stdcall *FN_Rel)(void*);
                      ((FN_Rel)(*(void***)pFoundTex)[2])(pFoundTex); }
                    break;
                }
            }
        }
        if (pFoundTex) {
            ((FN_SetTex)vt[SLOT_SetTexture])(self->pReal, 0, pFoundTex);
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 1, 2);  /* COLOROP = SELECTARG1 */
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 2, 2);  /* COLORARG1 = TEXTURE */
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 4, 2);  /* ALPHAOP = SELECTARG1 */
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 5, 2);  /* ALPHAARG1 = TEXTURE */
        }
        /* Terminate FFP stage iteration at stage 1 */
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 1, 1, 1);  /* Stage 1 COLOROP = DISABLE */
    }

    /* Decompose PROJ for terrain: realP = inv(V) * VP */
    if (g_cfgTerrainDecomposeProj && self->hasRealView && self->hasProjection) {
        float realProj[16];
        memcpy(savedProj, self->projMatrix, sizeof(savedProj));
        mat4_multiply(realProj, self->invViewMatrix, self->projMatrix);
        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_PROJECTION, realProj);
    }

    hr = ((FN_Draw)vt[SLOT_DrawIndexedPrimitive])(self->pReal,
        pt, bvi, mi, nv, si, pc);

    /* Restore PROJ */
    if (g_cfgTerrainDecomposeProj && self->hasRealView && self->hasProjection) {
        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_PROJECTION, savedProj);
    }

    ((FN_SetVDecl)vt[SLOT_SetVertexDeclaration])(self->pReal, savedVDecl);
    ((FN_SetVS)vt[SLOT_SetVertexShader])(self->pReal, savedVS);
    ((FN_SetPS)vt[SLOT_SetPixelShader])(self->pReal, savedPS);
    return hr;
}
'''

# Apply replacements
# First DP: from DP_START to (just before) DIP_START
idx_dp_start = content.index(DP_START)
idx_dp_end = content.index(DP_END)  # This is also DIP_START marker
content = content[:idx_dp_start] + NEW_DP + content[idx_dp_end:]
print("Replaced draw_terrain_with_world_dp")

# Now DIP: from DIP_START to DIP_END (/* ---- Matrix helpers ---- */)
idx_dip_start = content.index(DIP_START)
idx_dip_end = content.index(DIP_END)
content = content[:idx_dip_start] + NEW_DIP + content[idx_dip_end:]
print("Replaced draw_terrain_with_world_dip")

with open(FPATH, 'w') as f:
    f.write(content)

print("Done! d3d9_device.c updated.")
