"""
Apply terrain MVP-based WORLD fix and state save/restore.

Changes:
1. Add ensureInvProj forward declaration
2. In all 3 terrain draw paths: use WORLD = MVP * inv(P) * inv(V) instead of MV * inv(V)
3. In all 3 paths: save/restore texture stage 0, TSS stages 0+1 to fix UI glitches
4. Update diagnostic logging for the new computation
"""

import re
import sys

path = r'd:\Unity\Vibe-Reverse-Engineering_JPOG\patches\JPOG\proxy\d3d9_device.c'

with open(path, 'r', encoding='utf-8', errors='surrogateescape') as f:
    src = f.read()

lines = src.split('\n')
print(f"Original: {len(lines)} lines")

# ============================================================
# 1. Add ensureInvProj forward declaration
# ============================================================
old_fwd = "static int mat4_isRigid(const float *m);"
new_fwd = "static int mat4_isRigid(const float *m);\nstatic int ensureInvProj(WrappedDevice *self);"
assert old_fwd in src, "Could not find mat4_isRigid forward declaration"
assert "static int ensureInvProj(WrappedDevice *self);" not in src, "ensureInvProj forward decl already present"
src = src.replace(old_fwd, new_fwd, 1)
print("1. Added ensureInvProj forward declaration")

# ============================================================
# 2. Fix DP path â€” WORLD computation + state save/restore
# ============================================================

# Replace the WORLD + alpha blend + texture + draw section in DP path
# Find the DP path's WORLD computation block
dp_old_world = """    /* Compute WORLD = MV * inv(VIEW) to place terrain tile correctly */
    if (self->hasRealView && mat4_isRigid(self->mvMatrix)) {
        float world[16];
        mat4_multiply(world, self->mvMatrix, self->invViewMatrix);
        world[3] = 0.0f; world[7] = 0.0f; world[11] = 0.0f; world[15] = 1.0f;
        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, world);
    } else {
        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, (float*)s_identity);
    }

    /* Disable alpha blending for opaque terrain */
    ((FN_GetRS)vt[SLOT_GetRenderState])(self->pReal, 27, &savedAlphaBlend);
    ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, 0);

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
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 1, 2);
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 2, 2);
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 4, 2);
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 5, 2);
        }
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 1, 1, 1);  /* Stage 1 COLOROP = DISABLE */
    }"""

dp_new_world = """    /* Compute WORLD = MVP * inv(P) * inv(V) to place terrain tile correctly.
     * Terrain shaders only write c0-c3 (MVP), not c17-c20 (MV), so mvMatrix
     * is stale. Derive the model transform from the per-tile MVP instead. */
    if (self->hasMVP && self->hasRealView && ensureInvProj(self)) {
        float mv[16], world[16];
        mat4_multiply(mv, self->mvpMatrix, self->invProjMatrix);
        mat4_multiply(world, mv, self->invViewMatrix);
        world[3] = 0.0f; world[7] = 0.0f; world[11] = 0.0f; world[15] = 1.0f;
        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, world);
    } else {
        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, (float*)s_identity);
    }

    /* Save texture + TSS state before FFP setup */
    {
        void *savedTex0 = NULL;
        unsigned int sCOp0=0, sCArg0=0, sAOp0=0, sAArg0=0, sCOp1=0;
        ((FN_GetTex)vt[SLOT_GetTexture])(self->pReal, 0, &savedTex0);
        ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, 0, 1, &sCOp0);
        ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, 0, 2, &sCArg0);
        ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, 0, 4, &sAOp0);
        ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, 0, 5, &sAArg0);
        ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, 1, 1, &sCOp1);

        /* Disable alpha blending for opaque terrain */
        ((FN_GetRS)vt[SLOT_GetRenderState])(self->pReal, 27, &savedAlphaBlend);
        ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, 0);

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
                ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 1, 2);
                ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 2, 2);
                ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 4, 2);
                ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 5, 2);
            }
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 1, 1, 1);
        }"""

assert dp_old_world in src, "Could not find DP old WORLD block"
# We also need to add the restore after the draw. Find the restore section.
# The current DP restore is:
dp_old_restore = """    /* Restore alpha blend */
    ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, savedAlphaBlend);

    ((FN_SetVDecl)vt[SLOT_SetVertexDeclaration])(self->pReal, savedVDecl);
    ((FN_SetVS)vt[SLOT_SetVertexShader])(self->pReal, savedVS);
    ((FN_SetPS)vt[SLOT_SetPixelShader])(self->pReal, savedPS);
    return hr;
}

static int draw_terrain_with_world_dip"""

