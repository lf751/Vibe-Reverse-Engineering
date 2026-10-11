// Auto-generated knowledge base

// [propagated: _thunk_sub_10005B90, 0.80]
@ 0x100029F0 _thunk_sub_10005B90;

// [propagated: _thunk_sub_1000C9BC, 0.80]
@ 0x1000C9E8 _thunk_sub_1000C9BC;


// Confirmed 2026-10-09 from exports and disassembly, 32-bit build.
struct TStrataFieldBufferLayout {
  void* next; void* previous;
  unsigned char sectionX, sectionZ, lod, padding;
  void* ownerSection;
  unsigned char strata[289];
};
@ 0x1000A8A0 void* __thiscall CacheStrataBuffer(void* terrain, unsigned int sectionX, unsigned int sectionZ, void* section);
@ 0x10006140 void* __thiscall GetStrataBuffer(void* section);
@ 0x10006130 void* __thiscall GetShadowBuffer(void* section);
@ 0x10005DD0 void* __thiscall GetTerrainSection(void* terrain, unsigned int x, unsigned int z);
@ 0x100060F0 void __thiscall GetTerrainSectionPos(void* section, unsigned int* x, unsigned int* z);
$ 0x10012530 void* g_terrainInterface
