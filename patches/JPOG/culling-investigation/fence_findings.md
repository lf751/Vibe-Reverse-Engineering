## Native fence visibility — 2026-10-09

### Summary
The terrain cache patch changes TTerrainInterface capacity initialization only. Native fences have an independent manager and camera-dependent 512x512 wall scan window. Huge fade distances can move that window thousands of units forward, leaving nearby fences outside the scanned area. This is a concrete candidate requiring live confirmation; do not attribute missing fences to terrain capacity changes from timing alone.

### Native addresses and layout
- simjp005F2730 AWallSceneNode::Render calls005ED3E0 AWallManager::RenderScene through managerglobal8500A8.
- SingletonGetWallManager00469C70 returnsglobal850468.
- PublicRender005F21C0 onlystoresplanecountmanager+598; actualsubmissionoccursRenderScene.
- CullQuadTree005EF870; FillCache005EF2B0; renderlistplanechecks005F1140; visiblewallsubmission005F05E0; modelrender005EDB80.
- Cachedcellcoordinatesget/set005F1FF0/005F1FD0.
- Wallcacheentriespointermanager+F8;256entrieseach48bytes. cellindex=(cellX&15)+(cellZ&15)*16 with32unitcells. Entry+20processedthisframe,+24visibilitystate,+28/+2Cactualcellcoords. Cachecoordinatecheckavoidsstale reuse, butprocessedflagpreventsrepresentingmultiplealiasedcellsinoneframe.
- Cacheitemallocator005F1050 usesglobalfreeitemlist8B29D0/head8B29D4 andfreecount8B29DC; ifcount<=0itreturnswithoutaddingwall.

### Exact window computation
RenderScene obtains worldplanes into manager+538, negates all six, then replaces plane5(manager+588..594) using AFadeDist::far[type1] global83D4CC plusoffset840C44(nativeon-disk-4).
GetFadeFar004661F0 reads83D4C8+type*4;GetFadeNear00466200 reads83D4A0+type*4.83D4CCthereforedefinitelyfar[type1]; type1likelybuildingsbasedorderbutmustconfirm againstliveconfigvalues.

Forwardwindowoffset = ctx+1C0 + 0.5*(min(global83D4CC,ctx+1D0)-ctx+1C0).
Camera forwardvectorfromviewworldmatrix+20,+24,+28 ismultipliedbythisoffset then addedcamera worldposition. WindowcenterX/Z istruncatedtointand32unitaligned(maskFFFFFFE0).

005ED6AE callsCullQuadTree(centerX,centerZ,256,256,planes,mask),return005ED6B3. EntryargsESP+4=centerX,+8=centerZ,+C=halfWidth,+10=halfDepth,+14=planes,+18=mask.512x512windowmatches16x16cells. Enlarging256halfextentsalonewouldcausecachealias/processedflagissues; avoidthatpatch.

### Suggestedlive tests
Read83D4CC,840C44,ctx+1C0/+1D0 andfirstCullQuadTree invocationfrom005ED6B3. Ifcenter isfarfromparkfences,checkwindowcausalitybytemporarilyoverriding83D4CC to480onlyduringRenderScene(entrysavevalue/write480;onLeaverestoreoriginal). This keepsglobalfadevalueunchangedbetweenframesandtestswindow/planechange. Confirmfencesreturninvisibleaffectedviewbeforeclaimingcause.

SeparatelycountRenderScene,FillCache,renderlistsubmissionandRenderModelhits;inspectfreeitemcount8B29DC andwallmanager+500currentprocessedcellcount. IfRenderModelstillrunsforinvisiblefences,investigateGPUdraw/state/depthinstead. Nodescarrypositionfloats+0C/+10/+14,type+24,direction+28;RenderModelbuildsworldmatrixat8502E0andcallsmodelvirtual+2Cwithwallscene.

No source/game/binary edits performed by static analyst; findings only.