dp_new_restore = """    /* Restore alpha blend */
    ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, savedAlphaBlend);

        /* Restore texture + TSS state */
        ((FN_SetTex)vt[SLOT_SetTexture])(self->pReal, 0, savedTex0);
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 1, sCOp0);
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 2, sCArg0);
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 4, sAOp0);
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 5, sAArg0);
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 1, 1, sCOp1);
        if (savedTex0) {
            typedef unsigned long (__stdcall *FN_Rel)(void*);
            ((FN_Rel)(*(void***)savedTex0)[2])(savedTex0);
        }
    }

    ((FN_SetVDecl)vt[SLOT_SetVertexDeclaration])(self->pReal, savedVDecl);
    ((FN_SetVS)vt[SLOT_SetVertexShader])(self->pReal, savedVS);
    ((FN_SetPS)vt[SLOT_SetPixelShader])(self->pReal, savedPS);
    return hr;
}

static int draw_terrain_with_world_dip"""

assert dp_old_restore in src, "Could not find DP old restore block"
src = src.replace(dp_old_world, dp_new_world, 1)
src = src.replace(dp_old_restore, dp_new_restore, 1)
print("2. Fixed DP path: MVP-based WORLD + state save/restore")


# ============================================================
# 3. Fix DIP diagnostic path â€” WORLD computation + logging + state save/restore
# ============================================================

# Replace the MV log + WORLD computation section in diagnostic path
diag_old_world = """        /* Log MV matrix and compute WORLD = MV * inv(VIEW) */
        log_str("  == Cached MV ==\\r\\n");
        log_floats_dec("    row0: ", &self->mvMatrix[0], 4);
        log_floats_dec("    row1: ", &self->mvMatrix[4], 4);
        log_floats_dec("    row2: ", &self->mvMatrix[8], 4);
        log_floats_dec("    row3: ", &self->mvMatrix[12], 4);
        if (self->hasRealView && mat4_isRigid(self->mvMatrix)) {
            float world[16];
            mat4_multiply(world, self->mvMatrix, self->invViewMatrix);
            world[3] = 0.0f; world[7] = 0.0f; world[11] = 0.0f; world[15] = 1.0f;
            hrWT = ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, world);
            log_str("  == Computed WORLD (MV*invV) ==\\r\\n");
            log_floats_dec("    row0: ", &world[0], 4);
            log_floats_dec("    row1: ", &world[4], 4);
            log_floats_dec("    row2: ", &world[8], 4);
            log_floats_dec("    row3: ", &world[12], 4);
        } else {
            hrWT = ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, (float*)s_identity);
            log_str("  WORLD=Identity (no valid MV)\\r\\n");
        }"""

diag_new_world = """        /* Log MVP and compute WORLD = MVP * inv(P) * inv(V).
         * Terrain shaders only write c0-c3 (MVP), not c17-c20 (MV). */
        log_str("  == Cached MVP ==\\r\\n");
        log_floats_dec("    row0: ", &self->mvpMatrix[0], 4);
        log_floats_dec("    row1: ", &self->mvpMatrix[4], 4);
        log_floats_dec("    row2: ", &self->mvpMatrix[8], 4);
        log_floats_dec("    row3: ", &self->mvpMatrix[12], 4);
        log_int("  hasMVP=", self->hasMVP);
        if (self->hasMVP && self->hasRealView && ensureInvProj(self)) {
            float mv[16], world[16];
            mat4_multiply(mv, self->mvpMatrix, self->invProjMatrix);
            mat4_multiply(world, mv, self->invViewMatrix);
            world[3] = 0.0f; world[7] = 0.0f; world[11] = 0.0f; world[15] = 1.0f;
            hrWT = ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, world);
            log_str("  == Computed WORLD (MVP*invP*invV) ==\\r\\n");
            log_floats_dec("    row0: ", &world[0], 4);
            log_floats_dec("    row1: ", &world[4], 4);
            log_floats_dec("    row2: ", &world[8], 4);
            log_floats_dec("    row3: ", &world[12], 4);
        } else {
            hrWT = ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, (float*)s_identity);
            log_str("  WORLD=Identity (no valid MVP)\\r\\n");
        }"""

assert diag_old_world in src, "Could not find diagnostic old WORLD block"
src = src.replace(diag_old_world, diag_new_world, 1)
print("3. Fixed DIP diagnostic path: MVP-based WORLD + logging")

# Now add state save/restore around the diagnostic texture binding + draw
# Find the diagnostic texture binding start
diag_old_texbind = """        /* Find + bind terrain texture to stage 0, disable stage 1 */
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
                log_str("  tex0 bound for FFP\\r\\n");
            }
            /* Terminate FFP stage iteration at stage 1 */
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 1, 1, 1);  /* Stage 1 COLOROP = DISABLE */
        }

        /* Disable alpha blending for opaque terrain */
        ((FN_GetRS)vt[SLOT_GetRenderState])(self->pReal, 27, &savedAlphaBlend);
        ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, 0);"""

