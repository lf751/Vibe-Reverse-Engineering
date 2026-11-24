"""
Builds the 5 alpha-free Maxtree Vol 154 palm replacements for JPOG test-game:
  1. Runs Blender 4.2 headlessly to clean/trim zero-alpha faces, decimate, scale to JPOG meters (Z-up),
     and export per-material USDA meshes + 3ds Max FBX copies.
  2. Converts every referenced Maxtree Vol 154 material texture (Albedo with edge-bleed on Leaf so
     zero-alpha tips never show black, Octahedral Normal, Roughness from Glossiness, Translucency)
     into mipmapped RTX Remix .rtex.dds via nvtt_export.
  3. Writes test-game/rtx-remix/mods/JPOG_TestPalms/mod.usda overriding ONLY the 5 palm mesh hashes
     in test-game (leaving C:\\Users\\User\\Desktop\\Files\\JPOGRTX untouched!).
"""
import glob
import json
import os
import shutil
import subprocess
import numpy as np
from PIL import Image, ImageFilter

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
V154_ROOT = r"G:\Downloads\Maxtree - Plant Models Vol 154 - GrowFX 1.99+Mesh, Individual"
MAPS_DIR = os.path.join(V154_ROOT, "MT_PM_V154_Maps")
BLENDER_EXE = r"C:\Program Files\Blender Foundation\Blender 4.2\blender.exe"
NVTT_EXPORT = r"C:\Program Files\NVIDIA Corporation\NVIDIA Texture Tools\nvtt_export.exe"
TEST_PALMS_MOD = r"D:\Unity\Vibe-Reverse-Engineering_JPOG_BetaTools\test-artifacts\JPOG\terrain-mask-rework\test-game\rtx-remix\mods\JPOGRTX-TestPalms"


def dilate_albedo_by_opacity(albedo_path, opacity_path, max_size=2048):
    """Bleeds valid opaque leaf pigment outward into black background so zero-alpha tips never sample black."""
    img = Image.open(albedo_path).convert("RGB")
    op_img = Image.open(opacity_path).convert("L")
    if max(img.size) > max_size:
        img = img.resize((max_size, max_size), Image.Resampling.LANCZOS)
    if op_img.size != img.size:
        op_img = op_img.resize(img.size, Image.Resampling.BILINEAR)

    arr = np.asarray(img, dtype=np.float32)
    mask = (np.asarray(op_img, dtype=np.float32) / 255.0) > 0.55

    # Fast iterative 4-neighbor dilation (24 iterations = 24px edge bleed, zero scipy dependency).
    filled = arr.copy()
    valid = mask.copy()
    # Initialize invalid pixels with mean valid leaf color as far-field fallback.
    mean_col = arr[mask].mean(axis=0) if mask.any() else np.array([70.0, 95.0, 45.0], dtype=np.float32)
    filled[~valid] = mean_col

    cur_valid = mask.copy()
    cur_rgb = arr.copy()
    cur_rgb[~cur_valid] = 0.0
    for _ in range(24):
        if cur_valid.all():
            break
        # Sum valid 4-neighbors
        n_sum = np.zeros_like(cur_rgb)
        n_cnt = np.zeros(cur_valid.shape, dtype=np.float32)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            sv = np.roll(np.roll(cur_valid, dy, axis=0), dx, axis=1)
            sr = np.roll(np.roll(cur_rgb, dy, axis=0), dx, axis=1)
            n_sum += sr
            n_cnt += sv.astype(np.float32)
        newly = (~cur_valid) & (n_cnt > 0.0)
        if not newly.any():
            break
        cur_rgb[newly] = n_sum[newly] / n_cnt[newly, None]
        cur_valid[newly] = True

    out = np.where(cur_valid[:, :, None], cur_rgb, filled)
    return Image.fromarray(np.clip(np.round(out), 0, 255).astype(np.uint8), mode="RGB")


def encode_remix_octahedral_normal(normal_path, out_png, max_size=2048):
    img = Image.open(normal_path).convert("RGB")
    if max(img.size) > max_size:
        img = img.resize((max_size, max_size), Image.Resampling.LANCZOS)
    arr = np.asarray(img, dtype=np.float32) / 255.0
    nx = arr[:, :, 0] * 2.0 - 1.0
    ny = arr[:, :, 1] * 2.0 - 1.0
    nz = np.maximum(arr[:, :, 2] * 2.0 - 1.0, 1e-4)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    nx /= length
    ny /= length
    nz /= length

    l1 = np.abs(nx) + np.abs(ny) + nz
    px = nx / l1
    py = ny / l1
    oct_x = np.clip((px + py) * 0.5 + 0.5, 0.0, 1.0)
    oct_y = np.clip((px - py) * 0.5 + 0.5, 0.0, 1.0)

    out = np.zeros((arr.shape[0], arr.shape[1], 3), dtype=np.uint8)
    out[:, :, 0] = np.round(oct_x * 255.0).astype(np.uint8)
    out[:, :, 1] = np.round(oct_y * 255.0).astype(np.uint8)
    out[:, :, 2] = 255
    Image.fromarray(out, mode="RGB").save(out_png)


