"""Set the isolated game's daylight through the public Remix bridge API."""
import argparse
import frida

parser = argparse.ArgumentParser()
parser.add_argument("pid", type=int)
parser.add_argument("cycle", choices=("True", "False"))
args = parser.parse_args()
session = frida.attach(args.pid)
script = session.create_script("""
rpc.exports = {
  setCycle(value) {
    if (Process.pointerSize !== 4) throw new Error('Expected the 32-bit game client');
    const module = Process.getModuleByName('d3d9_remix.dll');
    const initialize = new NativeFunction(module.getExportByName('remixapi_InitializeLibrary'),
      'int', ['pointer', 'pointer'], 'stdcall');
    const info = Memory.alloc(16);
    info.writeU32(1);
    info.add(4).writePointer(ptr(0));
    info.add(8).writeU64(65536001);
    const api = Memory.alloc(4096);
    const result = initialize(info, api);
    if (result !== 0) throw new Error('API initialization failed: ' + result);
    const address = api.add(12 * Process.pointerSize).readPointer();
    if (address.isNull()) throw new Error('SetConfigVariable is unavailable');
    const setConfig = new NativeFunction(address, 'int', ['pointer', 'pointer'], 'stdcall');
    return setConfig(Memory.allocUtf8String('rtx.atmosphere.timeCycleEnable'), Memory.allocUtf8String(value));
  }
};
""")
try:
    script.load()
    result = script.exports_sync.set_cycle(args.cycle)
    if result != 0:
        raise RuntimeError(f"SetConfigVariable failed: {result}")
    print(f"Test day/night cycle set to {args.cycle}")
finally:
    session.detach()
