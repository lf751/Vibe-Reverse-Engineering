## Native terrain culling at long draw distance — 2026-10-09

### Summary
Active Constant.ini has HIGH_TerrainDrawDistance=7000 (original480), very wide object fades and modified FOV. Native CPU terrain culling is quadtree and section AABB versus six camera planes; these routines have no independent distance cap, fog arithmetic, fixed-capacity output array, or draw-distance-to-byte conversion. Output is a linked list. A live test should distinguish CPU visibility from GPU projection/depth before changing fog. No game files changed.

### Addresses (preferred TTerrainInterface.dll base0x10000000)
| Address | Meaning |
|---|---|
|0x1000B410|TTerrainInterface::CullTerrain public virtual wrapper|
|0x10007DB0|Cull setup: writes globals and dispatches root quadtree|
|0x10006C70|Recursive quadtree AABB plane checks and output insertion|
|0x10007020|TTerrainSection::SectionFrustumCheck|
|0x100070F0|Section reject return when ComparePlane=-1|
|0x00478820|simjp.exe terrain visibility/render orchestration|
|0x004788B0|GetWorldPlanes call through IAT0x739F50|
|0x004788C1|GetWorldGuardPlanes call through IAT0x739F48|
|0x0047891C|CullTerrain call through terrain virtual slot+0x3C|

### Contracts and formulas
CullTerrain is thiscall, ECX=terrain, args at entry ESP+4=worldPlanes; +8=planeCount; +0xC=TTerrainSection**outHead; +0x10=activeMask; +0x14=guardPlanes; +0x18=guardMask. Wrapper clears *outHead then passes terrain+0x2C root to setup; setup uses root+8 node pointer.

Globals RVA12544=planes,12540=planeCount,1253C=outHead,12538=guardPlanes,12534=guardMask.

SectionFrustumCheck is thiscall ECX=section; ESP+4=planes;+8=planeCount;+0xC=activeMask;+0x10=testMask. Section AABB center=(16*uint8(section+0x32)+8,float(section+8),16*uint8(section+0x33)+8); half extent=(8,float(section+0xC),8). Each enabled bit i checks a 16-byte plane using TAABox::ComparePlane. Return1 clears bit i (fully inside), return-1 rejects section with return-1, return0 preserves bit (intersects). Otherwise returns remaining mask.

Quadtree uses the same ComparePlane classification for node bounds, then section bounds at leaves. Node flag+0x18 high bit denotes leaf with up to four section pointers at+0x20,+0x24,+0x28,+0x2C. Accepted sections update low6 bits of flags+0x1C, set section+0x14=previous output head, and replace head. There is no output count/capacity parameter. Guard-plane secondary pass is only applied if guardMask intersects remaining mask.

simjp00478820 gets current render context, obtains six WorldPlanes and six WorldGuardPlanes, then Negate4 on every plane in both sets. It invokes CullTerrain with initialmask0x3F and guardMask=context+0x10. Secondarymask0 disables guard pass.

### Suggested live verification
1. Record projection and both plane sets under a good and failing camera. Native projection evidence from parent: near1,far~7000. Do not assume actual depth format from ScreenBestZ=false; inspect created depth surface format.
2. At CullTerrain entry, change arg4 activeMask from0x3F to0 for one invocation/frame. Recursive zero-mask branch walks all existing leaves and inserts all nonnull sections without any plane test. This bypasses node and section checks together; forcing only SectionFrustumCheck return cannot bypass earlier node rejection.
3. If holes disappear, inspect incorrect plane(s) and node/section bounds; keep primary/guard paths distinguishable. If holes persist, record that sections were output and submitted, then inspect depth format, near/far projection and water/terrain depth relationships.
4. Output head after call is *arg3, traverse next pointer+0x14; section coords unsignedbytes+0x32/+0x33. Observe ComparePlane -1 at SectionFrustumCheck return to identify culled section coordinates.
5. Repeat affected live path without Remix before attributing native issue to proxy projection.

