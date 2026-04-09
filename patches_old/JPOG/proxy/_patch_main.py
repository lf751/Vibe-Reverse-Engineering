"""Patch d3d9_main.c: add g_cfgTerrainDecomposeProj config."""
import os
FPATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'd3d9_main.c')

with open(FPATH, 'rb') as f:
    raw = f.read()

content = raw.decode('utf-8', errors='surrogateescape')

if 'g_cfgTerrainDecomposeProj' in content:
    print('g_cfgTerrainDecomposeProj already exists')
else:
    # 1. Add the global variable declaration after g_cfgTerrainFFPConversion
    old_decl = 'int g_cfgTerrainFFPConversion = 1;\r\n'
    new_decl = 'int g_cfgTerrainFFPConversion = 1;\r\nint g_cfgTerrainDecomposeProj = 1;\r\n'
    content = content.replace(old_decl, new_decl)
    print('Added g_cfgTerrainDecomposeProj declaration')

    # 2. Add INI parsing after TerrainFFPConversion parsing
    old_parse = '"ABTest", "TerrainFFPConversion", 1, iniBuf);'
    new_parse = (old_parse +
        '\r\n        g_cfgTerrainDecomposeProj = GetPrivateProfileIntA(\r\n'
        '            "ABTest", "TerrainDecomposeProj", 1, iniBuf);')
    content = content.replace(old_parse, new_parse)
    print('Added INI parsing')

    # 3. Add log line after TerrainFFPConversion log
    old_log = 'log_int("Config TerrainFFPConversion=", g_cfgTerrainFFPConversion);'
    new_log = (old_log +
        '\r\n        log_int("Config TerrainDecomposeProj=", g_cfgTerrainDecomposeProj);')
    content = content.replace(old_log, new_log)
    print('Added config log')

    with open(FPATH, 'wb') as f:
        f.write(content.encode('utf-8', errors='surrogateescape'))
    print('d3d9_main.c updated')
