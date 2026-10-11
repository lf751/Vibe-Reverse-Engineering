from datetime import datetime
from pathlib import Path
import re
import shutil

root = Path(r"D:\Unity\dxvk-remix_numos")
backup = Path(__file__).parents[3] / "backups" / "JPOG" / (datetime.now().strftime("%Y-%m-%d_%H%M%S") + "_consolidate-terrain-variants")
paths = [p for p in (root / "src").rglob("*") if p.is_file() and p.suffix in (".cpp", ".slang", ".slangh")
         and ("terrain_pom" in p.read_text(encoding="utf-8") or "TERRAIN_BLEND_POM" in p.read_text(encoding="utf-8"))]
paths.append(root / "documentation/TerrainBlend.md")
for p in paths:
    destination = backup / p.relative_to(root)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(p, destination)

for p in paths:
    text = p.read_text(encoding="utf-8")
    if p.suffix == ".slang":
        # Keep the displacement-capable member of every terrain variant pair.
        blocks = re.split(r"(?=^//!variant )", text, flags=re.M)
        kept = []
        for block in blocks:
            first = block.splitlines()[0] if block else ""
            if first.startswith("//!variant ") and "_terrain." in first:
                continue
            kept.append(block)
        text = "".join(kept)
        text = re.sub(r"^//!> axis terrain terrain TERRAIN_BLEND\n", "", text, flags=re.M)
        text = text.replace("axis terrain terrain_pom TERRAIN_BLEND TERRAIN_BLEND_POM", "axis terrain terrain TERRAIN_BLEND")
        text = re.sub(r"^//!>\s+TERRAIN_BLEND_POM\n", "", text, flags=re.M)
        text = text.replace("_terrain_pom", "_terrain")
    elif p.suffix == ".cpp":
        text = re.sub(r"^#include <rtx_shaders/[^\n]*_terrain\.h>\n", "", text, flags=re.M)
        if p.name == "rtx_pathtracer_gbuffer.cpp":
            text = text.replace("    constexpr uint32_t kGbufferVariantTerrainPOM = 1u << 12;\n", "")
            text = text.replace("    static_assert(RTX_SHADER_VARIANT_MATRIX_GBUFFER_TERRAIN_TERRAIN_POM ==\n      RTX_SHADER_VARIANT_MATRIX_GBUFFER_RAYQUERY_TERRAIN_TERRAIN_POM);\n", "")
            text = text.replace("      return (terrain != 0u ? kGbufferVariantTerrain : 0u) |\n        (terrain == RTX_SHADER_VARIANT_MATRIX_GBUFFER_TERRAIN_TERRAIN_POM ? kGbufferVariantTerrainPOM : 0u);", "      return terrain != 0u ? kGbufferVariantTerrain : 0u;")
            text = text.replace("      return getGbufferTerrainVariantKey(terrain.enabled != 0u\n        ? (terrain.displacementEnabled != 0u && terrain.displacementStrength > 0.0f\n          ? RTX_SHADER_VARIANT_MATRIX_GBUFFER_TERRAIN_TERRAIN_POM\n          : RTX_SHADER_VARIANT_MATRIX_GBUFFER_TERRAIN_TERRAIN)\n        : 0u);", "      return getGbufferTerrainVariantKey(terrain.enabled != 0u\n        ? RTX_SHADER_VARIANT_MATRIX_GBUFFER_TERRAIN_TERRAIN : 0u);")
            pattern = r"\? \(terrainPOMEnabled\n            \? (GET_SHADER_VARIANT\([^\n]+_terrain_pom\))\n            : GET_SHADER_VARIANT\([^\n]+_terrain\)\)"
            text, count = re.subn(pattern, r"? \1", text)
            if count != 16:
                raise RuntimeError(f"Expected 16 closest-hit selections, found {count}")
            text = text.replace(", const bool terrainPOMEnabled)", ")")
            text = text.replace("    const bool terrainPOMEnabled = terrain.displacementEnabled != 0u && terrain.displacementStrength > 0.0f;\n", "")
            text = text.replace("getGbufferClosestHitShader(closestHitKey, terrainEnabled, terrainPOMEnabled)", "getGbufferClosestHitShader(closestHitKey, terrainEnabled)")
        else:
            pattern = r"\? \(m_device->getCommon\(\)->getSceneManager\(\).getTerrainBlendArgs\(\).displacementEnabled != 0 \\\n      && m_device->getCommon\(\)->getSceneManager\(\).getTerrainBlendArgs\(\).displacementStrength > 0.0f \\\n      \? (GET_SHADER_VARIANT\(stage, shaderClass, code##_terrain_pom\)) \\\n      : GET_SHADER_VARIANT\(stage, shaderClass, code##_terrain\)\)"
            text, count = re.subn(pattern, r"? \1", text)
            if count != 1:
                raise RuntimeError("Indirect terrain selection did not match")
        text = text.replace("_terrain_pom", "_terrain")
    elif p.suffix == ".slangh":
        text = text.replace("TERRAIN_BLEND_POM", "TERRAIN_BLEND")
        if p.name == "integrator_indirect.slangh":
            text = text.replace("if (pathState.continuePath && cb.terrainBlendArgs.displacementSelfShadowEnabled != 0 &&", "if (pathState.continuePath && cb.terrainBlendArgs.enabled != 0 &&\n        cb.terrainBlendArgs.displacementEnabled != 0 && cb.terrainBlendArgs.displacementStrength > 0.f &&\n        cb.terrainBlendArgs.displacementSelfShadowEnabled != 0 &&")
    else:
        text += "\n## Terrain shader variants\n\nTerrain blending and displacement share one terrain-capable shader variant.\nThe authored displacement toggle and strength control relief at runtime; disabled\ndisplacement skips the relief intersection and terrain self-shadow sampling.\nScenes without terrain blending select the separate non-terrain variants.\n"
    if "terrain_pom" in text or "TERRAIN_BLEND_POM" in text or "terrainPOMEnabled" in text or "kGbufferVariantTerrainPOM" in text:
        raise RuntimeError(f"Unmigrated variant reference in {p}")
    p.write_text(text, encoding="utf-8")
print(f"Updated {len(paths)} files; backup: {backup}")

