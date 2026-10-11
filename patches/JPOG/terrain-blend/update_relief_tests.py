from pathlib import Path

root=Path('D:/Unity/toolkit-remix/source/extensions')
path=root/'lightspeed.trex.terrain_blend.core/lightspeed/trex/terrain_blend/core/tests/e2e/test_workflow.py'
s=path.read_text().replace('displacement_depths','displacement_inward').replace('displacementDepths','displacementInward')
s=s.replace('            TerrainPreset(displacement_strength=1.1),\n','')
s=s.replace('            TerrainPreset(displacement_quality="ultra"),', '''            TerrainPreset(displacement_max_iterations=3),
            TerrainPreset(displacement_max_iterations=129),
            TerrainPreset(displacement_max_iterations=4.5),
            TerrainPreset(displacement_shadow_max_iterations=65),
            TerrainPreset(displacement_shadow_max_iterations=True),
            TerrainPreset(displacement_self_shadow_enabled=1),
            TerrainPreset(displacement_shadow_strength=1.1),
            TerrainPreset(displacement_shadow_strength=float("nan")),
            TerrainPreset(displacement_outward=(0, 0, -1, 0)),
            TerrainPreset(displacement_outward=(0, 0, 0)),
            TerrainPreset(displacement_outward=(0, 0, float("inf"), 0)),
            TerrainPreset(displacement_height_min=(0, 0, 0)),
            TerrainPreset(displacement_height_max=(1, 1, 1)),
            TerrainPreset(displacement_height_min=(-1, 0, 0, 0)),
            TerrainPreset(displacement_height_max=(2, 1, 1, 1)),
            TerrainPreset(displacement_height_min=(1, 0, 0, 0)),
            TerrainPreset(displacement_height_max=(float("nan"), 1, 1, 1)),''')
s=s.replace('            "displacementQuality",','''            "displacementMaxIterations",
            "displacementOutward",
            "displacementHeightMin",
            "displacementHeightMax",
            "displacementSelfShadowEnabled",
            "displacementShadowStrength",
            "displacementShadowMaxIterations",''')
s=s.replace('        self.assertEqual(loaded.displacement_quality, "medium")','''        self.assertEqual(loaded.displacement_max_iterations, 64)
        self.assertTrue(loaded.displacement_self_shadow_enabled)''')
s+='''
    async def test_read_old_parallax_preset_migrates_depth_quality_and_keeps_shadow_disabled(self):
        """Convert previous parallax settings into one editable relief contract."""
        write_preset(self.stage, TerrainPreset())
        prim = self.stage.GetPrimAtPath(PRESET_PATH)
        for name in ("displacementInward", "displacementMaxIterations", "displacementSelfShadowEnabled"):
            prim.RemoveProperty("remix:terrainBlend:" + name)
        prim.CreateAttribute("remix:terrainBlend:displacementDepths", Sdf.ValueTypeNames.Float4).Set((0, .25, .5, 1))
        prim.CreateAttribute("remix:terrainBlend:displacementQuality", Sdf.ValueTypeNames.Token).Set("high")
        loaded = read_preset(self.stage)
        self.assertEqual(loaded.displacement_inward, (0, .25, .5, 1))
        self.assertEqual(loaded.displacement_max_iterations, 32)
        self.assertFalse(loaded.displacement_self_shadow_enabled)
        loaded.displacement_strength = 2.5
        write_preset(self.stage, loaded)
        self.assertFalse(prim.GetAttribute("remix:terrainBlend:displacementDepths"))
        self.assertFalse(prim.GetAttribute("remix:terrainBlend:displacementQuality"))
        self.assertEqual(read_preset(self.stage).displacement_strength, 2.5)
'''
path.write_text(s)

