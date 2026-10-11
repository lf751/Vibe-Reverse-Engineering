from pathlib import Path

root = Path('D:/Unity/toolkit-remix/source/extensions')
path = root / 'lightspeed.trex.terrain_blend.core/lightspeed/trex/terrain_blend/core/preset.py'
s = path.read_text().replace('displacement_depths', 'displacement_inward').replace('displacementDepths', 'displacementInward')
s = s.replace('    displacement_quality: str = "medium"', '    displacement_max_iterations: int = 64')
s = s.replace('    displacement_inward: tuple[float, float, float, float] = (0.1, 0.1, 0.1, 0.1)', '''    displacement_inward: tuple[float, float, float, float] = (0.1, 0.1, 0.1, 0.1)
    displacement_outward: tuple[float, float, float, float] = (0.0, 0.0, 0.0, 0.0)
    displacement_height_min: tuple[float, float, float, float] = (0.0, 0.0, 0.0, 0.0)
    displacement_height_max: tuple[float, float, float, float] = (1.0, 1.0, 1.0, 1.0)
    displacement_self_shadow_enabled: bool = True
    displacement_shadow_strength: float = 1.0
    displacement_shadow_max_iterations: int = 32''')
s = s.replace('            *self.displacement_inward,', '''            *self.displacement_inward,
            *self.displacement_outward,
            *self.displacement_height_min,
            *self.displacement_height_max,
            self.displacement_shadow_strength,''')
s = s.replace('        if not 0 <= self.displacement_strength <= 1:\n            raise ValueError("Terrain displacement strength must be in [0, 1].")\n        if self.displacement_quality not in ("low", "medium", "high"):\n            raise ValueError("Terrain displacement quality must be low, medium, or high.")\n        if len(self.displacement_inward) != 4 or min(self.displacement_inward) < 0:\n            raise ValueError("Terrain displacement requires four nonnegative material depths.")', '''        if self.displacement_strength < 0:
            raise ValueError("Terrain displacement strength must be nonnegative.")
        if type(self.displacement_max_iterations) is not int or not 4 <= self.displacement_max_iterations <= 128:
            raise ValueError("Terrain displacement maximum iterations must be an integer in [4, 128].")
        if type(self.displacement_shadow_max_iterations) is not int or not 4 <= self.displacement_shadow_max_iterations <= 64:
            raise ValueError("Terrain shadow maximum iterations must be an integer in [4, 64].")
        if not isinstance(self.displacement_self_shadow_enabled, bool):
            raise ValueError("Terrain self shadow enabled must be a boolean.")
        if not 0 <= self.displacement_shadow_strength <= 1:
            raise ValueError("Terrain shadow strength must be in [0, 1].")
        for depths in (self.displacement_inward, self.displacement_outward):
            if len(depths) != 4 or min(depths) < 0:
                raise ValueError("Terrain displacement requires four nonnegative inward and outward depths.")
        if len(self.displacement_height_min) != 4 or len(self.displacement_height_max) != 4:
            raise ValueError("Terrain displacement requires four height blackpoints and whitepoints.")
        if not all(0 <= low < high <= 1 for low, high in zip(self.displacement_height_min, self.displacement_height_max)):
            raise ValueError("Terrain height range must satisfy 0 <= blackpoint < whitepoint <= 1.")''')
s = s.replace('    "displacement_quality": ("displacementQuality", Sdf.ValueTypeNames.Token),', '    "displacement_max_iterations": ("displacementMaxIterations", Sdf.ValueTypeNames.Int),')
s = s.replace('    "displacement_inward": ("displacementInward", Sdf.ValueTypeNames.Float4),', '''    "displacement_inward": ("displacementInward", Sdf.ValueTypeNames.Float4),
    "displacement_outward": ("displacementOutward", Sdf.ValueTypeNames.Float4),
    "displacement_height_min": ("displacementHeightMin", Sdf.ValueTypeNames.Float4),
    "displacement_height_max": ("displacementHeightMax", Sdf.ValueTypeNames.Float4),
    "displacement_self_shadow_enabled": ("displacementSelfShadowEnabled", Sdf.ValueTypeNames.Bool),
    "displacement_shadow_strength": ("displacementShadowStrength", Sdf.ValueTypeNames.Float),
    "displacement_shadow_max_iterations": ("displacementShadowMaxIterations", Sdf.ValueTypeNames.Int),''')