### Follow-up — near-array and draw gating
RenderScene005ED3E0disassembly has noearly distance/neararray gatebeforeCullQuadTree005ED6AE: itcopiesworldplanes,negatesall6,replacesplane5,resetsprocessedflags,computeswindow,prepares18wallmodels,andunconditionallycallsCullQuadTree. No83D4A0neararrayread occursinthisfunction. A scopedfar480 testisthereforenot directly invalidated by neararray10000 at this level; howeverunderlyingmodelrenderfade maystilluse thefadearrays.

GetWallCount00469C00readsmanager+5C8. Verify nonzero firstbeforefurthercullingpatches. Manager rootwalllinkedlisthead+4; firstnode listpointerat+4,positions node+0C/+10/+14,type+24,direction+28. RenderModel005EDB80usesmodelinfo(manager+B0+type*4),getsinstance+3C,updatesmatrix8502E0usingnodeposition/direction,then invokesmodelvirtual+2C at005EDCC2 withscene850464. It doesnotreadfadearraysdirectly.

LivefeedbackreportedRenderScenehits butno model/submissionhits,freecacheitemcount2879,scopedfar480Cullcenter576,576 half256. These resultsdoNOTyetconfirmwindowoffsetasthecause. Inspectactualregisteredwallpositionsandcountagainstwindow[320,832]X/Z; sampleFillCache005EF2B0hits,per-entryrenderlistcount+0C,manager+500cachecells,5F1140maskandplane rejects. IfFillCacheadds0nodesdespitewalllistnonzeroandwallpositionsinsidewindow,traceitscoordfilters andrenderlistallocation path. If5F1140rejectsnodes,temporarilyforcemask0onlyforthatfunction(retainswindow) todiscriminatewallplane rejectionfromwindow/cache.

AFadeDisttype1mapping remainsunresolvedfromexports; onlyfararrayaddressanduseareconfirmed. Do notlabelitWallFadeorBuildingFadeasafactwithoutloader evidence.

### Confirmed ABI and wall list traversal
Current installed simjp.exe SHA256: `8343acf132795b58eee21949268f135a9aab900b1d05cb394767c43c08f00003`.
`0x005ED3E0` bytes: `57 56 55 53 81 EC 8C 00 00 00 8B D9`.
Cull call at `0x005ED6AE`: `E8 BD 21 00 00`, targeting `0x005EF870`.

`0x005F1140` is a **RenderList thiscall**, not a manager function with four explicit arguments. ECX is the render-list entry. Entry stack: +4 planes, +8 plane count, +C mask. Its return is `RET 0xC`. Setting the entry stack mask at +C to zero makes a nonempty list call `0x005F0C10(1)`, bypassing the per-wall sphere-plane tests while retaining normal submission. The list count is ECX+0C; zero count exits without rendering.

Root wall list sentinel is the address manager+4, not the pointer stored there. Begin at `*(manager+4)`, follow node+0, and stop at node==manager+4. Node+4 is the reverse link. Position floats are +0C/+10/+14; wall type +24, direction +28. A render-cache list has first/reverse pointers at +4/+8; each item carries its wall-node pointer at +0C and follows +4 to the next cache item. Do not confuse these two list layouts.

The per-wall rejection condition is `dot(plane.normal, wall.position) - plane.D < -modelRadius`; radius comes from modelInfo(type)+34 -> +34 -> float+10. Mask selects which of planeCount planes are tested. This function returns void; visibility is passed to `0x005F05E0`, and a completed nonempty list sets visibility state at list+24 to 2.

Live parent reports 204 registered walls and 3091 free cache items after relaunch. Those observations eliminate absent save registration and instantaneous wall-item pool exhaustion as explanations, but do not confirm cached walls lie inside the selected scan window.

