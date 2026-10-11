"""Build a version-checked native terrain pool capacity patch."""

import hashlib
from pathlib import Path

import pefile


source = Path(__file__).parent.parent / "backups/2026-10-09_124036_native-terrain-cache/TTerrainInterface.dll"
output = Path(__file__).parent / "TTerrainInterface.dll"
expected_hash = "4961df57871f539525b95c41dc72001414e4acb23569c8df3c958e155e50e3a3"
original = source.read_bytes()
assert hashlib.sha256(original).hexdigest() == expected_hash, "Unsupported original DLL"
pe = pefile.PE(data=original)
patched = bytearray(original)
changes = {0x8A12: 400, 0x8A17: 400, 0x8A1C: 400, 0x8A30: 350, 0x8A3A: 900}
allowed = set()
for rva, old_count in changes.items():
    offset = pe.get_offset_from_rva(rva)
    assert int.from_bytes(original[offset:offset + 4], "little") == old_count
    patched[offset:offset + 4] = (4096).to_bytes(4, "little")
    allowed.update(range(offset, offset + 4))
assert len(original) == len(patched)
assert all(a == b or i in allowed for i, (a, b) in enumerate(zip(original, patched)))
checked = pefile.PE(data=bytes(patched))
assert checked.FILE_HEADER.Machine == pe.FILE_HEADER.Machine
assert checked.OPTIONAL_HEADER.AddressOfEntryPoint == pe.OPTIONAL_HEADER.AddressOfEntryPoint
output.write_bytes(patched)
print(f"Original SHA256: {expected_hash}")
print(f"Patched SHA256:  {hashlib.sha256(patched).hexdigest()}")
print(f"Wrote: {output}")
print("Validated five capacity immediates; requires live validation after terrain recreation.")
