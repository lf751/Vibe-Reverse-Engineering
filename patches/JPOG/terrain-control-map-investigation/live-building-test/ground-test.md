# Live landscaping result

Compared the settled post-path snapshot with the post-landscaping snapshot from
the same terrain interface instance, `0x03D8C010`.

- 80,230 valid world-grid coordinates matched across the two strata captures.
- 335 unique strata samples changed.
- A stricter comparison restricted to sections retaining the same LOD confirms
  all 335 changed coordinates, all at LOD 0. Shared border vertices account for
  350 changed entries across 342 same-LOD section buffers.
- Shading planes A and B changed at 650 and 633 matching coordinates respectively.
- Strata changes are localized to section groups near (31–32, 40–42) and
  (35–37, 34–35), consistent with multiple localized edits.

This confirms that the scalar strata control is updated during live terrain
landscaping. The separate path test found no strata response to path placement.
The fields can potentially drive an optional game-derived material blend, but
scalar-to-material calibration and GPU/runtime transport still need validation.
These tests do not establish a general path or terrain-paint mask.

Evidence: `ground-comparison.json`, `ground-same-lod-comparison.json`, and
`after-ground/fields.json`. No integration changes were made.