### Wall fade mapping confirmed
AFadeDist type 1 is **Wall**, established through the native Constant.ini loader and quality mapper.
Loader `0x0046E892..0x0046E8E4` iterates parallel tables: name pointers `0x0083C640 + byteOffset`, parsed float storage `0x008C49A0 + byteOffset`.
Name slot `0x0083D0B0` points to `HIGH_WallFadeNearDistance` at `0x00CECC40`; its parsed float is `0x008C5410`. Next name slot `0x0083D0B4` points to `HIGH_WallFadeFarDistance` at `0x00CECC60`; parsed float is `0x008C5414`.
High-quality mapping at `0x0046EDAB` loads the near constant at `0x0046EDB6`, then stores it to `0x0083D4A4` at `0x0046EDD2`; it loads the far constant at `0x0046EDC1`, then stores it to `0x0083D4CC` at `0x0046EDD8`.
Thus the wall arrays are near[type1] = `0x0083D4A4`, far[type1] = `0x0083D4CC`. The earlier tentative Building label is superseded.

Minimal external-memory diagnostic: preserve both floats, write wall near=380 and far=410 to these two runtime addresses, then capture the same close-up view. This uses the user's saved original HIGH wall settings, changes the native wall scan window and its far plane, and needs no Frida function hook. If quality settings are reapplied during the test, also update/preserve the underlying HIGH parsed constants at `0x008C5410` and `0x008C5414`.

### Extended wall rendering design after confirmed live result
The user confirmed that restoring only HIGH_WallFadeNear/Far to 380/410 restores fences with the terrain cache patch still installed. The wall window is now confirmed as the relevant independent limitation; terrain capacity changes are not required to explain this disappearance.

A heap cache capacity increase alone is unsafe. Create at 0x005E3760 allocates 0x3004 bytes, including a four-byte header and 256 render lists of 48 bytes. CullQuadTree indexes `(cellX & 15) + (cellZ & 15)*16`, with 32-unit cells. RenderScene embeds two processed-index arrays inside the manager: +FC and +2FA, stride 0x1FE, selector +4F8, previous/current counts +4FC/+500. Index recording beyond these arrays overwrites other manager fields. Increasing half extents or the heap allocation alone cannot extend coverage safely.

Minimal cohesive per-game ASI design: replace only the call at 0x005ED6AE with an extended collection wrapper of the same thiscall ABI (manager ECX, four float window parameters, planes, mask; callee pops 24 bytes). Preserve native RenderScene plane generation, including the HIGH wall far plane, and all model preparation and finish calls around this call site. The wrapper collects the actual terrain bounds into a dedicated scratch RenderList using native FillCache, then uses native RenderList::Render with the original planes, plane count 6, and mask. This keeps per-wall sphere/frustum tests and native post, highlight, and model submission behavior; only the undersized spatial collection window is replaced.

FillCache 0x005EF2B0: manager ECX; explicit arguments list, minX, minZ, maxX, maxZ (unsigned integer bounds). It begins by calling ClearRenderList 0x005F10C0 on the supplied list. Bounds should be [0,width) and [0,depth), using the same heightfield dimensions native CullQuadTree reads. RenderList::Render 0x005F1140: list ECX, planes, count, mask; callee pops 12 bytes.

Scratch RenderList initialization mirrors native Create: zero all 48 bytes; list+4 and +8 point to list; list+14 and +18 point to list+10; list+24=1. List count +C starts at zero. ClearRenderList returns its allocated items to the global free pool and manages visibility before rebuilding; FillCache already invokes it each frame. Clear the scratch list while wall nodes and manager remain alive when unloading or replacing the park.

On transition from the original collection path, clear all 256 old render lists once and reset manager counts +4FC/+500 to zero. Otherwise the native RenderScene tail at 0x005ED6D6..0x005ED750 can mark walls invisible through previous cache lists after the extended wrapper has submitted them. Normal startup installation before any native wall frame avoids existing populated lists, but the lifecycle must still be explicit.

This design needs live validation and performance measurement before shipping. Per-frame full-bound collection walks native indexed wall/post lists and rebuilds one render list. Existing global item-pool capacity is unchanged: 204 walls are below the observed 3091 free items, but a much larger park can still exhaust that pool. Verify collected count and free count rather than assuming unlimited walls. The design avoids changing gameplay wall limits and avoids fragile expansion of the manager's embedded arrays.
