"""Visualize valid captured strata samples; never read unused coarse-LOD entries."""
import json
import argparse
import struct
import zlib
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("capture", nargs="?", type=Path, default=Path(__file__).parent / "capture" / "strata.json")
args = parser.parse_args()
root = args.capture.parent
capture = json.loads(args.capture.read_text(encoding="utf-8"))
buffers = capture["buffers"]
x0 = min(b["x"] for b in buffers) * 16
z0 = min(b["z"] for b in buffers) * 16
x1 = max(b["x"] for b in buffers) * 16 + 16
z1 = max(b["z"] for b in buffers) * 16 + 16
width, height = x1 - x0 + 1, z1 - z0 + 1
raw = bytearray([40, 20, 55] * width * height)
reconstructed = bytearray(raw)
for b in sorted(buffers, key=lambda value: -value["lod"]):
    step = 1 << b["lod"]
    for z in range(0, 17, step):
        for x in range(0, 17, step):
            value = b["bytes"][z * 17 + x]
            px, pz = b["x"] * 16 + x - x0, b["z"] * 16 + z - z0
            offset = (pz * width + px) * 3
            raw[offset:offset + 3] = bytes([value] * 3)
    for z in range(16):
        for x in range(16):
            value = b["bytes"][(z // step * step) * 17 + x // step * step]
            px, pz = b["x"] * 16 + x - x0, b["z"] * 16 + z - z0
            offset = (pz * width + px) * 3
            reconstructed[offset:offset + 3] = bytes([value] * 3)

def save_png(path, pixels):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    rows = b"".join(b"\0" + pixels[z * width * 3:(z + 1) * width * 3] for z in range(height))
    image = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    path.write_bytes(image + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))

save_png(root / "strata-valid-samples.png", raw)
save_png(root / "strata-nearest-preview.png", reconstructed)
(root / "preview-metadata.json").write_text(json.dumps({
    "originX": x0, "originZ": z0, "width": width, "height": height,
    "missingColor": [40, 20, 55],
    "rawPreview": "Valid samples only; purple marks uncaptured or unused samples",
    "reconstructedPreview": "Nearest-sample reconstruction within captured sections; not an original game texture",
}, indent=2), encoding="utf-8")
print(f"Rendered {width}x{height} captured region, origin ({x0}, {z0})")
