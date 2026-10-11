from pathlib import Path

root = Path('D:/Unity/toolkit-remix/source/extensions')
core = root / 'lightspeed.trex.terrain_blend.core/lightspeed/trex/terrain_blend/core/preset.py'
s = core.read_text()
s = s.replace('import math', 'import math\nfrom copy import deepcopy')
s = s.replace('_LAYERS = ("grass", "sand", "rock")', '_LAYERS = ("grass", "sand", "rock", "dirt")')
s = s.replace('coverage_sand_strength', 'dirt_strength').replace('coverage_sand_width', 'dirt_width').replace('coverageSandStrength', 'dirtStrength').replace('coverageSandWidth', 'dirtWidth')
s = s.replace('    dirt_width: float = 2.0', '    dirt_width: float = 2.0\n    dirt_min_elevation: float = 0.0\n    dirt_elevation_fade_width: float = 1.0\n    dirt_units_per_repeat: float = 8.0')
s = s.replace('            self.dirt_width,', '            self.dirt_width,\n            self.dirt_min_elevation,\n            self.dirt_elevation_fade_width,\n            self.dirt_units_per_repeat,')
s = s.replace('Grass exclusion sand strength must be in [0, 1] and border width must be positive.', 'Dirt strength must be in [0, 1] and border width must be positive.')
s = s.replace('        if len(self.layers) != 3:\n            raise ValueError("Terrain requires grass, sand, and rock materials.")', '        if self.dirt_elevation_fade_width <= 0 or self.dirt_units_per_repeat <= 0:\n            raise ValueError("Dirt elevation fade width and world units per repeat must be positive.")\n        if len(self.layers) != 4:\n            raise ValueError("Terrain requires grass, sand, rock, and dirt material slots.")')
s = s.replace('    "dirt_width": ("dirtWidth", Sdf.ValueTypeNames.Float),', '    "dirt_width": ("dirtWidth", Sdf.ValueTypeNames.Float),\n    "dirt_min_elevation": ("dirtMinElevation", Sdf.ValueTypeNames.Float),\n    "dirt_elevation_fade_width": ("dirtElevationFadeWidth", Sdf.ValueTypeNames.Float),\n    "dirt_units_per_repeat": ("dirtUnitsPerRepeat", Sdf.ValueTypeNames.Float),')
s = s.replace('    "dirt_width",\n}', '    "dirt_width",\n    "dirt_min_elevation",\n    "dirt_elevation_fade_width",\n    "dirt_units_per_repeat",\n}')
s = s.replace('    values = (', '    for name in ("coverageSandStrength", "coverageSandWidth"):\n        prim.RemoveProperty(_PREFIX + name)\n    values = (', 1) if False else s
s = s.replace('    prim.CreateAttribute(_PREFIX + "schemaVersion", Sdf.ValueTypeNames.Int).Set(1)', '    prim.CreateAttribute(_PREFIX + "schemaVersion", Sdf.ValueTypeNames.Int).Set(1)\n    for name in ("coverageSandStrength", "coverageSandWidth"):\n        prim.RemoveProperty(_PREFIX + name)')
s = s.replace('        preset.dirt_width,', '        preset.dirt_width,\n        preset.dirt_min_elevation,\n        preset.dirt_elevation_fade_width,\n        preset.dirt_units_per_repeat,')
s = s.replace('    layers = []\n    for name in _LAYERS:', '    legacy_strength = prim.GetAttribute(_PREFIX + "coverageSandStrength").Get()\n    migrate_coverage = legacy_strength is not None and prim.GetAttribute(_PREFIX + "dirtStrength").Get() is None\n    if migrate_coverage:\n        preset.dirt_strength = legacy_strength\n        legacy_width = prim.GetAttribute(_PREFIX + "coverageSandWidth").Get()\n        preset.dirt_width = legacy_width if legacy_width is not None else 2.0\n    layers = []\n    for name in _LAYERS:')
s = s.replace('        if not targets and not preset.enabled:', '        if not targets and name == "dirt" and migrate_coverage:\n            layers.append(deepcopy(layers[1]))\n            continue\n        if not targets and (not preset.enabled or (name == "dirt" and preset.dirt_strength == 0)):')
core.write_text(s)

