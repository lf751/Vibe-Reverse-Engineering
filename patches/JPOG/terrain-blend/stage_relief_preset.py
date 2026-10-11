"""Stage the current authored terrain preset for the terrain relief runtime."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent
SOURCE = Path(r"C:\Users\User\Desktop\Files\JPOGRTX\Ground.usda")
text = SOURCE.read_text(encoding="utf-8")
match = re.search(r"custom float4 remix:terrainBlend:displacementDepths = (\([^\n]+\))", text)
if match is None:
    raise ValueError("Expected the current legacy displacement preset")
inward = match.group(1)
text = re.sub(r"^\s*custom (?:float4|token) remix:terrainBlend:displacement(?:Depths|Quality) = [^\n]+\n", "", text, flags=re.MULTILINE)
attributes = (
    f"        custom float4 remix:terrainBlend:displacementInward = {inward}\n"
    "        custom float4 remix:terrainBlend:displacementOutward = (0, 0, 1, 0)\n"
    "        custom float4 remix:terrainBlend:displacementHeightMin = (0, 0, 0, 0)\n"
    "        custom float4 remix:terrainBlend:displacementHeightMax = (1, 1, 1, 1)\n"
    "        custom int remix:terrainBlend:displacementMaxIterations = 64\n"
    "        custom bool remix:terrainBlend:displacementSelfShadowEnabled = 1\n"
    "        custom float remix:terrainBlend:displacementShadowStrength = 1\n"
    "        custom int remix:terrainBlend:displacementShadowMaxIterations = 32\n"
)
text = text.replace('    def Scope "TerrainBlend"\n    {\n', '    def Scope "TerrainBlend"\n    {\n' + attributes, 1)
target = ROOT / "relief-Ground.usda"
target.write_text(text, encoding="utf-8")
print(f"Staged {target}; original authored preset unchanged")