def find_map(mat_name, suffix):
    hits = glob.glob(os.path.join(MAPS_DIR, f"{mat_name}_{suffix}.*"))
    return hits[0] if hits else None


def convert_material_textures(mat_names):
    tex_dir = os.path.join(OUT_DIR, "usd", "textures")
    tmp_dir = os.path.join(OUT_DIR, "_tmp_tex")
    os.makedirs(tex_dir, exist_ok=True)
    os.makedirs(tmp_dir, exist_ok=True)

    mat_info = {}
    for mname in sorted(set(mat_names)):
        is_leaf = "leaf" in mname.lower()
        max_sz = 2048 if is_leaf else 1024
        safe = mname.replace("-", "_").replace(".", "_")
        info = {"safe": safe, "is_leaf": is_leaf}

        albedo_src = find_map(mname, "albedo")
        opacity_src = find_map(mname, "opacity")
        normal_src = find_map(mname, "normal")
        gloss_src = find_map(mname, "glossiness")
        trans_src = find_map(mname, "translucency")

        if albedo_src:
            alb_png = os.path.join(tmp_dir, f"{safe}_albedo.png")
            alb_dds = os.path.join(tex_dir, f"{safe}_Albedo.a.rtex.dds")
            if opacity_src:
                img = dilate_albedo_by_opacity(albedo_src, opacity_src, max_size=max_sz)
            else:
                img = Image.open(albedo_src).convert("RGB")
                if max(img.size) > max_sz:
                    img = img.resize((max_sz, max_sz), Image.Resampling.LANCZOS)
            img.save(alb_png)
            subprocess.run(
                [NVTT_EXPORT, "--format", "bc7", "--mips", "--mip-gamma-correct", alb_png, "-o", alb_dds],
                check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
            info["albedo"] = f"./textures/{safe}_Albedo.a.rtex.dds"

        if normal_src:
            nrm_png = os.path.join(tmp_dir, f"{safe}_oct.png")
            nrm_dds = os.path.join(tex_dir, f"{safe}_OTH_Normal.n.rtex.dds")
            encode_remix_octahedral_normal(normal_src, nrm_png, max_size=max_sz)
            subprocess.run(
                [NVTT_EXPORT, "--format", "bc5", "--mips", "--no-mip-gamma-correct", nrm_png, "-o", nrm_dds],
                check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
            info["normal"] = f"./textures/{safe}_OTH_Normal.n.rtex.dds"

        if gloss_src:
            rgh_png = os.path.join(tmp_dir, f"{safe}_rough.png")
            rgh_dds = os.path.join(tex_dir, f"{safe}_Roughness.r.rtex.dds")
            g_img = Image.open(gloss_src).convert("L")
            if max(g_img.size) > max_sz:
                g_img = g_img.resize((max_sz, max_sz), Image.Resampling.LANCZOS)
            g_arr = np.asarray(g_img, dtype=np.float32) / 255.0
            r_arr = np.clip(1.0 - g_arr, 0.38 if is_leaf else 0.55, 0.94)
            Image.fromarray(np.round(r_arr * 255.0).astype(np.uint8), mode="L").save(rgh_png)
            subprocess.run(
                [NVTT_EXPORT, "--format", "bc4", "--mips", "--no-mip-gamma-correct", rgh_png, "-o", rgh_dds],
                check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
            info["roughness"] = f"./textures/{safe}_Roughness.r.rtex.dds"

        if is_leaf and trans_src:
            tr_png = os.path.join(tmp_dir, f"{safe}_trans.png")
            tr_dds = os.path.join(tex_dir, f"{safe}_Subsurface.a.rtex.dds")
            if opacity_src:
                t_img = dilate_albedo_by_opacity(trans_src, opacity_src, max_size=1024)
            else:
                t_img = Image.open(trans_src).convert("RGB").resize((1024, 1024), Image.Resampling.LANCZOS)
            t_img.save(tr_png)
            subprocess.run(
                [NVTT_EXPORT, "--format", "bc7", "--mips", "--mip-gamma-correct", tr_png, "-o", tr_dds],
                check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
            info["subsurface"] = f"./textures/{safe}_Subsurface.a.rtex.dds"

        mat_info[mname] = info
        print(f"TEX_BUILT: {safe} | leaf={is_leaf} | maps={[k for k in ('albedo','normal','roughness','subsurface') if k in info]}")

    shutil.rmtree(tmp_dir, ignore_errors=True)
    return mat_info


def write_test_mod_usda(manifest, mat_info):
    lines = [
        "#usda 1.0",
        "(",
        "    customLayerData = {",
        '        string lightspeed_game_name = "TRenderD3DInterface"',
        '        string lightspeed_layer_type = "replacement"',
        '        string lightspeed_mod_name = "JPOGRTX-TestPalms"',
        '        string lightspeed_mod_version = "1.0.0"',
        "    }",
        "    timeCodesPerSecond = 24",
        '    upAxis = "Y"',
        ")",
        "",
        'def Xform "RootNode"',
        "{",
        '    def Scope "TestPalmLooks"',
        "    {",
    ]

    for mname, info in sorted(mat_info.items()):
        safe = info["safe"]
        lines.extend([
            f'        def Material "{safe}"',
            "        {",
            f'            token outputs:mdl:surface.connect = </RootNode/TestPalmLooks/{safe}/Shader.outputs:out>',
            "",
            '            def Shader "Shader"',
            "            {",
            '                uniform token info:implementationSource = "sourceAsset"',
            '                uniform asset info:mdl:sourceAsset = @AperturePBR_Opacity.mdl@',
            '                uniform token info:mdl:sourceAsset:subIdentifier = "AperturePBR_Opacity"',
            "                int inputs:alpha_test_type = 7",
            "                bool inputs:blend_enabled = 0",
            "                color3f inputs:diffuse_color_constant = (1, 1, 1)",
        ])
        if "albedo" in info:
            lines.append(f'                asset inputs:diffuse_texture = @{info["albedo"]}@')
        lines.append("                float inputs:metallic_constant = 0")
        if "normal" in info:
            lines.append(f'                asset inputs:normalmap_texture = @{info["normal"]}@')
        lines.append("                float inputs:opacity_constant = 1")
        lines.append(f'                float inputs:reflection_roughness_constant = {0.55 if info["is_leaf"] else 0.78}')
        if "roughness" in info:
            lines.append(f'                asset inputs:reflectionroughness_texture = @{info["roughness"]}@')
        if "subsurface" in info:
            lines.extend([
                "                custom float inputs:subsurface_measurement_distance = 0.75",
                f'                custom asset inputs:subsurface_single_scattering_texture = @{info["subsurface"]}@',
                "                custom color3f inputs:subsurface_transmittance_color = (0.25, 0.35, 0.12)",
                "                custom float inputs:subsurface_volumetric_anisotropy = -0.1",
            ])
        lines.extend([
            "                bool inputs:use_legacy_alpha_state = 0",
            "                token outputs:out",
            "            }",
            "        }",
        ])

    lines.extend([
        "    }",
        "",
        '    def Scope "meshes"',
        "    {",
    ])

    for entry in manifest:
        out_name = entry["out_name"]
        mesh_hash = entry["mesh_hash"]
        old_ref = entry["old_ref"]
        lines.extend([
            f'        over "{mesh_hash}" (',
            "            references = None",
            "        )",
            "        {",
            f'            over "{old_ref}" (',
            "                active = false",
            "            )",
            "            {",
            "            }",
            "",
            f'            def Xform "ref_{out_name}" (',
            f'                prepend references = @./meshes/{out_name}.usda@',
            "            )",
            "            {",
            "                custom bool IsRemixRef = 1",
            "                double3 xformOp:rotateXYZ = (0, 0, 0)",
            "                double3 xformOp:scale = (1, 1, 1)",
            "                double3 xformOp:translate = (0, 0, 0)",
            '                uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:rotateXYZ", "xformOp:scale"]',
        ])
        for mname in entry["materials"]:
            safe = mat_info[mname]["safe"]
            lines.extend([
                f'                over "{safe}" (',
                '                    prepend apiSchemas = ["MaterialBindingAPI"]',
                "                )",
                "                {",
                f'                    rel material:binding = </RootNode/TestPalmLooks/{safe}>',
                "                }",
            ])
        lines.extend([
            "            }",
            "        }",
        ])

    lines.extend([
        "    }",
        "}",
        "",
    ])

    mod_usda = os.path.join(OUT_DIR, "usd", "mod.usda")
    with open(mod_usda, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print("WROTE:", mod_usda)


def deploy_to_test_game():
    if os.path.exists(TEST_PALMS_MOD):
        shutil.rmtree(TEST_PALMS_MOD)
    shutil.copytree(os.path.join(OUT_DIR, "usd"), TEST_PALMS_MOD)
    print("DEPLOYED_TEST_PALMS:", TEST_PALMS_MOD)


def main():
    blender_script = os.path.join(OUT_DIR, "blender_export_palms.py")
    subprocess.run(
        [BLENDER_EXE, "--factory-startup", "--background", "--python", blender_script, "--", OUT_DIR, V154_ROOT],
        check=True,
    )
    with open(os.path.join(OUT_DIR, "palms_manifest.json"), "r", encoding="utf-8") as f:
        manifest = json.load(f)
    all_mats = [m for e in manifest for m in e["materials"]]
    mat_info = convert_material_textures(all_mats)
    write_test_mod_usda(manifest, mat_info)
    print("ALL_PALM_ASSETS_BUILT")


if __name__ == "__main__":
    main()
