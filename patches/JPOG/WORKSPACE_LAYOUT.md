# Local JPOG workspace

- `patches/JPOG/`: active proxy and ASI source, knowledge base, findings, and project scripts.
- `backups/JPOG/`: local recovery snapshots and cleanup manifest, ignored by Git.
- `test-artifacts/JPOG/terrain-blend/`: isolated game, staged compressed runtime, smoke fixtures, captures, and build logs, ignored by Git.
- `tests/`: tracked toolkit regression tests.
- `patches/JPOG/archived-findings/`: previously tracked findings and knowledge bases from retired snapshots.

The October 11, 2026 cleanup retained current release recovery snapshots, the
compression rollback baseline, and the most recent normal-game deployment
snapshot. Older snapshots were retired after their notes, logs, configuration,
and source files were saved in `backups/JPOG/archived-notes-and-sources.zip` and
checked with CRC and SHA-256. Generated binary payloads were removed. The local
`cleanup-manifest.json` records every preserved archived file and checksum.

Deployment and rollback scripts in `terrain-blend/` use the new locations. The
desktop sharing package and installed game were not modified by this cleanup.
Historical paths inside archived findings describe their original captures.
