"""
Color-grades:
  1. Maxtree Imperata grass blade albedo so bright/dry pale tips blend naturally
     with the rich green base of the grass texture (updates both 3ds Max .jpg
     and Remix .rtex.dds, and syncs to test-game).
  2. TerrainBlend grass (varied-grass/diffuse.dds) and grassy rock
     (lower-rocky-terrain/diffuse.dds) in test-game so their green matches the
     3D grass model's olive-green hue while keeping rock faces untouched.

Untouched originals of the two terrain DDS textures are preserved under
patches/JPOG/terrain-grass/_terrain_orig_backup/ so re-grading is lossless.
"""
import os
import shutil
import subprocess
import numpy as np
from PIL import Image

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
MAXTREE_ALBEDO = r"G:\Downloads\Maxtree Vol 21\MT_PM_V21_Maps\MT_M_Imperata_cylindrica_Leaf_01_albedo.jpg"
NVTT_EXPORT = r"C:\Program Files\NVIDIA Corporation\NVIDIA Texture Tools\nvtt_export.exe"

TEST_MODS = r"D:\Unity\Vibe-Reverse-Engineering_JPOG_BetaTools\test-artifacts\JPOG\terrain-mask-rework\test-game\rtx-remix\mods"
GRASS_MOD_TEX = os.path.join(TEST_MODS, "JPOG_TerrainGrass", "textures", "Imperata_Leaf_Albedo.a.rtex.dds")
TERRAIN_GRASS_DDS = os.path.join(TEST_MODS, "JPOGRTX-Terrain-Weightmap", "varied-grass", "diffuse.dds")
TERRAIN_LOWER_ROCK_DDS = os.path.join(TEST_MODS, "JPOGRTX-Terrain-Weightmap", "lower-rocky-terrain", "diffuse.dds")

BACKUP_DIR = os.path.join(OUT_DIR, "_terrain_orig_backup")
TMP_DIR = os.path.join(OUT_DIR, "_tmp_grade")