diag_new_texbind = """        /* Save texture + TSS state before FFP setup */
        {
            void *savedTex0 = NULL;
            unsigned int sCOp0=0, sCArg0=0, sAOp0=0, sAArg0=0, sCOp1=0;
            ((FN_GetTex)vt[SLOT_GetTexture])(self->pReal, 0, &savedTex0);
            ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, 0, 1, &sCOp0);
            ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, 0, 2, &sCArg0);
            ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, 0, 4, &sAOp0);
            ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, 0, 5, &sAArg0);
            ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, 1, 1, &sCOp1);
            self->terrainSavedTex0 = savedTex0;
            self->terrainSavedCOp0 = sCOp0;
            self->terrainSavedCArg0 = sCArg0;
            self->terrainSavedAOp0 = sAOp0;
            self->terrainSavedAArg0 = sAArg0;
            self->terrainSavedCOp1 = sCOp1;
        }

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
                log_str("  tex0 bound for FFP\\r\\n");
            }
            /* Terminate FFP stage iteration at stage 1 */
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 1, 1, 1);  /* Stage 1 COLOROP = DISABLE */
        }

        /* Disable alpha blending for opaque terrain */
        ((FN_GetRS)vt[SLOT_GetRenderState])(self->pReal, 27, &savedAlphaBlend);
        ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, 0);"""

assert diag_old_texbind in src, "Could not find diagnostic old texbind block"
src = src.replace(diag_old_texbind, diag_new_texbind, 1)
print("3b. Added TSS save in DIP diagnostic path")

# Find the diagnostic restore section and add TSS restore
diag_old_restore = """        /* Restore alpha blend */
        ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, savedAlphaBlend);

        ((FN_SetVDecl)vt[SLOT_SetVertexDeclaration])(self->pReal, savedVDecl);
        ((FN_SetVS)vt[SLOT_SetVertexShader])(self->pReal, savedVS);
        ((FN_SetPS)vt[SLOT_SetPixelShader])(self->pReal, savedPS);
        return hr;
    }

    /* ---- Production path ---- */"""

diag_new_restore = """        /* Restore alpha blend */
        ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, savedAlphaBlend);

        /* Restore texture + TSS state */
        ((FN_SetTex)vt[SLOT_SetTexture])(self->pReal, 0, self->terrainSavedTex0);
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 1, self->terrainSavedCOp0);
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 2, self->terrainSavedCArg0);
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 4, self->terrainSavedAOp0);
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 5, self->terrainSavedAArg0);
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 1, 1, self->terrainSavedCOp1);
        if (self->terrainSavedTex0) {
            typedef unsigned long (__stdcall *FN_Rel)(void*);
            ((FN_Rel)(*(void***)self->terrainSavedTex0)[2])(self->terrainSavedTex0);
            self->terrainSavedTex0 = NULL;
        }

        ((FN_SetVDecl)vt[SLOT_SetVertexDeclaration])(self->pReal, savedVDecl);
        ((FN_SetVS)vt[SLOT_SetVertexShader])(self->pReal, savedVS);
        ((FN_SetPS)vt[SLOT_SetPixelShader])(self->pReal, savedPS);
        return hr;
    }

    /* ---- Production path ---- */"""

assert diag_old_restore in src, "Could not find diagnostic old restore block"
src = src.replace(diag_old_restore, diag_new_restore, 1)
print("3c. Added TSS restore in DIP diagnostic path")


# ============================================================
# 4. Fix DIP production path â€” WORLD computation + state save/restore
# ============================================================

prod_old_world = """    /* Compute WORLD = MV * inv(VIEW) to place terrain tile correctly */
    if (self->hasRealView && mat4_isRigid(self->mvMatrix)) {
        float world[16];
        mat4_multiply(world, self->mvMatrix, self->invViewMatrix);
        world[3] = 0.0f; world[7] = 0.0f; world[11] = 0.0f; world[15] = 1.0f;
        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, world);
    } else {
        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, (float*)s_identity);
    }

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

    /* Disable alpha blending for opaque terrain */
    {
        typedef int (__stdcall *FN_SetRS)(void*, unsigned int, unsigned int);
        typedef int (__stdcall *FN_GetRS)(void*, unsigned int, unsigned int*);
        ((FN_GetRS)vt[SLOT_GetRenderState])(self->pReal, 27, &savedAlphaBlend);
        ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, 0);
    }"""