for role, filename in [('widget', 'editor.py'), ('window', 'extension.py')]:
    path = root / f'lightspeed.trex.terrain_blend.{role}/lightspeed/trex/terrain_blend/{role}/{filename}'
    s = path.read_text().replace('coverage_sand_strength', 'dirt_strength').replace('coverage_sand_width', 'dirt_width')
    s = s.replace('("grass", "sand", "rock")', '("grass", "sand", "rock", "dirt")')
    if role == 'widget':
        s = s.replace('Grass exclusion sand strength', 'Dirt strength').replace('Sand border width (world units)', 'Dirt border width (world units)')
        s = s.replace('        ("dirt_width", "Dirt border width (world units)"),', '        ("dirt_width", "Dirt border width (world units)"),\n        ("dirt_min_elevation", "Dirt minimum elevation"),\n        ("dirt_elevation_fade_width", "Dirt elevation fade width"),')
        s = s.replace('before both sand/grass and rock thresholds', 'before sand/grass, rock, and dirt thresholds')
        s = s.replace('Grass exclusion sand follows the live game grass mask around paths and buildings. The mask also includes shores and rock exclusions. Strength 0 disables it.', 'Dirt follows live grass exclusions around paths and buildings above its minimum elevation. It replaces only grass, preserving sand and rock. Elevation fade width softens its lower boundary; strength 0 disables it.')
    else:
        s = s.replace('            "dirt_width": preset.dirt_width,', '            "dirt_width": preset.dirt_width,\n            "dirt_min_elevation": preset.dirt_min_elevation,\n            "dirt_elevation_fade_width": preset.dirt_elevation_fade_width,')
        s = s.replace('values[name + "_repeat"] = preset.units_per_repeat[index]', 'values[name + "_repeat"] = preset.dirt_units_per_repeat if name == "dirt" else preset.units_per_repeat[index]')
        s = s.replace('tuple(values[name + "_repeat"] for name in ("grass", "sand", "rock", "dirt"))', 'tuple(values[name + "_repeat"] for name in ("grass", "sand", "rock"))')
        s = s.replace('            dirt_width=values["dirt_width"],', '            dirt_width=values["dirt_width"],\n            dirt_min_elevation=values["dirt_min_elevation"],\n            dirt_elevation_fade_width=values["dirt_elevation_fade_width"],\n            dirt_units_per_repeat=values["dirt_repeat"],')
    path.write_text(s)

for path in root.glob('lightspeed.trex.terrain_blend.*/**/test_*.py'):
    s = path.read_text().replace('coverage_sand', 'dirt').replace('coverageSand', 'dirt').replace('Grass exclusion sand strength', 'Dirt strength').replace('live grass mask sand settings', 'live grass mask dirt settings')
    s = s.replace('len(preset.layers), 3', 'len(preset.layers), 4')
    path.write_text(s)

for role in ('core', 'widget', 'window'):
    path = root / f'lightspeed.trex.terrain_blend.{role}/docs/README.md'
    s = path.read_text().replace('three-layer', 'four-layer').replace('coverageSandStrength', 'dirtStrength').replace('coverageSandWidth', 'dirtWidth').replace('coverage sand', 'dirt').replace('Coverage-based sand', 'Grass-exclusion dirt').replace('control sand around exclusions', 'control dirt around exclusions').replace('grass, sand, and rock.', 'grass, sand, rock, and dirt.')
    s += '\nDirt uses an independent material slot and replaces only the existing grass contribution, preserving sand and rock.\n`dirtMinElevation` (float, 0) gates it in the flipped elevation coordinate; `dirtElevationFadeWidth` (float, 1, positive) softens the boundary.\n`dirtUnitsPerRepeat` (float, 8, positive) controls Dirt tiling independently of the existing float3 `unitsPerRepeat`.\n`dirtMaterial` is optional when Dirt strength is zero and required otherwise. Older coverage-sand presets migrate on load by copying their sand material into Dirt; applying or exporting writes only the new Dirt schema.\n'
    path.write_text(s)
    config = root / f'lightspeed.trex.terrain_blend.{role}/config/extension.toml'
    s = config.read_text().replace('version = "1.1.0"', 'version = "1.2.0"')
    config.write_text(s)
    changelog = root / f'lightspeed.trex.terrain_blend.{role}/docs/CHANGELOG.md'
    s = changelog.read_text()
    s += '\n### Changed\n\n- Replace coverage sand with an independent Dirt material, minimum elevation, and elevation fade controls.\n'
    changelog.write_text(s)