### Tools and limitations
Existing populated/indexed SimJPTerrain,TTerrainInterface,TTerrainShaderD3D projects reused. Installed pyghidra3.1 currently rejects Ghidra11.4.3 and requests12.0; fallback r2ghidra decompilation was used, verified against disassembly for contracts and section bounds. No environment changes, new full bootstrap, source edits, or live tools performed by static agent. Native fog linkage and depth format selection remain unproven; do not claim pc_fognear/far causes these holes.

### Live feedback and follow-up — 2026-10-09
Parent live run changed simjp initial mask3F to00 and all terrain disappeared (water/trees remained), then restored3F. That result does NOT establish GPU/depth cause: zero-mask path has different low6sectionflags and may trigger later behavior. Compare raw outHead at0047891F and final list0x84B150 before using the bypass as diagnostic. Safer candidate: preserve3F mask and force ComparePlane return0(intersects) only when called from native terrain node/section checks.

Downstream simjp uses dynamic sorted array at0x84B088: growth count+0, current count+4(0x84B08C), capacity+8(0x84B090),data+0xC(0x84B094). Initial capacity600 appears at0047899A and00478DDF; this is NOT a hard600section limit. Insert00479860 grows when requiredCount>capacity, allocates capacity+max(growth,insertCount) at0047998D+, and moves elements. Sorted key=(sectionX-cameraSectionX)^2+(sectionZ-cameraSectionZ)^2. Camera coordinates are float-to-int then arithmetic-shift4. Final linked head is global0x84B150. Do not patch600 as a culling fix based on this evidence.

### Filtered intersection test and native fog linkage — follow-up
TKernelInterface.dll exported TAABox::ComparePlane is RVA0x8210 (preferredVA0x10008210). TTerrainInterface caller return RVAs are0x6D17 (node) and0x70BE (section). Its IAT RVA0xD054 imports that exact function. To bypass culling while retaining0x3F flags, filter ComparePlane calls by those return addresses and return0(intersects). This avoids the different zero-mask branch. Scope overrides strictly to these return addresses.

Native UpdateFadeLighting (simjp005F9130) reads scale=float(*(rootTask+0xD4)+0x38), where rootTask comes from00484DB0. Its targetweather at manager+0x1AC has fognear/far fields+0xB8,+0xBC. The completed-transition path multiplies each by scale and calls TRenderContext::SetFogDistance at005F9217 (IAT739EB4), return005F921D. The transition path interpolates each old/new fogfraction then multiplies by same scale, call005F9793, return005F9799. A third call is005F35FC (return005F3602). Confirm scale=actual drawdistance by reading it live; static evidence proves scaling, not that scaling causes holes.

Config stringpc_fognear atD330C0 is referenced by loader005F5ABC with destinationstack+0x64; pc_fogfarD330E0 by005F5ACE with destinationstack+0x68. These are native loader settings. Updated fogcolour goes to rendercontext+0x15C,+0x160,+0x164,+0x168 before SetFogDistance. None of the established terrain CPU traversal routines reads these fog values.

### Projection and terrain submission — follow-up
TRenderInterface.dll SetProjectionParameters exported RVA0xA8F0(thiscall context, const pointer-to6floats), copying them tocontext+30,+34,+38,+3C,+40,+44; invokes dirty flag routineRVA0xA850(true). This sets context+8 bit0, clears bits4..7. TRenderD3DInterface.dll ComputePerspectiveProjection RVA0x6B10 reads near=context+40,far=context+44; writes depth coefficientscontext+4AC=far/(far-near),context+4BC=-near*far/(far-near),context+4B0=1. Projection startscontext+484. For reversible testing, filter maincontext and override near/far at compute entry so subsequent existing projection/plane generation uses them.

simjp ARenderer::GetFarClip00472A20 reads floatthis+38. Nearby00472A30 setter writesfloatthis+38 and returnsoldvaluevia x87. This identifies the fog scale(root renderer+38) as farclip, not merely an unnamed factor.