path=root/'lightspeed.trex.terrain_blend.window/lightspeed/trex/terrain_blend/window/tests/e2e/test_window.py'
s=path.read_text().replace('displacement_depths','displacement_inward').replace('displacement_depth','displacement_inward')
s=s.replace('        quality = ui_test.find("Terrain Blend//Frame/**/ComboBox[*].identifier==\'terrain_displacement_quality\'")', '        iterations = ui_test.find("Terrain Blend//Frame/**/IntField[*].identifier==\'terrain_displacement_max_iterations\'")')
s=s.replace('        quality.widget.model.get_item_value_model().set_value(2)', '        await self.__replace_field(iterations, "96")')
s=s.replace('        await self.__replace_field(strength, "2")','        await self.__replace_field(strength, "-1")')
s=s.replace('        await self.__replace_field(strength, "0.75")','        await self.__replace_field(strength, "2.5")')
s=s.replace('        self.assertEqual(authored.displacement_strength, 0.75)', '        self.assertEqual(authored.displacement_strength, 2.5)')
s=s.replace('        self.assertEqual(authored.displacement_quality, "high")', '        self.assertEqual(authored.displacement_max_iterations, 96)')
s=s.replace('        quality.widget.model.get_item_value_model().set_value(0)', '        await self.__replace_field(iterations, "4")')
s=s.replace('        self.assertEqual(quality.widget.model.get_item_value_model().get_value_as_int(), 2)', '        self.assertEqual(iterations.widget.model.get_value_as_int(), 96)')
s=s.replace('        self.assertEqual(exported.displacement_strength, 0.75)', '        self.assertEqual(exported.displacement_strength, 2.5)')
s=s.replace('        self.assertEqual(exported.displacement_quality, "high")', '        self.assertEqual(exported.displacement_max_iterations, 96)')
# Inward/outward and height range fields are all independently editable.
needle='        await self.__button("terrain_apply").click()\n        await ui_test.human_delay()\n        authored = read_preset(self.context.get_stage())'
replacement='''        outward = (0.125, 0.25, 0.375, 0.5)
        height_min = (0, 0.125, 0.25, 0.375)
        height_max = (0.5, 0.625, 0.75, 1)
        for key, values in (("displacement_outward", outward), ("displacement_height_min", height_min), ("displacement_height_max", height_max)):
            for name, value in zip(("grass", "sand", "rock", "dirt"), values):
                control = ui_test.find(f"Terrain Blend//Frame/**/FloatField[*].identifier=='terrain_{name}_{key}'")
                scroll.widget.scroll_y += control.center.y - scroll.center.y
                await ui_test.human_delay()
                await self.__replace_field(control, str(value))
        shadow_strength = ui_test.find("Terrain Blend//Frame/**/FloatField[*].identifier=='terrain_displacement_shadow_strength'")
        scroll.widget.scroll_y += shadow_strength.center.y - scroll.center.y
        await ui_test.human_delay()
        await self.__replace_field(shadow_strength, "0.5")
        shadow_iterations = ui_test.find("Terrain Blend//Frame/**/IntField[*].identifier=='terrain_displacement_shadow_max_iterations'")
        scroll.widget.scroll_y += shadow_iterations.center.y - scroll.center.y
        await ui_test.human_delay()
        await self.__replace_field(shadow_iterations, "48")
        shadow = ui_test.find("Terrain Blend//Frame/**/CheckBox[*].identifier=='terrain_displacement_self_shadow_enabled'")
        await shadow.click()
        await shadow.click()
''' + needle
s=s.replace(needle,replacement)
s=s.replace('        self.assertEqual(authored.displacement_inward, depths)', '''        self.assertEqual(authored.displacement_inward, depths)
        self.assertEqual(authored.displacement_outward, outward)
        self.assertEqual(authored.displacement_height_min, height_min)
        self.assertEqual(authored.displacement_height_max, height_max)
        self.assertTrue(authored.displacement_self_shadow_enabled)
        self.assertEqual(authored.displacement_shadow_strength, .5)
        self.assertEqual(authored.displacement_shadow_max_iterations, 48)''')
s=s.replace('        self.assertEqual(exported.displacement_inward, depths)', '''        self.assertEqual(exported.displacement_inward, depths)
        self.assertEqual(exported.displacement_outward, outward)
        self.assertEqual(exported.displacement_height_min, height_min)
        self.assertEqual(exported.displacement_height_max, height_max)
        self.assertTrue(exported.displacement_self_shadow_enabled)
        self.assertEqual(exported.displacement_shadow_strength, .5)
        self.assertEqual(exported.displacement_shadow_max_iterations, 48)''')
# The generic string replacement only applies POM's field edits: undo accidental Dirt strength changes.
start=s.index('    async def test_change_dirt_')
end=s.index('    async def test_enable_displacement_',start)
part=s[start:end].replace('await self.__replace_field(strength, "-1")','await self.__replace_field(strength, "2")').replace('await self.__replace_field(strength, "2.5")','await self.__replace_field(strength, "0.75")')
s=s[:start]+part+s[end:]
path.write_text(s)
