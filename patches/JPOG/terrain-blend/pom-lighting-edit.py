from pathlib import Path
import shutil
root=Path('D:\\Unity\\dxvk-remix_numos')
backup=Path('D:\\Unity\\Vibe-Reverse-Engineering_JPOG_BetaTools\\backups\\JPOG\\2026-10-10_000907_terrain-pom-lighting')
def edit(name,fn):
 p=root/name
 dst=backup/name
 if not dst.exists():
  dst.parent.mkdir(parents=True,exist_ok=True)
  shutil.copy2(p,dst)
 s=p.read_text(encoding="utf-8")
 result=fn(s)
 if s==result: raise RuntimeError("No change: "+name)
 p.write_text(result,encoding="utf-8")
 print(name)
