"""Capture the grass renderer's bound terrain texture with a read-only lock."""
import argparse
import json
import threading
from pathlib import Path

import frida

parser = argparse.ArgumentParser()
parser.add_argument("pid", type=int)
parser.add_argument("output", type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
done = threading.Event()


def receive(message, data):
    if message["type"] == "error":
        print(message)
        done.set()
        return
    metadata = message["payload"]
    (args.output / "texture.json").write_text(json.dumps(metadata, indent=2))
    if data is not None:
        (args.output / "texture.bin").write_bytes(data)
    print(json.dumps(metadata))
    done.set()


session = frida.attach(args.pid)
script = session.create_script("""
const dll = Process.getModuleByName('TTerrainShaderD3D.dll');
const address = dll.base.add(0x10d60);
let captured = false;
const hook = Interceptor.attach(address, {
  onEnter() { this.shader = this.context.ecx; },
  onLeave(texture) {
    if (captured || texture.isNull()) return;
    captured = true;
    const table = texture.readPointer();
    const desc = Memory.alloc(32);
    const getDesc = new NativeFunction(table.add(14*4).readPointer(), 'int', ['pointer','uint','pointer'], 'stdcall');
    const result = getDesc(texture, 0, desc);
    const info = {shader:this.shader.toString(),texture:texture.toString(),getter:address.toString(),
      lowEndTexture:this.shader.add(0x204).readPointer().toString(),
      highEndTexture:this.shader.add(0x338).readPointer().toString(),
      configuredSize:this.shader.add(0x298).readU32(),descResult:result};
    if (result !== 0) {send(info);return;}
    info.descriptor = Array.from(new Uint32Array(desc.readByteArray(32)));
    const lock = new NativeFunction(table.add(16*4).readPointer(), 'int', ['pointer','uint','pointer','pointer','uint'], 'stdcall');
    const unlock = new NativeFunction(table.add(17*4).readPointer(), 'int', ['pointer','uint'], 'stdcall');
    const locked = Memory.alloc(8);
    info.lockResult = lock(texture, 0, locked, ptr(0), 0x10);
    if (info.lockResult !== 0) {send(info);return;}
    try {
      info.pitch = locked.readS32();
      const height = info.descriptor[7];
      if (height < 1 || height > 4096 || info.pitch < 1 || info.pitch > 32768) throw new Error('Unexpected surface dimensions');
      send(info, locked.add(4).readPointer().readByteArray(info.pitch*height));
    } finally {unlock(texture, 0);}
  }
});
""")
try:
    script.on("message", receive)
    script.load()
    if not done.wait(20):
        raise TimeoutError("No terrain texture getter observed in 20 seconds")
finally:
    session.detach()
