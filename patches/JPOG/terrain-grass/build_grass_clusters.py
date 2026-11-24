"""
Builds the complete editable JPOG 3D grass pack from Maxtree Vol 21:
  1. Prepares standard 3ds Max textures (fbx/textures/) + Remix .rtex.dds textures (usd/textures/)
  2. Runs Blender 4.2 headlessly to assemble Near / Mid / Far opaque grass clusters (.fbx + .usda)
  3. Writes Grass.usda (/RootNode/TerrainGrass) ready for USD sublayer inclusion and hot-reload.
"""
import os
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image

MAXTREE_ROOT = r"G:\Downloads\Maxtree Vol 21"
BLENDER_EXE = r"C:\Program Files\Blender Foundation\Blender 4.2\blender.exe"
NVTT_EXPORT = r"C:\Program Files\NVIDIA Corporation\NVIDIA Texture Tools\nvtt_export.exe"
OUT_DIR = os.path.dirname(os.path.abspath(__file__))


def encode_remix_octahedral_normal(normal_rgb_path, out_png_path, max_size=2048):
    """Encodes a tangent-space RGB normal map into Remix unsigned octahedral RG (matching packing.slangh)."""
    img = Image.open(normal_rgb_path).convert("RGB")
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
    Image.fromarray(out, mode="RGB").save(out_png_path)


def prepare_textures():
    maps_root = os.path.join(MAXTREE_ROOT, "MT_PM_V21_Maps")
    fbx_tex_dir = os.path.join(OUT_DIR, "fbx", "textures")
    usd_tex_dir = os.path.join(OUT_DIR, "usd", "textures")
    tmp_dir = os.path.join(OUT_DIR, "_tmp_tex")
    os.makedirs(fbx_tex_dir, exist_ok=True)
    os.makedirs(usd_tex_dir, exist_ok=True)
    os.makedirs(tmp_dir, exist_ok=True)

    src_albedo = os.path.join(maps_root, "MT_M_Imperata_cylindrica_Leaf_01_albedo.jpg")
    src_gloss = os.path.join(maps_root, "MT_M_Imperata_cylindrica_Leaf_01_glossiness.jpg")
    src_normal = os.path.join(maps_root, "MT_M_Imperata_cylindrica_Leaf_01_normal.jpg")

    # 3ds Max FBX textures (2K resized + tip-blended albedo so pale tips match the green base)
    from grade_grass_and_terrain_colors import grade_grass_albedo
    fbx_albedo = os.path.join(fbx_tex_dir, "Imperata_Leaf_Albedo.jpg")
    albedo_dds = os.path.join(usd_tex_dir, "Imperata_Leaf_Albedo.a.rtex.dds")
    grade_grass_albedo(src_albedo, fbx_albedo, albedo_dds)

    normal_img = Image.open(src_normal).convert("RGB")
    if max(normal_img.size) > 2048:
        normal_img = normal_img.resize((2048, 2048), Image.Resampling.LANCZOS)
    normal_img.save(os.path.join(fbx_tex_dir, "Imperata_Leaf_Normal.jpg"), quality=95)

    # Invert Maxtree glossiness -> PBR roughness (clamped to realistic foliage range [0.45, 0.92])
    gloss_img = Image.open(src_gloss).convert("L")
    if max(gloss_img.size) > 2048:
        gloss_img = gloss_img.resize((2048, 2048), Image.Resampling.LANCZOS)
    gloss = np.asarray(gloss_img, dtype=np.float32) / 255.0
    rough = np.clip(1.0 - gloss, 0.45, 0.92)
    rough_png = os.path.join(fbx_tex_dir, "Imperata_Leaf_Roughness.png")
    Image.fromarray(np.round(rough * 255.0).astype(np.uint8), mode="L").save(rough_png)

    # Encode Remix octahedral normal intermediate PNG
    oct_png = os.path.join(tmp_dir, "Imperata_Leaf_Normal_Oct.png")
    encode_remix_octahedral_normal(src_normal, oct_png, max_size=2048)

    # Convert roughness and normal to mipmapped RTX Remix .rtex.dds via nvtt_export
    rough_dds = os.path.join(usd_tex_dir, "Imperata_Leaf_Roughness.r.rtex.dds")
    normal_dds = os.path.join(usd_tex_dir, "Imperata_Leaf_Normal_OTH_Normal.n.rtex.dds")

    cmds = [
        [NVTT_EXPORT, "--format", "bc4", "--mips", "--no-mip-gamma-correct", rough_png, "-o", rough_dds],
        [NVTT_EXPORT, "--format", "bc5", "--mips", "--no-mip-gamma-correct", oct_png, "-o", normal_dds],
    ]
    for cmd in cmds:
        print("Running:", " ".join(cmd))
        subprocess.run(cmd, check=True)

    shutil.rmtree(tmp_dir, ignore_errors=True)


