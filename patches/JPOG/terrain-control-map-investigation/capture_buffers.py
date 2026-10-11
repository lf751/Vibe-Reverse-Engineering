"""Read cached JPOG terrain strata buffers without changing game state."""
import argparse
import json
from pathlib import Path
import frida

parser = argparse.ArgumentParser()
parser.add_argument("pid", type=int)
parser.add_argument("output", type=Path)
args = parser.parse_args()
session = frida.attach(args.pid)
script = session.create_script("""
rpc.exports = {
  capture() {
    const dll = Process.getModuleByName('tterraininterface.dll');
    const terrain = dll.base.add(0x12530).readPointer();
    if (terrain.isNull()) return {ready:false, reason:'Terrain interface has not been created'};
    const height = terrain.add(0x28).readPointer();
    const sections = terrain.add(0x30).readPointer();
    if (height.isNull() || sections.isNull()) return {ready:false, reason:'Terrain sections are not initialized'};
    const nx = height.add(0x2c).readU32(), nz = height.add(0x30).readU32();
    if (nx < 1 || nz < 1 || nx > 128 || nz > 128) throw new Error('Unexpected section dimensions '+nx+'x'+nz);
    const result = {ready:true, dllBase:dll.base.toString(), terrain:terrain.toString(),
      sectionsX:nx, sectionsZ:nz, sizeX:height.add(0x38).readU32(), sizeZ:height.add(0x3c).readU32(), buffers:[], shadows:[], sections:[]};
    for (let z=0; z<nz; ++z) for (let x=0; x<nx; ++x) {
      const section = sections.add(0x50*(x+nx*z));
      result.sections.push({x:x,z:z,flags:section.add(0x18).readU32(),uiFlags:section.add(0x1c).readU32(),
        bytes:Array.from(new Uint8Array(section.readByteArray(0x50)))});
      const shadow = section.add(0x28).readPointer();
      if (!shadow.isNull()) {
        const lod = shadow.add(0xa).readU8();
        if (lod > 4) throw new Error('Unexpected shadow LOD '+lod);
        result.shadows.push({x:x,z:z,lod:lod,address:shadow.toString(),
          planeA:Array.from(new Uint8Array(shadow.add(0x10).readByteArray(289))),
          planeB:Array.from(new Uint8Array(shadow.add(0x131).readByteArray(289)))});
      }
      const buffer = section.add(0x24).readPointer();
      if (buffer.isNull()) continue;
      const lod = buffer.add(0xa).readU8();
      if (lod > 4) throw new Error('Unexpected strata LOD '+lod);
      result.buffers.push({x:x,z:z,headerX:buffer.add(8).readU8(),headerZ:buffer.add(9).readU8(),
        lod:lod,section:section.toString(),address:buffer.toString(),flags:section.add(0x18).readU32(),
        bytes:Array.from(new Uint8Array(buffer.add(0x10).readByteArray(289)))});
    }
    return result;
  }
};
""")
try:
    script.load()
    capture = script.exports_sync.capture()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(capture, indent=2), encoding="utf-8")
    if capture["ready"]:
        for buffer in capture["buffers"]:
            (args.output.parent / f"strata_{buffer['x']:02}_{buffer['z']:02}_lod{buffer['lod']}.bin").write_bytes(bytes(buffer["bytes"]))
        for buffer in capture["shadows"]:
            for plane in ("planeA", "planeB"):
                (args.output.parent / f"shadow_{plane}_{buffer['x']:02}_{buffer['z']:02}_lod{buffer['lod']}.bin").write_bytes(bytes(buffer[plane]))
        print(f"Read {len(capture['buffers'])} cached buffers across {capture['sectionsX']}x{capture['sectionsZ']} sections")
        print(f"Read {len(capture['shadows'])} cached two-plane shadow buffers")
    else:
        print(capture["reason"])
finally:
    session.detach()
