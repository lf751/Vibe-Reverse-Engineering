# Live path placement result

The user's loaded save was captured before and after construction of a visibly new
zigzag path, then captured again after allowing the caches to settle. The terrain
interface instance remained `0x03D8C010`; both captures contained 400 strata buffers.
Comparison uses matching world-grid coordinates and valid samples at each buffer's
LOD, with finer samples taking precedence at shared section borders.

- 90,177 shared valid strata samples: zero changed in both post-path captures.
- Shadow/shading planes A and B: zero changes over 90,177 shared valid samples each
  in the first post-path capture.
- Fourteen section UI-flag records changed in the first comparison. These are
  flags, not established continuous material weights; unrelated game updates may
  also affect them.
- The new path is visible in `after-path/game.png`.

This establishes that the captured strata and shading fields did not encode this
path placement. It does not establish whether the strata field updates after a
terrain-height edit, or whether other game fields carry path/paint control data.
No integration or game binary changes were made.

Raw snapshots and JSON comparison results are stored beside this report.