TTerrainMaterialHAL::PreRender(shaderDLL RVA E450), non-highend branch, submits native projectioncontext+484 usingD3D8 SetTransform(PROJECTION=3), identityVIEW=2, and context+4C asWORLD=0x100. TTterrainShaderHAL::RenderRVA5790 low and medium paths bindvertexstreamstride20/28, then indexedtriangle strips. They explicitly disable FFP fog withSetRenderState(FOGENABLE=27,0) beforeterrain submission. Highend path usesstride4 andvertexshaderconstants instead. No evidence these low/medium draws contain CPU XYZRHW preprojected vertices.

ComputeLOD inTTerrainInterfaceRVA7470 uses distance-to-camera squared and staticLODthresholds, mountainbounds,waterheight andsectionheight; returnsLOD0..3. The r2ghidra output on this function has imprecise x87/local types, so its exact shoreline formulas are not trustworthy withoutdisassembly. No directprojectionfar/fognear/fogfar reference observed inthatfunction.

### Native terrain cache budgets — follow-up
Confirmed non-highend TTerrainInterface::Create pool budgets from disassembly: mesh900(globalRVA1E380, immediatewriteRVA8A34), strata400(global1E384),shadow400(global1E388),grass350(global1E38C). Create non-highend immediatesRVA8A11/8A16/8A1B all400; instance+34/+38 receivesstrata/shadow limitsat8A3E/8A43. Highend alternatebranchRVA8F99 usesstrata/shadow1 andmesh3969(0xF81) at8FBC. These globals resideBSS, initializedbycode; on-diskreadcannotshowlivevalues.

GetFreeMeshRVA A7F0 pullsfree listinstance+90, orreclaimssection frominactive lists+ A0/A8 usingDeactivateTerrainSectionMeshA6B0; ifnoneavailable returnsNULL. OnlyindexedcallerActivateTerrainSection9900 callsat9C5E,return9C63. A NULL branchesat9C6E toA204: deactivatescurrentsectionmesh, incrementsvertexcachetimestamp,setsinstance+0xC=1. EarlierinActivate at99B4 checksinstance+0xC andifnonzero returnsexistingsection immediatelywithoutcreatingmissingmesh. Meshes allocatedonceinCreate: arrayinstance+C0,countglobal1E380,eachrecord20bytes,associatedHALmeshrecord+C. Do notraiseglobalmeshcountafterallocation: iterationwouldrunbeyondexistingarray.

Strata/shadow cache buffers differ: dynamicallyallocateduntilinstancecaps+34/+38. CacheStrataBufferA8A0 comparescountinstance+60 tolimit+34 atA9C5; undercapallocates0x134bytes,aboveevictsoldowner viaA660andreuses. EvictioncounterglobalRVA12524 incrementsatA9FA. CacheShadowBufferAA50 comparescount+64 tolimit+38 atAB75; undercapallocates0x254bytes. ThereforeliveincreaseINSTANCE+34/+38 to4096canpermitadditionalallocationswithoutchangingfixedmesharray; inspectmemoryallocationandsubsequentmeshNULLbeforejudgingresult. Restoringcapsbelowcurrentallocated/activecountmayevictaggressively; restoreaftertestonlywithunderstandingofpoolstate.

Suggestedliveevidence: countGetFreeMeshNULLatreturn9C63; perframeterrain+0xC; activeMeshCountgetterC2F0 returnsinstance+8; countinstance+60/+64; cachelimit+34/+38; strataevictioncounterRVA12524. Compare1046visiblesectionsagainstpoolbudgets, butdo notassumeeveryvisibleoceansectionneedsmesh. Sampleactualsectionmeshpointer+2C before/afteractivation; strata+24,shadow+28. No cache-sizeINIentriesfound inConstant.ini.

### Allocation-consistent candidate patch — 4096 cache entries
Parent live evidence: GetFreeMesh returnedNULLin8/8samples atcallerRVA9C63, active strata/shadow400andlivecacheglobals900/400/400/350. This confirms actual mesh exhaustion, not merelyvisiblecount exceedingnominalcapacity.

