"""Temporarily compare native fade floats using external process memory access."""
import argparse
import ctypes
import json
import struct
import threading
from ctypes import wintypes

parser=argparse.ArgumentParser()
parser.add_argument('pid',type=int)
parser.add_argument('--float',action='append',required=True,dest='changes')
parser.add_argument('--seconds',type=float,default=20)
args=parser.parse_args()
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD]
k.OpenProcess.restype=wintypes.HANDLE
k.ReadProcessMemory.argtypes=[wintypes.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
k.WriteProcessMemory.argtypes=k.ReadProcessMemory.argtypes
k.CloseHandle.argtypes=[wintypes.HANDLE]
h=k.OpenProcess(0x38,False,args.pid)
if not h: raise ctypes.WinError(ctypes.get_last_error())
originals=[]
def transfer(method,address,data):
    count=ctypes.c_size_t()
    if not method(h,address,data,4,ctypes.byref(count)): raise ctypes.WinError(ctypes.get_last_error())
    if count.value!=4: raise RuntimeError('Incomplete process memory transfer')
try:
    for change in args.changes:
        address_text,value_text=change.split('=',1)
        address=int(address_text,0)
        old=ctypes.create_string_buffer(4)
        transfer(k.ReadProcessMemory,address,old)
        originals.append((address,old))
        new=ctypes.create_string_buffer(struct.pack('<f',float(value_text)))
        transfer(k.WriteProcessMemory,address,new)
        print(json.dumps({'address':hex(address),'old':struct.unpack('<f',old.raw)[0],'new':float(value_text)}),flush=True)
    threading.Event().wait(args.seconds)
finally:
    try:
        for address,old in reversed(originals): transfer(k.WriteProcessMemory,address,old)
        print('Restored original runtime values.',flush=True)
    finally: k.CloseHandle(h)
