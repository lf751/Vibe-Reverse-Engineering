import argparse
import json
import threading

import frida


parser = argparse.ArgumentParser()
parser.add_argument("pid", type=int)
parser.add_argument("--bypass", action="store_true")
parser.add_argument("--depth24", action="store_true")
parser.add_argument("--near", type=float)
parser.add_argument("--far", type=float)
parser.add_argument("--seconds", type=float, default=20)
args = parser.parse_args()
session = frida.attach(args.pid)
source = r"""
const terrain = Process.getModuleByName('TTerrainInterface.dll');
const kernel = Process.getModuleByName('TKernelInterface.dll');
const render = Process.getModuleByName('TRenderD3DInterface.dll');
let count = 0;
let inspected = false;
let depthState = null;
let projectionListener = null;
let projectionCalls = 0;
const projectionOriginals = new Map();
const projectionSetterAddress = Process.getModuleByName('TRenderInterface.dll').base.add(0xa8f0);
const projectionSetter = new NativeFunction(projectionSetterAddress, 'void', ['pointer','pointer'], 'thiscall');
if (NEAR !== null || FAR !== null) {
  projectionListener = Interceptor.attach(projectionSetterAddress, {
    onEnter(a) {
      const ctx = this.context.ecx;
      const input = this.context.esp.add(4).readPointer();
      if (input.add(20).readFloat() < 1000) return;
      const original = Memory.alloc(24);
      original.writeByteArray(input.readByteArray(24));
      projectionOriginals.set(ctx.toString(), {ctx, original});
      this.copy = Memory.alloc(24);
      this.copy.writeByteArray(input.readByteArray(24));
      if (NEAR !== null) this.copy.add(16).writeFloat(NEAR);
      if (FAR !== null) this.copy.add(20).writeFloat(FAR);
      this.context.esp.add(4).writePointer(this.copy);
      projectionCalls++;
    }
  });
}
if (BYPASS) {
  Interceptor.attach(kernel.base.add(0x8210), {
    onEnter() {
      this.terrain = this.returnAddress.equals(terrain.base.add(0x6d17)) ||
                     this.returnAddress.equals(terrain.base.add(0x70be));
    },
    onLeave(retval) { if (this.terrain) { retval.replace(0); count++; } }
  });
}
Interceptor.attach(render.base.add(0x7180), {
  onEnter() {
    if (inspected) return;
    const ctx = this.context.ecx;
    const near = ctx.add(0x40).readFloat(), far = ctx.add(0x44).readFloat();
    if (far < 1000) return;
    inspected = true;
    const device = ctx.add(4).readPointer().add(0x178).readPointer();
    const vt = device.readPointer();
    const out = Memory.alloc(Process.pointerSize);
    const getDepth = new NativeFunction(vt.add(33 * 4).readPointer(), 'int', ['pointer', 'pointer'], 'stdcall');
    const hr = getDepth(device, out);
    const info = {near, far, device: device.toString(), hr};
    if (hr >= 0) {
      const surface = out.readPointer();
      const desc = Memory.alloc(32);
      const svt = surface.readPointer();
      const getDesc = new NativeFunction(svt.add(8 * 4).readPointer(), 'int', ['pointer', 'pointer'], 'stdcall');
      info.descHr = getDesc(surface, desc);
      info.desc = Array.from({length: 8}, (_, i) => desc.add(i*4).readU32());
      if (DEPTH24) {
        const replacementOut = Memory.alloc(4);
        const create = new NativeFunction(vt.add(26*4).readPointer(), 'int', ['pointer','uint','uint','uint','uint','pointer'], 'stdcall');
        info.createHr = create(device, info.desc[6], info.desc[7], 75, info.desc[5], replacementOut);
        if (info.createHr >= 0) {
          const replacement = replacementOut.readPointer();
          const colorOut = Memory.alloc(4);
          new NativeFunction(vt.add(32*4).readPointer(), 'int', ['pointer','pointer'], 'stdcall')(device,colorOut);
          const color = colorOut.readPointer();
          const set = new NativeFunction(vt.add(31*4).readPointer(), 'int', ['pointer','pointer','pointer'], 'stdcall');
          const listener = Interceptor.attach(vt.add(31*4).readPointer(), {
            onEnter(a) { if (a[0].equals(device) && a[2].equals(surface)) a[2] = replacement; }
          });
          info.setHr = set(device,color,replacement);
          depthState = {device, surface, replacement, color, set, listener};
        }
      }
      if (!depthState) new NativeFunction(svt.add(2*4).readPointer(), 'uint', ['pointer'], 'stdcall')(surface);
    }
    send(info);
  }
});
rpc.exports = {
  stats() { return {overriddenPlaneTests: count, depth24: depthState !== null, projectionCalls}; },
  restore() {
    if (projectionListener) {
      projectionListener.detach();
      for (const p of projectionOriginals.values()) projectionSetter(p.ctx,p.original);
      projectionListener = null;
    }
    if (!depthState) return;
    const d = depthState;
    d.listener.detach();
    const hr = d.set(d.device,d.color,d.surface);
    send({restoreHr:hr});
    for (const p of [d.surface,d.replacement,d.color])
      new NativeFunction(p.readPointer().add(2*4).readPointer(), 'uint', ['pointer'], 'stdcall')(p);
    depthState = null;
  }
};
""".replace("BYPASS", "true" if args.bypass else "false").replace("DEPTH24", "true" if args.depth24 else "false").replace("NEAR", json.dumps(args.near)).replace("FAR", json.dumps(args.far))
script = session.create_script(source)
script.on("message", lambda message, data: print(json.dumps(message), flush=True))
try:
    script.load()
    threading.Event().wait(args.seconds)
    print(json.dumps(script.exports_sync.stats()), flush=True)
finally:
    script.exports_sync.restore()
    session.detach()