ExacttargetTTerrainInterface.dll SHA2564961df57871f539525b95c41dc72001414e4acb23569c8df3c958e155e50e3a3. ImageBase10000000, SizeOfImage29000. InthisbinaryRVA=fileoffsetforlistedcode.

|Immediate RVA/file offset|Originalbytes|Replacementbytes|Purpose|
|---|---|---|---|
|0x8A12|90 01 00 00|00 10 00 00|mov edx,4096 =>instancestratalimit|
|0x8A17|90 01 00 00|00 10 00 00|mov eax,4096 =>globalstrata/shadowlimit|
|0x8A1C|90 01 00 00|00 10 00 00|mov edi,4096 =>instanceshadowlimit|
|0x8A3A|84 03 00 00|00 10 00 00|globalmeshcount900=>4096|

Checkinstructionopcodes/signaturebeforepatch: RVA8A11 sequenceBA90010000 B890010000 BF90010000 A384E30110 A388E30110 C7058CE301105E010000 C70580E3011084030000. FourimmediatesmustchangebeforeCreatebeginsallocation. Grass350andhighendalternatepathunchanged. Highendalreadyuses3969meshes/1strata/1shadow.

Allocationconsistency: CreateRVA8B5F readsmodifiedglobalmeshcount andtmalloc(count*20+4),storesarraycookiecount,constructseach20byterecord;8BF1..8C85 createsoneHALmeshperentryusingsameglobalcount. Destructor9030 loops900/4096fromsameglobalat9049/907B. Thereforeinitpatchmaintainsmatchingallocation/constructor/destructorcounts. Do notpatchglobalcountpostCreate.

MeshCreatecallvirtual+30 receives(0x12,0x121,0x30) at8C50..8C59. ActualvertexpoolallocationcostdependsqualityandHALimplementation; fixedrecordoverheadat4096is81924bytes. Lazystratamax4096*0x134=1261568bytes;lazyshadowmax4096*0x254=2441216bytes,excludingallocatoroverhead. VRAM/vertexpoolcapacitystillneedsliveallocationanddrawvalidation.4096entriesis acandidatebudget,notproofthatallpossibleviewsfit: section+2C is ameshchainandmayneedmultiplemeshespersection. ValidateGetFreeMeshNULLcount0inaffectedviewsafterreload,fullterrainvisible,gameconsole/logandresourceallocfailures,andnativepathwithoutRemix.

### Live diagnosis and candidate installation - 2026-10-09

Live PID 13492: native main camera near=1, far=7000. Actual D3D8 depth surface format=80 (D16), 2560x1440. Temporary D24S8, near=10, and CPU ComparePlane/intersects tests were restored. Weather changed to heatwave during tests, browning water; screenshots appearing filled cannot prove those tests fixed terrain.

Definitive resource evidence: terrain mode has mesh budget900, material/strata400, shadow400, grass350. Both instance material/shadow counters reached400. GetFreeMesh returnedNULL for all8sampled attempts, caller TTerrainInterface+9C63. Sorted visible sections exceeded1000. Native activation handles allocation failure by skipping uncached terrain sections. This is a confirmed failure path; cache expansion still requires live proof that it resolves all holes.

MaCc camera block matches installed Constant.ini. TerraFormLakeDeformRatio differs (installed1.5, savedoriginal3.1), but that is not established as cause of renderer allocation failure. Native fog fractions do scale by camera renderer distance; fog linkage does not establish fog as cause of omitted geometry.

Built four-immediate candidate via build_cache_patch.py: non-highend mesh/material/shadow budgets4096. Grass and highend unchanged. Exact original SHA2564961df57871f539525b95c41dc72001414e4acb23569c8df3c958e155e50e3a3; patched SHA256c7a521b4be39b4ddc399bfb3ce24c4e42a8ac191985fc88e1d8c224f2cf7cab1. Verified PE length, architecture, entrypoint, original immediate values and allchangedbyteswithin4intfields.