s = s.replace('    "displacement_quality",', '    "displacement_max_iterations",')
s = s.replace('    "displacement_inward",\n}', '''    "displacement_inward",
    "displacement_outward",
    "displacement_height_min",
    "displacement_height_max",
    "displacement_self_shadow_enabled",
    "displacement_shadow_strength",
    "displacement_shadow_max_iterations",
}''')
s = s.replace('("coverageSandStrength", "coverageSandWidth")', '("coverageSandStrength", "coverageSandWidth", "displacementDepths", "displacementQuality")')
s = s.replace('        preset.displacement_quality,', '        preset.displacement_max_iterations,')
s = s.replace('        Gf.Vec4f(*preset.displacement_inward),', '''        Gf.Vec4f(*preset.displacement_inward),
        Gf.Vec4f(*preset.displacement_outward),
        Gf.Vec4f(*preset.displacement_height_min),
        Gf.Vec4f(*preset.displacement_height_max),
        preset.displacement_self_shadow_enabled,
        preset.displacement_shadow_strength,
        preset.displacement_shadow_max_iterations,''')
s = s.replace('tuple(value) if member in ("units_per_repeat", "displacement_inward") else value', 'tuple(value) if member in ("units_per_repeat", "displacement_inward", "displacement_outward", "displacement_height_min", "displacement_height_max") else value')
s = s.replace('    legacy_strength =', '''    legacy_depths = prim.GetAttribute(_PREFIX + "displacementDepths").Get()
    if legacy_depths is not None and prim.GetAttribute(_PREFIX + "displacementInward").Get() is None:
        preset.displacement_inward = tuple(legacy_depths)
        if prim.GetAttribute(_PREFIX + "displacementSelfShadowEnabled").Get() is None:
            preset.displacement_self_shadow_enabled = False
    legacy_quality = prim.GetAttribute(_PREFIX + "displacementQuality").Get()
    if legacy_quality is not None and prim.GetAttribute(_PREFIX + "displacementMaxIterations").Get() is None:
        if legacy_quality not in ("low", "medium", "high"):
            raise ValueError("Legacy terrain displacement quality must be low, medium, or high.")
        preset.displacement_max_iterations = {"low": 8, "medium": 16, "high": 32}[legacy_quality]
    legacy_strength =''')
path.write_text(s)

path = root / 'lightspeed.trex.terrain_blend.widget/lightspeed/trex/terrain_blend/widget/editor.py'
s = path.read_text()
s = s.replace('    _DISPLACEMENT_QUALITIES = ("low", "medium", "high")\n', '')
s = s.replace('        ("displacement_strength", "Displacement strength"),', '        ("displacement_strength", "Displacement strength"),\n        ("displacement_shadow_strength", "Self shadow strength"),')
start = s.index('                        with ui.HStack(height=self._ROW_HEIGHT):\n                            ui.Label("Displacement quality"')
end = s.index('                        for key, label in self._SETTINGS:', start)
s = s[:start] + '''                        self.__int_row("displacement_max_iterations", "Displacement iterations")
                        self.__models["displacement_self_shadow_enabled"] = ui.SimpleBoolModel(True)
                        with ui.HStack(height=self._ROW_HEIGHT):
                            ui.Label("Enable terrain self shadow", width=self._LABEL_WIDTH)
                            ui.CheckBox(model=self.__models["displacement_self_shadow_enabled"], identifier="terrain_displacement_self_shadow_enabled")
                        self.__int_row("displacement_shadow_max_iterations", "Self shadow iterations")
''' + s[end:]
s = s.replace('                                    self.__float_row(name + "_displacement_depth", "Displacement depth")', '''                                    self.__float_row(name + "_displacement_inward", "Inward depth")
                                    self.__float_row(name + "_displacement_outward", "Outward depth")
                                    self.__float_row(name + "_displacement_height_min", "Height blackpoint")
                                    self.__float_row(name + "_displacement_height_max", "Height whitepoint")''')
