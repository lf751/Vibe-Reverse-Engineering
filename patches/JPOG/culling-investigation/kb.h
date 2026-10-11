// Module-specific observations; all addresses below are preferred TTerrainInterface.dll VAs.
@ 0x1000B410 void __thiscall CullTerrain(void* terrain, void* worldPlanes, unsigned int planeCount, void** outHead, unsigned int activeMask, void* guardPlanes, int guardMask);
@ 0x10007DB0 void __thiscall SetupTerrainCull(void* root, void* worldPlanes, unsigned int planeCount, void** outHead, unsigned int activeMask, void* guardPlanes, int guardMask);
@ 0x10006C70 void __thiscall TraverseTerrainCull(void* node, unsigned int activeMask);
@ 0x10007020 int __thiscall SectionFrustumCheck(void* section, void* planes, unsigned int planeCount, unsigned int activeMask, int testMask);
$ 0x10012544 void* g_cullWorldPlanes
$ 0x10012540 unsigned int g_cullPlaneCount
$ 0x1001253C void** g_cullOutputHead
$ 0x10012538 void* g_cullGuardPlanes
$ 0x10012534 int g_cullGuardMask
