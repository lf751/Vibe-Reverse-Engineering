import pefile, struct
dll = r'd:\Games\JPOGHD\Jurassic Park Operation Genesis HD Edition\Shaders\D3D\TTerrainShaderD3D.dll'
pe = pefile.PE(dll)
data = pe.get_memory_mapped_image()
for (rva, label) in [(0x43600, 'SkyVS'), (0x43BE0, 'SkyPS')]:
    print('=== %s ===' % label)
    raw = data[rva:rva+0x400]
    words = [struct.unpack_from('<I', raw, k*4)[0] for k in range(len(raw)//4)]
    j = 0
    while j < len(words):
        w = words[j]; lo = w&0xFFFF; hi=(w>>16)&0xFFFF
        if hi==0xFFFE: j+=lo+1; continue
        if w==0xFFFE0101 or w==0xFFFF0101: j+=1; continue
        if lo==0xFFFF: print('END'); break
        rest = ' '.join('%08X'%words[j+k] for k in range(1,5) if j+k<len(words))
        print('  %08X  %s' % (w, rest))
        j+=1
