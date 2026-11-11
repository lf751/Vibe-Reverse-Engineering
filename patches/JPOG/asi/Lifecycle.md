# ASI hook and camera lifecycle

The ASI installs process-lifetime jumps in game modules and pins itself before
starting its worker. Its trampolines remain allocated until process exit. An
explicit `FreeLibrary` therefore cannot unload the code while native jumps still
reference it. Successful weather and terrain trampolines are instruction-cache
flushed before publication.

The selected camera is released by the native `TRenderContextD3D` destructor,
rather than by a timer or a different context appearing. The existing distance
and positive-determinant filters still choose its replacement. Reflection passes
cannot displace a live main context. The water reset repair retains the same
quality-1/high-water guards and shader-handle recreation rules. Its diagnostic
file handle is scoped to each record instead of being retained indefinitely.

## Binary evidence

Installed `TRenderD3DInterface.dll` exports:

- `?SetRenderMatrices@TRenderContextD3D@Toshi@@AAEXXZ`, RVA `0x7180`.
  Signature `56 8B F1 8B 46 04`: push esi, mov esi/ecx, mov eax/[esi+4].
- `??1TRenderContextD3D@Toshi@@UAE@XZ`, RVA `0x71C0`.
  First instruction `C7 01 <module+0x74480>` is six bytes, restoring its vtable.
  At `0x71C6` it calls the imported base destructor, then returns at `0x71CC`.

Both signatures are checked before either camera patch is installed. The absolute
vtable address is computed from the loaded module base to account for relocation.
The destructor trampoline copies the complete six-byte instruction, then resumes
at `0x71C6`. Module image bounds are checked before signature comparison.

## Validation (2026-11-11)

- ASI x86 release build passed without compiler warnings.
- CPU-only synthetic camera test passed all fourteen assertions, including native
  execution of the destructor trampoline, unrelated context preservation,
  selected context destruction, reflection rejection and new-camera selection.
- `git diff --check` passed.
- Logs: `test-artifacts/JPOG/asi-lifecycle/build.log` and `test.log`.
- Recovery baseline: `backups/JPOG/2026-11-11_191007_asi-lifecycle`.
- No installed deployment or live game launch was performed for this change.

Live validation still needs park/save transitions and minimise/restore with the
actual game. The native destructor evidence establishes the ownership hook, but
does not establish how often JPOG replaces render contexts in normal play.
