from pathlib import Path
import hashlib
import json
import re
from pxr import Sdf, Usd

root = Path(r"C:\Users\User\Desktop\JPOGLatestRTX\rtx-remix\mods\JPOGRTX")
source = Path(r"C:\Users\User\Desktop\Files\JPOGRTX")
rows = []
for file in sorted(root.rglob("*")):
    if file.suffix.lower() not in (".usd", ".usda", ".usdc"):
        continue
    layer = Sdf.Layer.FindOrOpen(str(file))
    if not layer:
        raise RuntimeError(f"Cannot open {file}")
    for asset in sorted(set(re.findall(r"@([^@\r\n]*)@", layer.ExportToString())) - {""}):
        if asset.endswith(".mdl"):
            rows.append(dict(layer=str(file.relative_to(root)), asset=asset, status="runtime-mdl"))
            continue
        target = (file.parent / asset).resolve()
        contained = target.is_relative_to(root.resolve())
        rows.append(dict(layer=str(file.relative_to(root)), asset=asset,
                         status="ok" if contained and target.is_file() else "missing" if contained else "external"))
terrain = []
for asset in sorted(set(re.findall(r"@([^@]+)@", (root / "Ground.usda").read_text()))):
    if asset.endswith(".mdl"):
        continue
    file = (root / asset).resolve()
    original = source / "assets" / "ingested" / "Terrain" / file.name
    terrain.append(dict(asset=asset, exists=file.is_file(), bytes=file.stat().st_size if file.is_file() else 0,
                        matches_working_mod=file.is_file() and original.is_file() and
                        hashlib.sha256(file.read_bytes()).digest() == hashlib.sha256(original.read_bytes()).digest()))
report = dict(references=rows, terrain=terrain)
Path(__file__).with_name("release-asset-audit.json").write_text(json.dumps(report, indent=2))
print(json.dumps(dict(references=len(rows), problems=[r for r in rows if r['status'] not in ('ok', 'runtime-mdl')], terrain=terrain), indent=2))
stage = Usd.Stage.Open(str(root / "mod.usd"))
if not stage:
    raise RuntimeError("Cannot compose release stage")
print('Sublayers:', list(stage.GetRootLayer().subLayerPaths))
print('Terrain present:', bool(stage.GetPrimAtPath('/RootNode/TerrainBlend')))
