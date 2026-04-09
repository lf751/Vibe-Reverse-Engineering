"""Apply terrain WORLD matrix fix: MV * inv(VIEW) instead of Identity."""
import sys

with open('d3d9_device.c', 'r', encoding='utf-8', errors='replace') as f:
    lines = f.readlines()

out = []
i = 0
while i < len(lines):
    line = lines[i]

    # === Edit 1: DP function — add typedefs, savedAlphaBlend, MV-based WORLD, disable alpha ===
    # Replace line 220 (typedef FN_SetTSS) through line 241 (SetTransform WORLD Identity)
    # We detect the DP function block by its unique sequence
    if i > 0 and 'typedef int (__stdcall *FN_SetTSS)' in line and i < 230:
        # This is the DP function's typedef block (around line 220)
        out.append(line)  # keep FN_SetTSS
        out.append('    typedef int (__stdcall *FN_SetRS)(void*, unsigned int, unsigned int);\n')
        out.append('    typedef int (__stdcall *FN_GetRS)(void*, unsigned int, unsigned int*);\n')
        i += 1
        # Copy through to savedProj line, then add savedAlphaBlend
        while i < len(lines):
            if 'float savedProj[16]' in lines[i]:
                out.append(lines[i])
                out.append('    unsigned int savedAlphaBlend = 0;\n')
                i += 1
                break
            out.append(lines[i])
            i += 1
        # Copy through to the SetTransform WORLD identity line
        while i < len(lines):
            if 'D3DTS_WORLD' in lines[i] and 's_identity' in lines[i]:
                # Replace with MV-based WORLD
                out.append('\n')
                out.append('    /* Compute WORLD = MV * inv(VIEW) to place terrain tile correctly */\n')
                out.append('    if (self->hasRealView && mat4_isRigid(self->mvMatrix)) {\n')
                out.append('        float world[16];\n')
                out.append('        mat4_multiply(world, self->mvMatrix, self->invViewMatrix);\n')
                out.append('        world[3] = 0.0f; world[7] = 0.0f; world[11] = 0.0f; world[15] = 1.0f;\n')
                out.append('        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, world);\n')
                out.append('    } else {\n')
                out.append('        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, (float*)s_identity);\n')
                out.append('    }\n')
                out.append('\n')
                out.append('    /* Disable alpha blending for opaque terrain */\n')
                out.append('    ((FN_GetRS)vt[SLOT_GetRenderState])(self->pReal, 27, &savedAlphaBlend);\n')
                out.append('    ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, 0);\n')
                i += 1
                break
            out.append(lines[i])
            i += 1
        continue

    # === Edit 1b: DP function — restore alpha blend before restoring VS/PS ===
    # Find the restore sequence at end of DP function
    if '((FN_SetVDecl)vt[SLOT_SetVertexDeclaration])(self->pReal, savedVDecl);' in line and i < 295:
        # This is the DP function restore block (around line ~286)
        out.append('    /* Restore alpha blend */\n')
        out.append('    ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, savedAlphaBlend);\n')
        out.append('\n')
        out.append(line)
        i += 1
        continue

    # === Edit 2: DIP diagnostic — add typedefs, savedAlphaBlend ===
    # The DIP function starts around line 290 with its own typedef block
    if 'typedef int (__stdcall *FN_GetTSS)' in line:
        out.append(line)  # keep FN_GetTSS
        out.append('    typedef int (__stdcall *FN_SetRS)(void*, unsigned int, unsigned int);\n')
        out.append('    typedef int (__stdcall *FN_GetRS)(void*, unsigned int, unsigned int*);\n')
        i += 1
        # Find savedProj and add savedAlphaBlend after it
        while i < len(lines):
            if 'float savedProj[16]' in lines[i]:
                out.append(lines[i])
                out.append('    unsigned int savedAlphaBlend = 0;\n')
                i += 1
                break
            out.append(lines[i])
            i += 1
        continue

    # === Edit 3: DIP diagnostic path — replace WORLD=Identity (line ~384) ===
    if 'hrWT  = ((FN_SetTransform)' in line and 'D3DTS_WORLD' in line and 's_identity' in line:
        # Replace with MV-based WORLD + logging
        out.append('\n')
        out.append('        /* Log MV matrix and compute WORLD = MV * inv(VIEW) */\n')
        out.append('        log_str("  == Cached MV ==\\r\\n");\n')
        out.append('        log_floats_dec("    row0: ", &self->mvMatrix[0], 4);\n')
        out.append('        log_floats_dec("    row1: ", &self->mvMatrix[4], 4);\n')
        out.append('        log_floats_dec("    row2: ", &self->mvMatrix[8], 4);\n')
        out.append('        log_floats_dec("    row3: ", &self->mvMatrix[12], 4);\n')
        out.append('        if (self->hasRealView && mat4_isRigid(self->mvMatrix)) {\n')
        out.append('            float world[16];\n')
        out.append('            mat4_multiply(world, self->mvMatrix, self->invViewMatrix);\n')
        out.append('            world[3] = 0.0f; world[7] = 0.0f; world[11] = 0.0f; world[15] = 1.0f;\n')
        out.append('            hrWT = ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, world);\n')
        out.append('            log_str("  == Computed WORLD (MV*invV) ==\\r\\n");\n')
        out.append('            log_floats_dec("    row0: ", &world[0], 4);\n')
        out.append('            log_floats_dec("    row1: ", &world[4], 4);\n')
        out.append('            log_floats_dec("    row2: ", &world[8], 4);\n')
        out.append('            log_floats_dec("    row3: ", &world[12], 4);\n')
        out.append('        } else {\n')
        out.append('            hrWT = ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, (float*)s_identity);\n')
        out.append('            log_str("  WORLD=Identity (no valid MV)\\r\\n");\n')
        out.append('        }\n')
        i += 1
        continue

    # === Edit 4: DIP diagnostic — add alpha blend disable after texture setup ===
    # Find the "Terminate FFP stage iteration at stage 1" in diagnostic block
    if "/* Terminate FFP stage iteration at stage 1 */" in line and i < 420:
        out.append(line)  # keep comment
        i += 1
        out.append(lines[i])  # keep the SetTSS line
        i += 1
        # After closing brace of texture block, add alpha blend disable
        while i < len(lines):
            if lines[i].strip() == '}':
                out.append(lines[i])  # closing brace
                i += 1
                out.append('\n')
                out.append('        /* Disable alpha blending for opaque terrain */\n')
                out.append('        ((FN_GetRS)vt[SLOT_GetRenderState])(self->pReal, 27, &savedAlphaBlend);\n')
                out.append('        ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, 0);\n')
                break
            out.append(lines[i])
            i += 1
        continue

    # === Edit 5: DIP production path — replace WORLD=Identity (line ~486) ===
    if i > 480 and i < 495 and 'D3DTS_WORLD' in line and 's_identity' in line:
        out.append('\n')
        out.append('    /* Compute WORLD = MV * inv(VIEW) to place terrain tile correctly */\n')
        out.append('    if (self->hasRealView && mat4_isRigid(self->mvMatrix)) {\n')
        out.append('        float world[16];\n')
        out.append('        mat4_multiply(world, self->mvMatrix, self->invViewMatrix);\n')
        out.append('        world[3] = 0.0f; world[7] = 0.0f; world[11] = 0.0f; world[15] = 1.0f;\n')
        out.append('        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, world);\n')
        out.append('    } else {\n')
        out.append('        ((FN_SetTransform)vt[SLOT_SetTransform])(self->pReal, D3DTS_WORLD, (float*)s_identity);\n')
        out.append('    }\n')
        i += 1
        continue

    # === Edit 6: DIP production — add alpha blend disable after stage 1 COLOROP=DISABLE ===
    if i > 500 and i < 530 and "/* Stage 1 COLOROP = DISABLE */" in line:
        out.append(line)
        i += 1
        # Find closing brace of the texture block
        while i < len(lines):
            if lines[i].strip() == '}':
                out.append(lines[i])
                i += 1
                out.append('\n')
                out.append('    /* Disable alpha blending for opaque terrain */\n')
                out.append('    {\n')
                out.append('        typedef int (__stdcall *FN_SetRS)(void*, unsigned int, unsigned int);\n')
                out.append('        ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, 0);\n')
                out.append('    }\n')
                break
            out.append(lines[i])
            i += 1
        continue

    # === Edit 7: DIP diagnostic restore — add alpha blend restore before savedVDecl restore ===
    # The diagnostic restore block is the first SetVertexDeclaration restore after the render state logging
    if i > 470 and i < 485 and '((FN_SetVDecl)vt[SLOT_SetVertexDeclaration])(self->pReal, savedVDecl);' in line:
        out.append('        /* Restore alpha blend */\n')
        out.append('        ((FN_SetRS)vt[SLOT_SetRenderState])(self->pReal, 27, savedAlphaBlend);\n')
        out.append('\n')
        out.append(line)
        i += 1
        continue

    # === Edit 8: DIP production restore ===
    if i > 540 and i < 560 and '((FN_SetVDecl)vt[SLOT_SetVertexDeclaration])(self->pReal, savedVDecl);' in line:
        out.append('    /* Restore alpha blend */\n')
        out.append('    {\n')
        out.append('        typedef int (__stdcall *FN_SetRS)(void*, unsigned int, unsigned int);\n')
        out.append('        /* Note: production path doesn\'t save/restore, just forces off for each terrain draw */\n')
        out.append('    }\n')
        out.append('\n')
        out.append(line)
        i += 1
        continue

    out.append(line)
    i += 1

with open('d3d9_device.c', 'w', encoding='utf-8') as f:
    f.writelines(out)

print(f"Done. {len(lines)} -> {len(out)} lines")
