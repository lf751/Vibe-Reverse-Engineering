"""Read native terrain section resource counts without changing game state."""

import argparse
import json

import frida


parser = argparse.ArgumentParser()
parser.add_argument("pid", type=int)
parser.add_argument("terrain")
args = parser.parse_args()
session = frida.attach(args.pid)
script = session.create_script(r"""
rpc.exports = {
  snapshot(address) {
    const terrain = ptr(address);
    const heightfield = terrain.add(0x28).readPointer();
    const nx = heightfield.add(0x2c).readU32(), nz = heightfield.add(0x30).readU32();
    const sections = terrain.add(0x30).readPointer();
    const info = {nx, nz, sections: nx*nz, withGrass: 0, withStrata: 0, withShadow: 0};
    if (nx*nz > 65536) throw new Error('Unexpected section dimensions');
    for (let i=0; i<nx*nz; i++) {
      const s = sections.add(i*0x50);
      if (!s.add(0x3c).readPointer().isNull()) info.withGrass++;
      if (!s.add(0x24).readPointer().isNull()) info.withStrata++;
      if (!s.add(0x28).readPointer().isNull()) info.withShadow++;
    }
    return info;
  }
};
""")
try:
    script.load()
    print(json.dumps(script.exports_sync.snapshot(args.terrain), indent=2))
finally:
    session.detach()
