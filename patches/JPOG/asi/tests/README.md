# ASI camera lifecycle regression

`camera_lifecycle.c` includes the actual ASI source and uses a synthetic mapped
render module. Its x86 trampoline runs native instructions, not a mocked
callback. It checks signature rejection without mutation, complete-instruction
patches, reflection filtering, selected/unrelated context destruction, camera
reacquisition and module-range rejection.

Compile in an x86 Visual Studio environment, with output/intermediates in
`test-artifacts/JPOG/asi-lifecycle`:

```
cl /nologo /W4 /O2 /GS- /Zl /I<runtime>/public/include /c <project>/asi/tests/camera_lifecycle.c <project>/asi/terrain_water_reset.c
link /nologo /NODEFAULTLIB /ENTRY:TestMain /SUBSYSTEM:CONSOLE /OUT:camera_lifecycle.exe camera_lifecycle.obj terrain_water_reset.obj kernel32.lib
camera_lifecycle.exe
```

Exit 0 passes. Codes 1-14 identify the failed assertion in `TestMain`.
This is a CPU-only lifecycle check, not live JPOG camera or reset validation.
