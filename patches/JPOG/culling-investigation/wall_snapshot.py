"""Read wall-manager state externally without injecting into JPOG."""

import argparse
import ctypes
import json
import struct
from ctypes import wintypes

parser = argparse.ArgumentParser()
parser.add_argument("pid", type=int)
args = parser.parse_args()
kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel.OpenProcess.restype = wintypes.HANDLE
kernel.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
kernel.ReadProcessMemory.restype = wintypes.BOOL
kernel.CloseHandle.argtypes = [wintypes.HANDLE]
handle = kernel.OpenProcess(0x10, False, args.pid)
if not handle:
    raise ctypes.WinError(ctypes.get_last_error())

def read(address, size):
    buffer = ctypes.create_string_buffer(size)
    count = ctypes.c_size_t()
    if not kernel.ReadProcessMemory(handle, address, buffer, size, ctypes.byref(count)):
        raise ctypes.WinError(ctypes.get_last_error())
    if count.value != size:
        raise RuntimeError("Incomplete process memory read")
    return buffer.raw

def integer(address):
    return struct.unpack("<I", read(address, 4))[0]

try:
    info = {"pid": args.pid, "farDistances": struct.unpack("<8f", read(0x83D4C8, 32)),
            "wallCacheFreeItems": integer(0x8B29DC), "managers": {}}
    for name, address in [("scene", 0x8500A8), ("singleton", 0x850468)]:
        manager = integer(address)
        if manager:
            info["managers"][name] = {"address": hex(manager), "wallCount": integer(manager+0x5C8),
                "firstWords": [hex(n) for n in struct.unpack("<8I", read(manager,32))]}
    print(json.dumps(info, indent=2))
finally:
    kernel.CloseHandle(handle)
