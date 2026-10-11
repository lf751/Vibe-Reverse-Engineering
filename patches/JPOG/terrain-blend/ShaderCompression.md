# Experimental embedded shader compression — 2026-10-11

The opt-in Meson flag `compress_embedded_shaders` defaults to false. Slang-generated
RTX shader headers can use DXVK's existing word codec, serialized as a versioned
magic, original word count, payload byte count, reserved zero word, two-bit width
masks and an eight-byte-padded little-endian payload. GLSL shader headers remain
raw. The embedded decoder accepts both representations, reconstructs owned
SPIR-V, and retains it in ShaderManager's existing cache. Runtime shader reloads
continue reading raw `.spv` files. Header-mode changes force shader regeneration;
the mode stamp is written only after a successful shader build.

The release build and three regressions passed: embedded codec interoperability,
smooth terrain normals, and preset hot reload. The codec fixture covers all word
byte widths, zero, partial mask groups, packed-word crossings and malformed data.
A real 2,181,844-byte terrain shader round-tripped exactly from 1,476,216 packed
bytes. A 100-decode CPU microbenchmark averaged approximately 2.1 ms per decode;
this is not a game loading-time benchmark.

The DLL decreased from 388,189,184 bytes (370.2 MiB) to 261,105,152 bytes
(249.0 MiB), saving 121.2 MiB (32.7%). The complete 191-file experimental package
was hash-verified in `terrain-blend/test-game/.trex`. Configuration, terrain
preset, root integration and the retained bridge were preserved. Existing shader
caches were retained. The test game booted and logged active terrain smoothing
without shader decoding or creation errors.

The sharing package now uses the compressed release runtime. The release build
and install succeeded, and its DLL exactly matches the gameplay-tested compressed
binary. The normal release build option is enabled and _output contains the
compressed package. The normal game installation was not replaced.

Experimental package: `compressed-runtime/` beside this document. Baseline package
location: `compression-backup.txt`. To switch the isolated game back after closing
it, run `deploy-compression-test.ps1 -Baseline`; omit the switch to reinstall the
compressed package. The switch script backs up the replaced runtime and verifies
all source file hashes and retained integration files.

Evidence: `compression-runtime-build*.log`, `compression-runtime-tests-final.log`,
`compression-test-deployment.json`, `compression-real-*.spv`, and
`compression-startup.png` and `compression-real-test.log`.

The user loaded the mountain save and reported that loading felt a bit slower,
but the game runs and looks fine. They considered the smaller package worth
keeping. The compressed runtime remains installed in the isolated test game;
the desktop release folder now uses the same compressed runtime. This is subjective loading-time feedback,
not a measured cold-start or GPU performance comparison.

