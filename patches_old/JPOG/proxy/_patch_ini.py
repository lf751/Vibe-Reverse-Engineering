"""Patch proxy.ini: add TerrainDecomposeProj=1 after TerrainFFPConversion."""
FPATH = r'd:\Games\JPOGRTX\JPOG\proxy.ini'

with open(FPATH, 'rb') as f:
    raw = f.read()

content = raw.decode('utf-8', errors='surrogateescape')

if 'TerrainDecomposeProj' not in content:
    old = 'TerrainFFPConversion=1'
    new = (old +
        '\r\n; Decompose combined V*P projection matrix for terrain draws.\r\n'
        '; When Toshi sets PROJ=View*Proj combined, extract real Proj = inv(V)*VP.\r\n'
        '; 1 = enabled, 0 = disabled.\r\n'
        'TerrainDecomposeProj=1')
    content = content.replace(old, new)
    with open(FPATH, 'wb') as f:
        f.write(content.encode('utf-8', errors='surrogateescape'))
    print('Added TerrainDecomposeProj=1 to proxy.ini')
else:
    print('TerrainDecomposeProj already exists')