User requested keeping extended distance, closed game for install. Installed patched TTerrainInterface.dll and verified hash. Backup: backups/JPOG/2026-10-09_124036_native-terrain-cache/TTerrainInterface.dll. Constant.ini/Options.ini unchanged by agent. Awaiting relaunch for actualpoolcounts, allocationfailures and visualverification.

### Remaining grass coverage — follow-up
Grass uses its own350-entrypool. Eachsection owns at mostonegrassrecord via section+0x3C; record20bytes: next+0,previous+4,HALgrassmesh+8,ownersection+0xC,frame/timestamp+0x10. TerrainLRUlisthead/tail+0xB8/+0xBC; grassrecordarray+C4; globalgrasscountRVA1E38C.

Minimalcandidate creation patch: nonhighend initializer atRVA/fileoffset8A2A isC7 05 8C E3 01 10 5E 01 00 00. Modifyonly4-byteimmediateRVA/fileoffset8A30 from5E010000(350) to00100000(4096). MustapplybeforeCreate,notliveglobalcountafterallocation. Highend initializer atRVA8FB2 independentlywrites350andremainsunchangedunlessseparatelyauthorized.

Consistencyverified: Create8BAC allocatescount*20; constructloop8BCD..8BE7; HALgrassmeshinitialization8CEE..8D6F reads sameglobalcount,createsoneHALmeshperentryviagrassshader virtual+0x44,thenmeshvirtual+0x2C with(0,zeroPosition). Destructor908A..90C6 reads sameglobalcountanddestroysentry+8;resource recoveryBC80/BDA0 alsoiterate sameglobal. Increasingcreationimmediatekeepsallarraysandloopsconsistent. Poolmetadata4096*20=81920bytes;actualgrassgeometryallocationcostdependson laterHALmeshpreparationandpopulation.

Activation: existingsectiongrass checkedat9B39. Missingrecord pathA320firsttestsdistance-to-cameraagainstgrassshaderfadefar(vtable+0x94) squared atA3B5..A3D1,then grass enabled(vtable+0x9C). PooltailretrievalA3F0..A3FC;A402comparestailrecord+0x10againstterrain+4 currentframe stamp. Ifequal,allrecordsarealreadyusedthisframe,skipgrassviaA404->9B44. Ifdifferent,reassignstailrecord,clearsoldownersection+3C,setsnewownersection+3C atA40A..A41B,stampscurrentframe,movestohead. This is grasspool exhaustion withoutNULLreturn/GetFreeGrass export.

Suggestedliveevidence: hookA402readEDI(tailstamp),EBX(currentstamp),EDX(record),EBP(section),ESI(terrain); equalitymeanscachecapacityskip. Countattemptedgrasssections versusrecordstamp occupancy. NativeGetGrassMeshgetterRVA5FC0readssection+3C'sHALpointer; noactivegrasscountgetterfound. Confirmremaininggrassissuewiththesehitsbeforeclaiminggrassbudgetfix. Initialgrasspoolincreasepreservesexistinggameplay,density,andfade distances; itchangesavailablecachedsectioncapacityonly.

### Final grass patch and live validation - 2026-10-09

User restarted after grass extension. Installed SHA256ab112afdb3863ee47a90419250c89e71d6b486b995f5ee36cda2a84dcdfb53fd (five constants). LivePID19532, TTerrainInterface base01610000. All four exported pool budgets4096. Terrain039C2CE0 instance material/shadow capacities4096, active3969each. resource_snapshot.py enumerated63x63=3969sections, all3969withgrass, all3969withstrata, all3969withshadow. Camera0364FB00 near1,far7000. Screenshottraces/culling_final.png shows continuous terrain and grass; no large rectangular missing patches in captured view. Test is live withRemix; nativewithoutRemix rerun notperformed, though allocationfailure andfixarenative.

Constant.ini SHA2560cd86a3da855d26648c84a541116a5e86dfb19b183a27587017851bf622dda75 matchesprepatchbackup. No permanentcamera/fog/depth/CPUculling override, ASI/proxy/runtime change. Larger pools requireadditionalmemory; noprofilerbenchmark. OriginalDLLrestore_script provided. No commits/pushes made.