s = s.replace('else model.get_value_as_float()\n', 'else model.get_value_as_int()\n            if isinstance(model, ui.SimpleIntModel)\n            else model.get_value_as_float()\n')
start = s.index('        values["displacement_quality"] =')
end = s.index('        return values', start)
s = s[:start] + s[end:]
start = s.index('            elif name == "displacement_quality":')
end = s.index('            else:', start)
s = s[:start] + s[end:]
s = s.replace('Displacement uses each material\'s height texture: white is high and black is recessed. Depth is in game world units. No height texture or depth 0 leaves that layer unchanged.', 'Displacement uses inward and outward depths in game world units. Height blackpoint and whitepoint remap contrast for displacement only. No height texture or both depths 0 leave that layer unchanged. Higher iteration limits improve detail at a rendering cost.')
s = s.replace('    def __string_row(self, key: str, label: str) -> None:', '''    def __int_row(self, key: str, label: str) -> None:
        model = ui.SimpleIntModel()
        self.__models[key] = model
        with ui.HStack(spacing=self._SPACING, height=self._ROW_HEIGHT):
            ui.Label(label, width=self._LABEL_WIDTH)
            ui.IntField(model=model, identifier="terrain_" + key)

    def __string_row(self, key: str, label: str) -> None:''')
path.write_text(s)

path = root / 'lightspeed.trex.terrain_blend.window/lightspeed/trex/terrain_blend/window/extension.py'
s = path.read_text().replace('displacement_quality', 'displacement_max_iterations').replace('displacement_depths', 'displacement_inward').replace('displacement_depth', 'displacement_inward')
s = s.replace('            "displacement_max_iterations": preset.displacement_max_iterations,', '''            "displacement_max_iterations": preset.displacement_max_iterations,
            "displacement_self_shadow_enabled": preset.displacement_self_shadow_enabled,
            "displacement_shadow_strength": preset.displacement_shadow_strength,
            "displacement_shadow_max_iterations": preset.displacement_shadow_max_iterations,''')
s = s.replace('            values[name + "_displacement_inward"] = preset.displacement_inward[index]', '''            values[name + "_displacement_inward"] = preset.displacement_inward[index]
            values[name + "_displacement_outward"] = preset.displacement_outward[index]
            values[name + "_displacement_height_min"] = preset.displacement_height_min[index]
            values[name + "_displacement_height_max"] = preset.displacement_height_max[index]''')
s = s.replace('            displacement_max_iterations=values["displacement_max_iterations"],', '''            displacement_max_iterations=values["displacement_max_iterations"],
            displacement_self_shadow_enabled=values["displacement_self_shadow_enabled"],
            displacement_shadow_strength=values["displacement_shadow_strength"],
            displacement_shadow_max_iterations=values["displacement_shadow_max_iterations"],''')
s = s.replace('            displacement_inward=tuple(', '''            displacement_outward=tuple(values[name + "_displacement_outward"] for name in ("grass", "sand", "rock", "dirt")),
            displacement_height_min=tuple(values[name + "_displacement_height_min"] for name in ("grass", "sand", "rock", "dirt")),
            displacement_height_max=tuple(values[name + "_displacement_height_max"] for name in ("grass", "sand", "rock", "dirt")),
            displacement_inward=tuple(''')
path.write_text(s)