prod_new_world = """    /* Compute WORLD = MVP * inv(P) * inv(V) to place terrain tile correctly.
     * Terrain shaders only write c0-c3 (MVP), not c17-c20 (MV). */
    if (self->hasMVP && self->hasRealView && ensureInvProj(self)) {
        float mv[16], world[16];
        mat4_multiply(mv, self->mvpMatrix, self->invProjMatrix);
        mat4_multiply(world, mv, self->invViewMatrix);
        world[3] = 0.0f; world[7] = 0.0f; world[11] = 0.0f; world[15] = 1.0f;
        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, world);
    } else {
        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, (float*)s_identity);
    }

    /* Save texture + TSS state before FFP setup */
    {
        void *savedTex0 = NULL;
        unsigned int sCOp0=0, sCArg0=0, sAOp0=0, sAArg0=0, sCOp1=0;
        ((FN_GetTex)vt[SLOT_GetTexture])(self->pReal, 0, &savedTex0);
        ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, 0, 1, &sCOp0);
        ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, 0, 2, &sCArg0);
        ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, 0, 4, &sAOp0);
        ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, 0, 5, &sAArg0);
        ((FN_GetTSS)vt[SLOT_GetTextureStageState])(self->pReal, 1, 1, &sCOp1);

        /* Bind terrain texture to stage 0, terminate at stage 1 */
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
                ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 1, 2);
                ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 2, 2);
                ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 4, 2);
                ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 5, 2);
            }
            ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 1, 1, 1);
        }

        /* Disable alpha blending for opaque terrain */
        ((FN_GetRS)vt[SLOT_GetRenderState])(self->pReal, 27, &savedAlphaBlend);
        ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, 0);"""

assert prod_old_world in src, "Could not find production old WORLD block"
src = src.replace(prod_old_world, prod_new_world, 1)
print("4. Fixed DIP production path: MVP-based WORLD + state save")

# Fix production path restore â€” need to close the save block and add TSS restore
prod_old_restore = """    /* Restore alpha blend */
    {
        typedef int (__stdcall *FN_SetRS)(void*, unsigned int, unsigned int);
        ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, savedAlphaBlend);
    }

    ((FN_SetVDecl)vt[SLOT_SetVertexDeclaration])(self->pReal, savedVDecl);
    ((FN_SetVS)vt[SLOT_SetVertexShader])(self->pReal, savedVS);
    ((FN_SetPS)vt[SLOT_SetPixelShader])(self->pReal, savedPS);
    return hr;
}

/* ---- Matrix helpers ---- */"""

prod_new_restore = """    /* Restore alpha blend + texture + TSS state */
        ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, savedAlphaBlend);

        ((FN_SetTex)vt[SLOT_SetTexture])(self->pReal, 0, savedTex0);
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 1, sCOp0);
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 2, sCArg0);
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 4, sAOp0);
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 0, 5, sAArg0);
        ((FN_SetTSS)vt[SLOT_SetTextureStageState])(self->pReal, 1, 1, sCOp1);
        if (savedTex0) {
            typedef unsigned long (__stdcall *FN_Rel)(void*);
            ((FN_Rel)(*(void***)savedTex0)[2])(savedTex0);
        }
    }

    ((FN_SetVDecl)vt[SLOT_SetVertexDeclaration])(self->pReal, savedVDecl);
    ((FN_SetVS)vt[SLOT_SetVertexShader])(self->pReal, savedVS);
    ((FN_SetPS)vt[SLOT_SetPixelShader])(self->pReal, savedPS);
    return hr;
}

/* ---- Matrix helpers ---- */"""

assert prod_old_restore in src, "Could not find production old restore block"
src = src.replace(prod_old_restore, prod_new_restore, 1)
print("4b. Fixed DIP production path: state restore")

# ============================================================
# 5. Add terrainSaved* fields to WrappedDevice struct
# ============================================================

old_struct_end = """    unsigned int drawCallCount;
    unsigned int sceneCount;
} WrappedDevice;"""

new_struct_end = """    /* Saved state for terrain FFP draw restore */
    void *terrainSavedTex0;
    unsigned int terrainSavedCOp0;
    unsigned int terrainSavedCArg0;
    unsigned int terrainSavedAOp0;
    unsigned int terrainSavedAArg0;
    unsigned int terrainSavedCOp1;

    unsigned int drawCallCount;
    unsigned int sceneCount;
} WrappedDevice;"""

assert old_struct_end in src, "Could not find WrappedDevice struct end"
src = src.replace(old_struct_end, new_struct_end, 1)
print("5. Added terrainSaved* fields to WrappedDevice struct")


# ============================================================
# Write output
# ============================================================

with open(path, 'w', encoding='utf-8', errors='surrogateescape') as f:
    f.write(src)

new_lines = src.split('\n')
print(f"Final: {len(new_lines)} lines (was {len(lines)})")
print("All changes applied successfully.")