def smoothstep(edge0, edge1, x):
    t = np.clip((x - edge0) / max(edge1 - edge0, 1e-6), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def patch_dds_bc7_srgb(dds_path):
    """Sets DXGI format byte 128 from BC7_UNORM (98) to BC7_UNORM_SRGB (99) to match original terrain DDS header."""
    with open(dds_path, "r+b") as f:
        hdr = f.read(132)
        if hdr[84:88] == b"DX10" and int.from_bytes(hdr[128:132], "little") == 98:
            f.seek(128)
            f.write((99).to_bytes(4, "little"))


def grade_grass_albedo(src_path, out_jpg, out_dds):
    img = Image.open(src_path).convert("RGB")
    if max(img.size) > 2048:
        img = img.resize((2048, 2048), Image.Resampling.LANCZOS)
    arr = np.asarray(img, dtype=np.float32) / 255.0
    h, w, _ = arr.shape

    r, g, b = arr[:, :, 0], arr[:, :, 1], arr[:, :, 2]
    lum = 0.2126 * r + 0.7152 * g + 0.0722 * b

    # Measure base color from the lower 30% of the blade texture (healthy green base).
    base_band = arr[int(h * 0.70):, :, :]
    base_green_mask = (base_band[:, :, 1] > base_band[:, :, 0] * 1.02) & (base_band[:, :, 1] > base_band[:, :, 2] * 1.15)
    base_rgb = base_band[base_green_mask].mean(axis=0) if base_green_mask.any() else base_band.mean(axis=(0, 1))
    base_lum = float(0.2126 * base_rgb[0] + 0.7152 * base_rgb[1] + 0.0722 * base_rgb[2])
    base_chroma = base_rgb / max(base_lum, 1e-4)

    # Identify bright / warm straw-colored blade tips (both by hue and vertical tip region).
    y_norm = np.linspace(0.0, 1.0, h, dtype=np.float32)[:, None]
    rg_ratio = r / np.maximum(g, 1e-4)
    warm_mask = smoothstep(0.90, 1.06, rg_ratio)
    top_mask = smoothstep(0.48, 0.0, y_norm)
    bright_mask = smoothstep(base_lum * 1.02, base_lum * 1.38, lum)

    tip_blend = np.clip(np.maximum(warm_mask * 0.88, top_mask * 0.72) + bright_mask * 0.18, 0.0, 0.90)

    # Tame pale bright tip highlights toward base luminance while keeping natural vein/rib contrast.
    tamed_lum = np.where(
        lum > base_lum,
        base_lum + (lum - base_lum) * (1.0 - 0.68 * tip_blend),
        lum,
    )
    target_pixels = np.stack([
        tamed_lum * base_chroma[0],
        tamed_lum * base_chroma[1],
        tamed_lum * base_chroma[2],
    ], axis=-1)

    graded = arr * (1.0 - tip_blend[:, :, None]) + target_pixels * tip_blend[:, :, None]
    graded = np.clip(graded, 0.0, 1.0)

    top_before = arr[:int(h * 0.25)].mean(axis=(0, 1))
    bot_before = arr[int(h * 0.75):].mean(axis=(0, 1))
    top_after = graded[:int(h * 0.25)].mean(axis=(0, 1))
    bot_after = graded[int(h * 0.75):].mean(axis=(0, 1))
    overall_after = graded.mean(axis=(0, 1))

    print(f"Grass base target RGB: {base_rgb.round(3)} (G/R={base_rgb[1]/base_rgb[0]:.2f})")
    print(f"Grass tips BEFORE: {top_before.round(3)} -> AFTER: {top_after.round(3)}")
    print(f"Grass base BEFORE: {bot_before.round(3)} -> AFTER: {bot_after.round(3)}")
    print(f"Grass overall AFTER: {overall_after.round(3)} (G/R={overall_after[1]/overall_after[0]:.2f})")

    out_img = Image.fromarray(np.round(graded * 255.0).astype(np.uint8), mode="RGB")
    out_img.save(out_jpg, quality=95)

    subprocess.run(
        [NVTT_EXPORT, "--format", "bc7", "--mips", "--mip-gamma-correct", out_jpg, "-o", out_dds],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return overall_after


def decode_mip0(dds_path, tmp_png):
    subprocess.run(
        [NVTT_EXPORT, dds_path, "-o", tmp_png],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    img = Image.open(tmp_png).convert("RGB")
    h = img.size[1]
    return img.crop((0, 0, h, h))


def encode_terrain_dds(img, tmp_png, out_dds):
    img.save(tmp_png)
    subprocess.run(
        [NVTT_EXPORT, "--format", "bc7", "--mips", "--mip-gamma-correct", tmp_png, "-o", out_dds],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    patch_dds_bc7_srgb(out_dds)


def grade_terrain_grass(orig_dds, out_dds, grass_target_rgb):
    mip0 = decode_mip0(orig_dds, os.path.join(TMP_DIR, "tgrass_dec.png"))
    arr = np.asarray(mip0, dtype=np.float32) / 255.0
    r, g, b = arr[:, :, 0], arr[:, :, 1], arr[:, :, 2]

    gmask = (g > r * 1.02) & (g > b * 1.08)
    src_green = arr[gmask].mean(axis=0, dtype=np.float64).astype(np.float32)

    # Flat sunlit terrain lacks 3D blade self-shadowing, so match the rendered in-game
    # canopy tone of the 3D grass (~0.62x darker + slightly cooler/less lime-yellow).
    target_rgb = np.array([0.205, 0.245, 0.160], dtype=np.float32)
    gain = target_rgb / np.maximum(src_green, 1e-4)

    graded = np.clip(arr * gain[None, None, :], 0.0, 1.0)

    before_mean = arr.mean(axis=(0, 1), dtype=np.float64)
    after_mean = graded.mean(axis=(0, 1), dtype=np.float64)
    after_green = graded[gmask].mean(axis=0, dtype=np.float64)
    print(
        f"Terrain grass BEFORE: {before_mean.round(3)} (G/R={src_green[1]/src_green[0]:.2f}) "
        f"-> AFTER: {after_mean.round(3)} (G/R={after_green[1]/after_green[0]:.2f})"
    )
    out_img = Image.fromarray(np.round(graded * 255.0).astype(np.uint8), mode="RGB")
    encode_terrain_dds(out_img, os.path.join(TMP_DIR, "tgrass_out.png"), out_dds)


def grade_lower_rock(orig_dds, out_dds, dirt_dds=None):
    if dirt_dds is None:
        dirt_dds = os.path.join(TEST_MODS, "JPOGRTX-Terrain-Weightmap", "rocky-dirt", "diffuse.dds")
    dirt_mip0 = decode_mip0(dirt_dds, os.path.join(TMP_DIR, "dirt_dec.png"))
    dirt_arr = np.asarray(dirt_mip0, dtype=np.float32) / 255.0
    dirt_mean = dirt_arr.mean(axis=(0, 1), dtype=np.float64).astype(np.float32)
    dirt_lum = float(0.2126 * dirt_mean[0] + 0.7152 * dirt_mean[1] + 0.0722 * dirt_mean[2])
    dirt_chroma = dirt_mean / max(dirt_lum, 1e-4)

    mip0 = decode_mip0(orig_dds, os.path.join(TMP_DIR, "lrock_dec.png"))
    arr = np.asarray(mip0, dtype=np.float32) / 255.0
    r, g, b = arr[:, :, 0], arr[:, :, 1], arr[:, :, 2]
    lum = 0.2126 * r + 0.7152 * g + 0.0722 * b

    gmask = (g > r * 1.02) & (g > b * 1.08)
    src_green = arr[gmask].mean(axis=0, dtype=np.float64).astype(np.float32)
    src_green_lum = float(0.2126 * src_green[0] + 0.7152 * src_green[1] + 0.0722 * src_green[2])

    # Match the green pockets in lowerRock to rocky-dirt brown while leaving grey rock faces untouched.
    lum_ratio = dirt_lum / max(src_green_lum, 1e-4)
    target_pixels = np.stack([
        lum * lum_ratio * dirt_chroma[0],
        lum * lum_ratio * dirt_chroma[1],
        lum * lum_ratio * dirt_chroma[2],
    ], axis=-1)

    g_weight = smoothstep(0.000, 0.040, g - r * 0.96) * smoothstep(0.012, 0.065, g - b)
    graded = np.clip(arr * (1.0 - g_weight[:, :, None]) + target_pixels * g_weight[:, :, None], 0.0, 1.0)

    after_green = graded[gmask].mean(axis=0, dtype=np.float64)
    before_rock = arr[~gmask].mean(axis=0, dtype=np.float64)
    after_rock = graded[~gmask].mean(axis=0, dtype=np.float64)
    print(f"LowerRock grassy pockets BEFORE: {src_green.round(3)} -> AFTER: {after_green.round(3)} (dirt={dirt_mean.round(3)})")
    print(f"LowerRock rock faces     BEFORE: {before_rock.round(3)} -> AFTER: {after_rock.round(3)}")
    out_img = Image.fromarray(np.round(graded * 255.0).astype(np.uint8), mode="RGB")
    encode_terrain_dds(out_img, os.path.join(TMP_DIR, "lrock_out.png"), out_dds)


def main():
    os.makedirs(BACKUP_DIR, exist_ok=True)
    os.makedirs(TMP_DIR, exist_ok=True)

    # Back up pristine originals once so repeated tweaks never double-grade.
    tgrass_orig = os.path.join(BACKUP_DIR, "varied-grass_diffuse.dds")
    lrock_orig = os.path.join(BACKUP_DIR, "lower-rocky-terrain_diffuse.dds")
    if not os.path.exists(tgrass_orig):
        shutil.copy2(TERRAIN_GRASS_DDS, tgrass_orig)
    if not os.path.exists(lrock_orig):
        shutil.copy2(TERRAIN_LOWER_ROCK_DDS, lrock_orig)

    fbx_albedo = os.path.join(OUT_DIR, "fbx", "textures", "Imperata_Leaf_Albedo.jpg")
    usd_albedo = os.path.join(OUT_DIR, "usd", "textures", "Imperata_Leaf_Albedo.a.rtex.dds")
    grass_target_rgb = grade_grass_albedo(MAXTREE_ALBEDO, fbx_albedo, usd_albedo)
    shutil.copy2(usd_albedo, GRASS_MOD_TEX)

    grade_terrain_grass(tgrass_orig, TERRAIN_GRASS_DDS, grass_target_rgb)
    grade_lower_rock(lrock_orig, TERRAIN_LOWER_ROCK_DDS)

    shutil.rmtree(TMP_DIR, ignore_errors=True)
    print("ALL_COLORS_GRADED_OK")


if __name__ == "__main__":
    main()