def write_grass_usda():
    grass_usda = os.path.join(OUT_DIR, "usd", "Grass.usda")
    content = """#usda 1.0
(
    upAxis = "Y"
)

def Xform "RootNode"
{
    def Scope "TerrainGrass"
    {
        custom int remix:terrainGrass:schemaVersion = 1
        custom bool remix:terrainGrass:enabled = 1
        custom bool remix:terrainGrass:hideVanillaGrass = 1
        custom bool remix:terrainGrass:flipUp = 1
        custom bool remix:terrainGrass:useTerrainBlendRules = 0
        custom float remix:terrainGrass:worldSize = 1024
        custom float2 remix:terrainGrass:worldOrigin = (0, 0)
        custom float remix:terrainGrass:minElevation = 0.68
        custom float remix:terrainGrass:maxSlopeDegrees = 40
        custom float remix:terrainGrass:sinkDepth = 0.08
        custom float remix:terrainGrass:normalAlign = 0.65
        custom float remix:terrainGrass:borderShrinkWidth = 6.5
        custom float remix:terrainGrass:minScale = 0.75
        custom float remix:terrainGrass:maxScale = 1.40
        custom float remix:terrainGrass:transitionWidth = 28
        custom float remix:terrainGrass:coverageThreshold = 0.75

        custom float remix:terrainGrass:nearSpacing = 3.10
        custom float remix:terrainGrass:nearRadius = 335
        custom float remix:terrainGrass:nearScale = 2.15
        custom rel remix:terrainGrass:nearPrototypes = [
            </RootNode/TerrainGrass/Prototypes/Mid_A>,
            </RootNode/TerrainGrass/Prototypes/Mid_B>,
            </RootNode/TerrainGrass/Prototypes/Mid_A2>,
            </RootNode/TerrainGrass/Prototypes/Mid_B2>,
            </RootNode/TerrainGrass/Prototypes/Mid_A3>,
            </RootNode/TerrainGrass/Prototypes/Mid_C_BrownTips>
        ]

        custom float remix:terrainGrass:midSpacing = 3.10
        custom float remix:terrainGrass:midRadius = 335
        custom float remix:terrainGrass:midScale = 2.15
        custom rel remix:terrainGrass:midPrototypes = [
            </RootNode/TerrainGrass/Prototypes/Mid_A>,
            </RootNode/TerrainGrass/Prototypes/Mid_B>,
            </RootNode/TerrainGrass/Prototypes/Mid_A2>,
            </RootNode/TerrainGrass/Prototypes/Mid_B2>,
            </RootNode/TerrainGrass/Prototypes/Mid_A3>,
            </RootNode/TerrainGrass/Prototypes/Mid_C_BrownTips>
        ]

        custom float remix:terrainGrass:farSpacing = 5.20
        custom float remix:terrainGrass:farRadius = 1024
        custom float remix:terrainGrass:farScale = 2.05
        custom rel remix:terrainGrass:farPrototypes = [
            </RootNode/TerrainGrass/Prototypes/Far_A>,
            </RootNode/TerrainGrass/Prototypes/Far_B>,
            </RootNode/TerrainGrass/Prototypes/Far_A2>,
            </RootNode/TerrainGrass/Prototypes/Far_B2>,
            </RootNode/TerrainGrass/Prototypes/Far_A3>,
            </RootNode/TerrainGrass/Prototypes/Far_C_BrownTips>
        ]

        def Material "ImperataGrassMaterial"
        {
            token outputs:mdl:surface.connect = </RootNode/TerrainGrass/ImperataGrassMaterial/Shader.outputs:out>

            def Shader "Shader"
            {
                uniform token info:implementationSource = "sourceAsset"
                uniform asset info:mdl:sourceAsset = @AperturePBR_Opacity.mdl@
                uniform token info:mdl:sourceAsset:subIdentifier = "AperturePBR_Opacity"
                int inputs:alpha_test_type = 7
                bool inputs:blend_enabled = 0
                color3f inputs:diffuse_color_constant = (1, 1, 1)
                asset inputs:diffuse_texture = @./textures/Imperata_Leaf_Albedo.a.rtex.dds@
                float inputs:metallic_constant = 0
                asset inputs:normalmap_texture = @./textures/Imperata_Leaf_Normal_OTH_Normal.n.rtex.dds@
                float inputs:opacity_constant = 1
                float inputs:reflection_roughness_constant = 0.72
                asset inputs:reflectionroughness_texture = @./textures/Imperata_Leaf_Roughness.r.rtex.dds@
                bool inputs:use_legacy_alpha_state = 0
                token outputs:out
            }
        }

        def Material "ImperataGrassMaterial_BrownTips"
        {
            token outputs:mdl:surface.connect = </RootNode/TerrainGrass/ImperataGrassMaterial_BrownTips/Shader.outputs:out>

            def Shader "Shader"
            {
                uniform token info:implementationSource = "sourceAsset"
                uniform asset info:mdl:sourceAsset = @AperturePBR_Opacity.mdl@
                uniform token info:mdl:sourceAsset:subIdentifier = "AperturePBR_Opacity"
                int inputs:alpha_test_type = 7
                bool inputs:blend_enabled = 0
                color3f inputs:diffuse_color_constant = (1, 1, 1)
                asset inputs:diffuse_texture = @./textures/Imperata_Leaf_Albedo_BrownTips.a.rtex.dds@
                float inputs:metallic_constant = 0
                asset inputs:normalmap_texture = @./textures/Imperata_Leaf_Normal_OTH_Normal.n.rtex.dds@
                float inputs:opacity_constant = 1
                float inputs:reflection_roughness_constant = 0.72
                asset inputs:reflectionroughness_texture = @./textures/Imperata_Leaf_Roughness.r.rtex.dds@
                bool inputs:use_legacy_alpha_state = 0
                token outputs:out
            }
        }

        def Scope "Prototypes" (
            prepend apiSchemas = ["MaterialBindingAPI"]
        )
        {
            rel material:binding = </RootNode/TerrainGrass/ImperataGrassMaterial>

            def Xform "Near_A" (
                prepend references = @./meshes/GrassCluster_Near_A.usda@
            )
            {
            }

            def Xform "Near_B" (
                prepend references = @./meshes/GrassCluster_Near_B.usda@
            )
            {
            }

            def Xform "Mid_A" (
                prepend references = @./meshes/GrassCluster_Mid_A.usda@
            )
            {
            }

            def Xform "Mid_B" (
                prepend references = @./meshes/GrassCluster_Mid_B.usda@
            )
            {
            }

            def Xform "Mid_A2" (
                prepend references = @./meshes/GrassCluster_Mid_A.usda@
            )
            {
            }

            def Xform "Mid_B2" (
                prepend references = @./meshes/GrassCluster_Mid_B.usda@
            )
            {
            }

            def Xform "Mid_A3" (
                prepend references = @./meshes/GrassCluster_Mid_A.usda@
            )
            {
            }

            def Xform "Mid_C_BrownTips" (
                prepend apiSchemas = ["MaterialBindingAPI"]
                prepend references = @./meshes/GrassCluster_Mid_A.usda@
            )
            {
                rel material:binding = </RootNode/TerrainGrass/ImperataGrassMaterial_BrownTips>
            }

            def Xform "Far_A" (
                prepend references = @./meshes/GrassCluster_Far_A.usda@
            )
            {
            }

            def Xform "Far_B" (
                prepend references = @./meshes/GrassCluster_Far_B.usda@
            )
            {
            }

            def Xform "Far_A2" (
                prepend references = @./meshes/GrassCluster_Far_A.usda@
            )
            {
            }

            def Xform "Far_B2" (
                prepend references = @./meshes/GrassCluster_Far_B.usda@
            )
            {
            }

            def Xform "Far_A3" (
                prepend references = @./meshes/GrassCluster_Far_A.usda@
            )
            {
            }

            def Xform "Far_C_BrownTips" (
                prepend apiSchemas = ["MaterialBindingAPI"]
                prepend references = @./meshes/GrassCluster_Far_A.usda@
            )
            {
                rel material:binding = </RootNode/TerrainGrass/ImperataGrassMaterial_BrownTips>
            }
        }
    }
}
"""
    with open(grass_usda, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"Wrote {grass_usda}")


def main():
    prepare_textures()
    blender_script = os.path.join(OUT_DIR, "blender_assemble_grass.py")
    cmd = [BLENDER_EXE, "--background", "--python", blender_script, "--", OUT_DIR, MAXTREE_ROOT]
    print("Running Blender:", " ".join(cmd))
    subprocess.run(cmd, check=True)
    write_grass_usda()
    print("Grass cluster pack ready in:", OUT_DIR)


if __name__ == "__main__":
    main()
