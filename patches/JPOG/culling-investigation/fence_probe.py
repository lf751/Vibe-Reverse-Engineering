"""Temporarily test the native wall-window distance without persisting changes."""
import argparse
import json
import threading

import frida

parser = argparse.ArgumentParser()
parser.add_argument("pid", type=int)
parser.add_argument("--distance", type=float, default=480)
parser.add_argument("--seconds", type=float, default=25)
args = parser.parse_args()
session = frida.attach(args.pid)
script = session.create_script(r"""
let calls=0;
let samples=[];
let saved=null;
const far=ptr('0x83d4cc');
Interceptor.attach(ptr('0x5ed3e0'), {
  onEnter() {
    this.original=far.readFloat();
    saved=this.original;
    far.writeFloat(DISTANCE);
    calls++;
  },
  onLeave() { far.writeFloat(this.original); saved=null; }
});
Interceptor.attach(ptr('0x5ef870'), {
  onEnter() {
    if (samples.length>=3 || !this.returnAddress.equals(ptr('0x5ed6b3'))) return;
    samples.push(Array.from({length:4},(_,i)=>this.context.esp.add(4+i*4).readFloat()));
  }
});
rpc.exports={stats(){return {calls,samples};},restore(){if(saved!==null)far.writeFloat(saved);}};
""".replace("DISTANCE", str(args.distance)))
script.on("message",lambda m,d:print(json.dumps(m),flush=True))
try:
    script.load()
    threading.Event().wait(args.seconds)
    print(json.dumps(script.exports_sync.stats()),flush=True)
finally:
    script.exports_sync.restore()
    session.detach()
