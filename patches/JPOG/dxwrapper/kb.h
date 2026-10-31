// DxWrapper 1.9.8700.25, preferred PE base 0x10000000.
// Exact pinned d3d8to9 source and binary disassembly identify this entry.
@ 0x10154710 long __stdcall Direct3DDevice8_SetVertexShader(void* device, unsigned long Handle);
// [sigdb: byte, 0.70] crt
@ 0x100CD770 crt_xmatch_0040D1A0;

// [sigdb: byte, 0.70] crt
@ 0x100CD780 crt_xmatch_0040D1A0_100CD780;

// [sigdb: byte, 0.70] crt
@ 0x100CD790 crt_xmatch_0040D1A0_100CD790;

// [sigdb: byte, 0.75] crt
@ 0x10164490 crt_xmatch_0040D1A0_10164490;

// [sigdb: byte, 0.70] crt
@ 0x1018F110 crt_xmatch_00EEBD40;

// [sigdb: byte, 0.70] crt
@ 0x101AA7A0 crt_xmatch_00EF5C90;

// [sigdb: byte, 0.70] crt
@ 0x101B2CC0 crt_xmatch_00EF3040;

// [sigdb: byte, 0.70] crt
@ 0x101B2D05 crt_xmatch_00EF3085;

// [sigdb: byte, 0.70] crt
@ 0x101B2D48 crt_xmatch_00EF30C8;

// [sigdb: byte, 0.70] crt
@ 0x101B2D5E crt_xmatch_00EF30DE;

// [sigdb: byte, 0.70] crt
@ 0x101B2DA9 crt_xmatch_00EF3129;

// [sigdb: byte, 0.85] crt
@ 0x101B2E50 crt_xmatch_00EF2FE0;

// [sigdb: byte, 0.85] crt
@ 0x101B2E67 crt_xmatch_00EF2FF7;

// [sigdb: byte, 0.95] crt
@ 0x101B3430 crt_xmatch_00EECD00;

// [sigdb: byte, 0.95] crt
@ 0x101B3510 crt_xmatch_00EECC20;

// [sigdb: byte, 0.70] crt
@ 0x101B36A0 crt_xmatch_00EED8C0;

// [sigdb: byte, 0.70] crt
@ 0x101B471D crt_xmatch_00EECE8D;

// [sigdb: byte, 0.75] crt
@ 0x101B4A10 crt_xmatch_00EF43D0;

// [sigdb: byte, 0.70] crt
@ 0x101B51FE crt_xmatch_00EF4BF1;

// [rtti] .?AVexception@std@@
// hierarchy: .?AVexception@std@@
@ 0x101CA194 exception_std_vtable;

// [rtti] .?AVbad_alloc@std@@
// hierarchy: .?AVbad_alloc@std@@ -> .?AVexception@std@@
@ 0x101CA1A0 bad_alloc_std_vtable;

// [rtti] .?AVbad_function_call@std@@
// hierarchy: .?AVbad_function_call@std@@ -> .?AVexception@std@@
@ 0x101CA1AC bad_function_call_std_vtable;

// [rtti] .?AVlogic_error@std@@
// hierarchy: .?AVlogic_error@std@@ -> .?AVexception@std@@
@ 0x101CA1CC logic_error_std_vtable;

// [rtti] .?AVinvalid_argument@std@@
// hierarchy: .?AVinvalid_argument@std@@ -> .?AVlogic_error@std@@ -> .?AVexception@std@@
@ 0x101CA1D8 invalid_argument_std_vtable;

// [rtti] .?AVlength_error@std@@
// hierarchy: .?AVlength_error@std@@ -> .?AVlogic_error@std@@ -> .?AVexception@std@@
@ 0x101CA1E4 length_error_std_vtable;

// [rtti] .?AVout_of_range@std@@
// hierarchy: .?AVout_of_range@std@@ -> .?AVlogic_error@std@@ -> .?AVexception@std@@
@ 0x101CA1F0 out_of_range_std_vtable;

// [rtti] .?AVruntime_error@std@@
// hierarchy: .?AVruntime_error@std@@ -> .?AVexception@std@@
@ 0x101CA1FC runtime_error_std_vtable;

// [rtti] .?AVregex_error@std@@
// hierarchy: .?AVregex_error@std@@ -> .?AVruntime_error@std@@ -> .?AVexception@std@@
@ 0x101CA208 regex_error_std_vtable;

// [rtti] .?AV_Facet_base@std@@
// hierarchy: .?AV_Facet_base@std@@
@ 0x101CA74C _Facet_base_std_vtable;

// [rtti] .?AV_Locimp@locale@std@@
// hierarchy: .?AV_Locimp@locale@std@@ -> .?AVfacet@locale@std@@ -> .?AV_Facet_base@std@@ -> .?AU_Crt_new_delete@std@@
@ 0x101CA75C _Locimp_locale_std_vtable;

// [rtti] .?AVbad_array_new_length@std@@
// hierarchy: .?AVbad_array_new_length@std@@ -> .?AVbad_alloc@std@@ -> .?AVexception@std@@
@ 0x101CD9E8 bad_array_new_length_std_vtable;

// [rtti] .?AVbad_cast@std@@
// hierarchy: .?AVbad_cast@std@@ -> .?AVexception@std@@
@ 0x101CD9F4 bad_cast_std_vtable;

// [rtti] .?AV?$ctype@D@std@@
// hierarchy: .?AV?$ctype@D@std@@ -> .?AUctype_base@std@@ -> .?AVfacet@locale@std@@ -> .?AV_Facet_base@std@@ -> .?AU_Crt_new_delete@std@@
@ 0x101CDA00 _ctype_D_std_vtable;

// [rtti] .?AV?$num_put@DV?$ostreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@
// hierarchy: .?AV?$num_put@DV?$ostreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@ -> .?AVfacet@locale@std@@ -> .?AV_Facet_base@std@@ -> .?AU_Crt_new_delete@std@@
@ 0x101CDA30 _num_put_DV__ostreambuf_iterator_DU__char_traits_D_std___std___std_vtable;

// [rtti] .?AV?$numpunct@D@std@@
// hierarchy: .?AV?$numpunct@D@std@@ -> .?AVfacet@locale@std@@ -> .?AV_Facet_base@std@@ -> .?AU_Crt_new_delete@std@@
@ 0x101CDA60 _numpunct_D_std_vtable;

// [rtti] .?AV?$codecvt@DDU_Mbstatet@@@std@@
// hierarchy: .?AV?$codecvt@DDU_Mbstatet@@@std@@ -> .?AVcodecvt_base@std@@ -> .?AVfacet@locale@std@@ -> .?AV_Facet_base@std@@ -> .?AU_Crt_new_delete@std@@
@ 0x101CDA84 _codecvt_DDU_Mbstatet___std_vtable;

// [rtti] .?AV_System_error@std@@
// hierarchy: .?AV_System_error@std@@ -> .?AVruntime_error@std@@ -> .?AVexception@std@@
@ 0x101CDBC4 _System_error_std_vtable;

// [rtti] .?AVsystem_error@std@@
// hierarchy: .?AVsystem_error@std@@ -> .?AV_System_error@std@@ -> .?AVruntime_error@std@@ -> .?AVexception@std@@
@ 0x101CDBD0 system_error_std_vtable;

// [rtti] .?AV_Iostream_error_category2@std@@
// hierarchy: .?AV_Iostream_error_category2@std@@ -> .?AVerror_category@std@@
@ 0x101CDBDC _Iostream_error_category2_std_vtable;

// [rtti] .?AV?$ctype@_W@std@@
// hierarchy: .?AV?$ctype@_W@std@@ -> .?AUctype_base@std@@ -> .?AVfacet@locale@std@@ -> .?AV_Facet_base@std@@ -> .?AU_Crt_new_delete@std@@
@ 0x101CDBF8 _ctype__W_std_vtable;

// [rtti] .?AVfailure@ios_base@std@@
// hierarchy: .?AVfailure@ios_base@std@@ -> .?AVsystem_error@std@@ -> .?AV_System_error@std@@ -> .?AVruntime_error@std@@ -> .?AVexception@std@@
@ 0x101CDC38 failure_ios_base_std_vtable;

// [rtti] .?AV?$num_put@_WV?$ostreambuf_iterator@_WU?$char_traits@_W@std@@@std@@@std@@
// hierarchy: .?AV?$num_put@_WV?$ostreambuf_iterator@_WU?$char_traits@_W@std@@@std@@@std@@ -> .?AVfacet@locale@std@@ -> .?AV_Facet_base@std@@ -> .?AU_Crt_new_delete@std@@
@ 0x101CDC44 _num_put__WV__ostreambuf_iterator__WU__char_traits__W_std___std___std_vtable;

// [rtti] .?AV?$numpunct@_W@std@@
// hierarchy: .?AV?$numpunct@_W@std@@ -> .?AVfacet@locale@std@@ -> .?AV_Facet_base@std@@ -> .?AU_Crt_new_delete@std@@
@ 0x101CDC74 _numpunct__W_std_vtable;

// [rtti] .?AV?$collate@D@std@@
// hierarchy: .?AV?$collate@D@std@@ -> .?AVfacet@locale@std@@ -> .?AV_Facet_base@std@@ -> .?AU_Crt_new_delete@std@@
@ 0x101CDCB4 _collate_D_std_vtable;

// [rtti] .?AVbad_exception@std@@
// hierarchy: .?AVbad_exception@std@@ -> .?AVexception@std@@
@ 0x101CDD90 bad_exception_std_vtable;

// [propagated: _thunk_sub_10190490, 0.80]
@ 0x10001710 _thunk_sub_10190490;

// [propagated: _thunk_sub_100019A0, 0.80]
@ 0x10001750 _thunk_sub_100019A0;

// [propagated: _thunk_sub_10001C50, 0.80]
@ 0x10001C30 _thunk_sub_10001C50;

// [propagated: _thunk_sub_10001A90, 0.80]
@ 0x10002050 _thunk_sub_10001A90;

// [propagated: _thunk_sub_10001BF0, 0.80]
@ 0x10002350 _thunk_sub_10001BF0;

// [propagated: _thunk_sub_100023D0, 0.80]
@ 0x100023C0 _thunk_sub_100023D0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1000281F _thunk_sub_1018DEB0;

// [propagated: _thunk_sub_10002830, 0.80]
@ 0x10002A10 _thunk_sub_10002830;

// [propagated: _thunk_sub_10008AD0, 0.80]
@ 0x10008B10 _thunk_sub_10008AD0;

// [propagated: _thunk_sub_1018F7EF, 0.80]
@ 0x10008B30 _thunk_sub_1018F7EF;

// [propagated: _thunk_sub_1018F7EF, 0.80]
@ 0x10008B70 _thunk_sub_1018F7EF_10008B70;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x10008CC0 _thunk_sub_1018B1F4;

// [propagated: _thunk_sub_1000A1E0, 0.80]
@ 0x100098F0 _thunk_sub_1000A1E0;

// [propagated: _thunk_sub_1018FF10, 0.80]
@ 0x10009C4A _thunk_sub_1018FF10;

// [propagated: _thunk_sub_10008AD0, 0.80]
@ 0x1000A1C0 _thunk_sub_10008AD0_1000A1C0;

// [propagated: _thunk_sub_101685C0, 0.80]
@ 0x1000A290 _thunk_sub_101685C0;

// [propagated: _thunk_sub_10197930, 0.80]
@ 0x1000A2C0 _thunk_sub_10197930;

// [propagated: _thunk_sub_1000B3C0, 0.80]
@ 0x1000A2F0 _thunk_sub_1000B3C0;

// [propagated: _thunk_sub_1000A650, 0.80]
@ 0x1000A3D0 _thunk_sub_1000A650;

// [propagated: _thunk_sub_1000B530, 0.80]
@ 0x1000A890 _thunk_sub_1000B530;

// [propagated: _thunk_sub_100096F0, 0.80]
@ 0x1000B780 _thunk_sub_100096F0;

// [propagated: _thunk_sub_100096F0, 0.80]
@ 0x1000C480 _thunk_sub_100096F0_1000C480;

// [propagated: _thunk_sub_1018B214, 0.80]
@ 0x1000C7C0 _thunk_sub_1018B214;

// [propagated: _thunk_sub_1000B360, 0.80]
@ 0x1000CAF0 _thunk_sub_1000B360;

// [propagated: _thunk_sub_101B1BF0, 0.80]
@ 0x1000CBF0 _thunk_sub_101B1BF0;

// [propagated: _thunk_sub_100100B0, 0.80]
@ 0x1000DE4D _thunk_sub_100100B0;

// [propagated: _thunk_sub_100100B0, 0.80]
@ 0x1000E1EC _thunk_sub_100100B0_1000E1EC;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x1000FE70 _thunk_sub_1018DEF3;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x10010620 _thunk_sub_1018DEF3_10010620;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x10010640 _thunk_sub_1018DEF3_10010640;

// [propagated: _thunk_sub_1000A1E0, 0.80]
@ 0x10010E69 _thunk_sub_1000A1E0_10010E69;

// [propagated: _thunk_sub_1018A4C0, 0.80]
@ 0x10011153 _thunk_sub_1018A4C0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1001181C _thunk_sub_1018DEB0_1001181C;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10011AA9 _thunk_sub_1018DEB0_10011AA9;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10011E1B _thunk_sub_1018DEB0_10011E1B;

// [propagated: _thunk_sub_1018B957, 0.80]
@ 0x10014140 _thunk_sub_1018B957;

// [propagated: _thunk_sub_10198600, 0.80]
@ 0x10014750 _thunk_sub_10198600;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x100166C0 _thunk_sub_10019B20;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1001832C _thunk_sub_1018DEB0_1001832C;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100184A7 _thunk_sub_1018DEB0_100184A7;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100184A9 _thunk_sub_1018DEB0_100184A9;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10019AF3 _thunk_sub_1018DEB0_10019AF3;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x10019B20 _thunk_sub_1018DEF3_10019B20;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x1001BA10 _thunk_sub_1018B1F4_1001BA10;

// [propagated: _thunk_sub_1001BDF0, 0.80]
@ 0x1001BE20 _thunk_sub_1001BDF0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10022F92 _thunk_sub_1018DEB0_10022F92;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100240D7 _thunk_sub_1018DEB0_100240D7;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100240E8 _thunk_sub_1018DEB0_100240E8;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1002587E _thunk_sub_1018DEB0_1002587E;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10025886 _thunk_sub_1018DEB0_10025886;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10025AF9 _thunk_sub_1018DEB0_10025AF9;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10025F02 _thunk_sub_1018DEB0_10025F02;

// [propagated: _thunk_sub_10190490, 0.80]
@ 0x10027970 _thunk_sub_10190490_10027970;

// [propagated: _thunk_sub_10010540, 0.80]
@ 0x10027D40 _thunk_sub_10010540;

// [propagated: _thunk_sub_10010540, 0.80]
@ 0x10027E80 _thunk_sub_10010540_10027E80;

// [propagated: _thunk_sub_10010540, 0.80]
@ 0x10027FC0 _thunk_sub_10010540_10027FC0;

// [propagated: _thunk_sub_10010540, 0.80]
@ 0x100280F0 _thunk_sub_10010540_100280F0;

// [propagated: _thunk_sub_10010540, 0.80]
@ 0x10028230 _thunk_sub_10010540_10028230;

// [propagated: _thunk_sub_10190490, 0.80]
@ 0x100284A0 _thunk_sub_10190490_100284A0;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x100287D0 _thunk_sub_1018B1F4_100287D0;

// [propagated: _thunk_sub_1002A9A0, 0.80]
@ 0x10028E20 _thunk_sub_1002A9A0;

// [propagated: _thunk_sub_10029E30, 0.80]
@ 0x10029110 _thunk_sub_10029E30;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1002D346 _thunk_sub_1018DEB0_1002D346;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1002D34C _thunk_sub_1018DEB0_1002D34C;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1002DDCC _thunk_sub_1018DEB0_1002DDCC;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1002DDD2 _thunk_sub_1018DEB0_1002DDD2;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x1002DE80 _thunk_sub_1018B1F4_1002DE80;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1002E8D2 _thunk_sub_1018DEB0_1002E8D2;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1002EF04 _thunk_sub_1018DEB0_1002EF04;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1002F224 _thunk_sub_1018DEB0_1002F224;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x10031C00 _thunk_sub_1018DEF3_10031C00;

// [propagated: _thunk_sub_100324A0, 0.80]
@ 0x100325E7 _thunk_sub_100324A0;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x10033224 _thunk_sub_1018B1F4_10033224;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x100332C0 _thunk_sub_1018DEF3_100332C0;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x10033410 _thunk_sub_1018B1F4_10033410;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100337AF _thunk_sub_1018DEB0_100337AF;

// [propagated: _thunk_sub_10037740, 0.80]
@ 0x100376D6 _thunk_sub_10037740;

// [propagated: _thunk_sub_1000B3C0, 0.80]
@ 0x1003B260 _thunk_sub_1000B3C0_1003B260;

// [propagated: _thunk_sub_1000B3C0, 0.80]
@ 0x1003B2C0 _thunk_sub_1000B3C0_1003B2C0;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x10046590 _thunk_sub_1018DEF3_10046590;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x100465B0 _thunk_sub_1018DEF3_100465B0;

// [propagated: _thunk_sub_10049A90, 0.80]
@ 0x10049CE0 _thunk_sub_10049A90;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1004A4A8 _thunk_sub_1018DEB0_1004A4A8;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1004A838 _thunk_sub_1018DEB0_1004A838;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1004ABCE _thunk_sub_1018DEB0_1004ABCE;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1004AF5E _thunk_sub_1018DEB0_1004AF5E;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1004B41A _thunk_sub_1018DEB0_1004B41A;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1004CA6A _thunk_sub_1018DEB0_1004CA6A;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1004CE0A _thunk_sub_1018DEB0_1004CE0A;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1004DDDD _thunk_sub_1018DEB0_1004DDDD;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1004E7ED _thunk_sub_1018DEB0_1004E7ED;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1004F1ED _thunk_sub_1018DEB0_1004F1ED;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1004FBED _thunk_sub_1018DEB0_1004FBED;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100505ED _thunk_sub_1018DEB0_100505ED;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10050D8D _thunk_sub_1018DEB0_10050D8D;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100518E0 _thunk_sub_1018DEB0_100518E0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10051C00 _thunk_sub_1018DEB0_10051C00;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x1005237C _thunk_sub_1018B1F4_1005237C;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x10052410 _thunk_sub_1018DEF3_10052410;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x100524A0 _thunk_sub_1018B1F4_100524A0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1005285F _thunk_sub_1018DEB0_1005285F;

// [propagated: _thunk_sub_10052880, 0.80]
@ 0x10052970 _thunk_sub_10052880;

// [propagated: _thunk_sub_10065780, 0.80]
@ 0x10058E35 _thunk_sub_10065780;

// [propagated: _thunk_sub_10190490, 0.80]
@ 0x10058F40 _thunk_sub_10190490_10058F40;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10059593 _thunk_sub_1018DEB0_10059593;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x10059A71 _thunk_sub_1018B1F4_10059A71;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x10059B80 _thunk_sub_1018B1F4_10059B80;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10059F94 _thunk_sub_1018DEB0_10059F94;

// [propagated: _thunk_sub_1005AA60, 0.80]
@ 0x1005A610 _thunk_sub_1005AA60;

// [propagated: _thunk_sub_1005ABA0, 0.80]
@ 0x1005AAC0 _thunk_sub_1005ABA0;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x1005AB80 _thunk_sub_1018DEF3_1005AB80;

// [propagated: _thunk_sub_1005AC40, 0.80]
@ 0x1005AEC0 _thunk_sub_1005AC40;

// [propagated: _thunk_sub_1005B1F0, 0.80]
@ 0x1005AF00 _thunk_sub_1005B1F0;

// [propagated: _thunk_sub_10064B90, 0.80]
@ 0x10064D30 _thunk_sub_10064B90;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10065CD1 _thunk_sub_1018DEB0_10065CD1;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10065F91 _thunk_sub_1018DEB0_10065F91;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10066257 _thunk_sub_1018DEB0_10066257;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10066527 _thunk_sub_1018DEB0_10066527;

// [propagated: _thunk_sub_10008AD0, 0.80]
@ 0x100675C0 _thunk_sub_10008AD0_100675C0;

// [propagated: _thunk_sub_1000B3C0, 0.80]
@ 0x10067640 _thunk_sub_1000B3C0_10067640;

// [propagated: _thunk_sub_10067F60, 0.80]
@ 0x10067E80 _thunk_sub_10067F60;

// [propagated: _thunk_sub_10067F60, 0.80]
@ 0x10067EEB _thunk_sub_10067F60_10067EEB;

// [propagated: _thunk_sub_100144D0, 0.80]
@ 0x10068BD0 _thunk_sub_100144D0;

// [propagated: _thunk_sub_1000FDD0, 0.80]
@ 0x10069A00 _thunk_sub_1000FDD0;

// [propagated: _thunk_sub_1006A9B0, 0.80]
@ 0x10069B10 _thunk_sub_1006A9B0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1006A144 _thunk_sub_1018DEB0_1006A144;

// [propagated: _thunk_sub_10077800, 0.80]
@ 0x1006A300 _thunk_sub_10077800;

// [propagated: _thunk_sub_1018B1B7, 0.80]
@ 0x1006AA20 _thunk_sub_1018B1B7;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x1006B270 _thunk_sub_1018DEF3_1006B270;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x1006B290 _thunk_sub_1018DEF3_1006B290;

// [propagated: _thunk_sub_10190490, 0.80]
@ 0x100728A0 _thunk_sub_10190490_100728A0;

// [propagated: _thunk_sub_10074660, 0.80]
@ 0x10072A00 _thunk_sub_10074660;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x100732A0 _thunk_sub_1018DEF3_100732A0;

// [propagated: _thunk_sub_10075040, 0.80]
@ 0x10073D20 _thunk_sub_10075040;

// [propagated: _thunk_sub_10075040, 0.80]
@ 0x10073DD0 _thunk_sub_10075040_10073DD0;

// [propagated: _thunk_sub_10074660, 0.80]
@ 0x100743E8 _thunk_sub_10074660_100743E8;

// [propagated: _thunk_sub_10067360, 0.80]
@ 0x10075990 _thunk_sub_10067360;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x10076120 _thunk_sub_1018DEF3_10076120;

// [propagated: _thunk_sub_1001AEE0, 0.80]
@ 0x10076416 _thunk_sub_1001AEE0;

// [propagated: _thunk_sub_1001AEE0, 0.80]
@ 0x1007642B _thunk_sub_1001AEE0_1007642B;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10076600 _thunk_sub_1018DEB0_10076600;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10076C6D _thunk_sub_1018DEB0_10076C6D;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1007896E _thunk_sub_1018DEB0_1007896E;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10079AE5 _thunk_sub_1018DEB0_10079AE5;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10079E41 _thunk_sub_1018DEB0_10079E41;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10079E43 _thunk_sub_1018DEB0_10079E43;

// [propagated: _thunk_sub_10079B10, 0.80]
@ 0x10079E70 _thunk_sub_10079B10;

// [propagated: _thunk_sub_1002DE80, 0.80]
@ 0x10085567 _thunk_sub_1002DE80;

// [propagated: _thunk_sub_10068ED0, 0.80]
@ 0x10085870 _thunk_sub_10068ED0;

// [propagated: _thunk_sub_100868B0, 0.80]
@ 0x10086810 _thunk_sub_100868B0;

// [propagated: _thunk_sub_100868B0, 0.80]
@ 0x10086930 _thunk_sub_100868B0_10086930;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1008ADC0 _thunk_sub_1018DEB0_1008ADC0;

// [propagated: _thunk_sub_1008C540, 0.80]
@ 0x1008C4A0 _thunk_sub_1008C540;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1008C5F0 _thunk_sub_1018DEB0_1008C5F0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1008D58B _thunk_sub_1018DEB0_1008D58B;

// [propagated: _thunk_sub_1018DEC3, 0.80]
@ 0x1008D5B0 _thunk_sub_1018DEC3;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1008DC55 _thunk_sub_1018DEB0_1008DC55;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1008DC57 _thunk_sub_1018DEB0_1008DC57;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1008E3F5 _thunk_sub_1018DEB0_1008E3F5;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1008E3F7 _thunk_sub_1018DEB0_1008E3F7;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1008EB95 _thunk_sub_1018DEB0_1008EB95;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1008EB97 _thunk_sub_1018DEB0_1008EB97;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1008F063 _thunk_sub_1018DEB0_1008F063;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1008F34A _thunk_sub_1018DEB0_1008F34A;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1008F34C _thunk_sub_1018DEB0_1008F34C;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1008F800 _thunk_sub_1018DEB0_1008F800;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1008FAEA _thunk_sub_1018DEB0_1008FAEA;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1008FAEC _thunk_sub_1018DEB0_1008FAEC;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1009092E _thunk_sub_1018DEB0_1009092E;

// [propagated: _thunk_sub_10085190, 0.80]
@ 0x10090D40 _thunk_sub_10085190;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1009406E _thunk_sub_1018DEB0_1009406E;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100948BD _thunk_sub_1018DEB0_100948BD;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10097190 _thunk_sub_1018DEB0_10097190;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10097712 _thunk_sub_1018DEB0_10097712;

// [propagated: _thunk_sub_10097FA0, 0.80]
@ 0x10097F20 _thunk_sub_10097FA0;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x10097F80 _thunk_sub_1018DEF3_10097F80;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1009830A _thunk_sub_1018DEB0_1009830A;

// [propagated: _thunk_sub_10097060, 0.80]
@ 0x1009D17D _thunk_sub_10097060;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1009ED70 _thunk_sub_1018DEB0_1009ED70;

// [propagated: _thunk_sub_10190490, 0.80]
@ 0x100A0440 _thunk_sub_10190490_100A0440;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100A05C0 _thunk_sub_1018DEB0_100A05C0;

// [propagated: _thunk_sub_100A08E0, 0.80]
@ 0x100A0DE0 _thunk_sub_100A08E0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100A1484 _thunk_sub_1018DEB0_100A1484;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100A3463 _thunk_sub_1018DEB0_100A3463;

// [propagated: _thunk_sub_100A3DC0, 0.80]
@ 0x100A3490 _thunk_sub_100A3DC0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100A3C2E _thunk_sub_1018DEB0_100A3C2E;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100A4EE9 _thunk_sub_1018DEB0_100A4EE9;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100A4F68 _thunk_sub_1018DEB0_100A4F68;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100A5210 _thunk_sub_1018DEB0_100A5210;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100A5287 _thunk_sub_1018DEB0_100A5287;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100A5376 _thunk_sub_1018DEB0_100A5376;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x100A59D0 _thunk_sub_1018DEF3_100A59D0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100A5CE3 _thunk_sub_1018DEB0_100A5CE3;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100A5CE5 _thunk_sub_1018DEB0_100A5CE5;

// [propagated: _thunk_sub_100A6B10, 0.80]
@ 0x100A5F30 _thunk_sub_100A6B10;

// [propagated: _thunk_sub_100A6B10, 0.80]
@ 0x100A64B0 _thunk_sub_100A6B10_100A64B0;

// [propagated: _thunk_sub_100A82D0, 0.80]
@ 0x100A9B90 _thunk_sub_100A82D0;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x100AAC60 _thunk_sub_1018DEF3_100AAC60;

// [propagated: _thunk_sub_1000B360, 0.80]
@ 0x100ACD00 _thunk_sub_1000B360_100ACD00;

// [propagated: _thunk_sub_1018E630, 0.80]
@ 0x100B129F _thunk_sub_1018E630;

// [propagated: _thunk_sub_1000B360, 0.80]
@ 0x100B2540 _thunk_sub_1000B360_100B2540;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x100B38F6 _thunk_sub_1018DEF3_100B38F6;

// [propagated: _thunk_sub_1000FDD0, 0.80]
@ 0x100B4500 _thunk_sub_1000FDD0_100B4500;

// [propagated: _thunk_sub_1019AA10, 0.80]
@ 0x100B4550 _thunk_sub_1019AA10;

// [propagated: _thunk_sub_101931D7, 0.80]
@ 0x100B51EC _thunk_sub_101931D7;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100B90EA _thunk_sub_1018DEB0_100B90EA;

// [propagated: _thunk_sub_100C3400, 0.80]
@ 0x100B9DF0 _thunk_sub_100C3400;

// [propagated: _thunk_sub_1000A1E0, 0.80]
@ 0x100BB8A8 _thunk_sub_1000A1E0_100BB8A8;

// [propagated: _thunk_sub_1000A1E0, 0.80]
@ 0x100BBD36 _thunk_sub_1000A1E0_100BBD36;

// [propagated: _thunk_sub_100CEAD0, 0.80]
@ 0x100C1F88 _thunk_sub_100CEAD0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100C3198 _thunk_sub_1018DEB0_100C3198;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100C35F9 _thunk_sub_1018DEB0_100C35F9;

// [propagated: _thunk_sub_100D0980, 0.80]
@ 0x100C4980 _thunk_sub_100D0980;

// [propagated: _thunk_sub_100B3700, 0.80]
@ 0x100C536D _thunk_sub_100B3700;

// [propagated: _thunk_sub_1019AA10, 0.80]
@ 0x100C8194 _thunk_sub_1019AA10_100C8194;

// [propagated: _thunk_sub_100B3700, 0.80]
@ 0x100C9112 _thunk_sub_100B3700_100C9112;

// [propagated: _thunk_sub_100CA3E0, 0.80]
@ 0x100CA190 _thunk_sub_100CA3E0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100CC08D _thunk_sub_1018DEB0_100CC08D;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100CC662 _thunk_sub_1018DEB0_100CC662;

// [propagated: _thunk_sub_101685C0, 0.80]
@ 0x100CCBB0 _thunk_sub_101685C0_100CCBB0;

// [propagated: _thunk_sub_100B44D0, 0.80]
@ 0x100CCF60 _thunk_sub_100B44D0;

// [propagated: _thunk_sub_1019AA10, 0.80]
@ 0x100CD5C0 _thunk_sub_1019AA10_100CD5C0;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x100D009C _thunk_sub_1018B1F4_100D009C;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x100D0F30 _thunk_sub_1018DEF3_100D0F30;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x100D0F50 _thunk_sub_1018DEF3_100D0F50;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x100D0F70 _thunk_sub_1018DEF3_100D0F70;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x100D1DA0 _thunk_sub_1018DEF3_100D1DA0;

// [propagated: _thunk_sub_1019AA10, 0.80]
@ 0x100D1FD0 _thunk_sub_1019AA10_100D1FD0;

// [propagated: _thunk_sub_100CD6A0, 0.80]
@ 0x100D2089 _thunk_sub_100CD6A0;

// [propagated: _thunk_sub_100B3270, 0.80]
@ 0x100D66F0 _thunk_sub_100B3270;

// [propagated: _thunk_sub_100B3270, 0.80]
@ 0x100D679C _thunk_sub_100B3270_100D679C;

// [propagated: _thunk_sub_10190490, 0.80]
@ 0x100D6B40 _thunk_sub_10190490_100D6B40;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100D8366 _thunk_sub_1018DEB0_100D8366;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100D836B _thunk_sub_1018DEB0_100D836B;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100D8935 _thunk_sub_1018DEB0_100D8935;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100D8EB3 _thunk_sub_1018DEB0_100D8EB3;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100D9435 _thunk_sub_1018DEB0_100D9435;

// [propagated: _thunk_sub_1019AA10, 0.80]
@ 0x100DD404 _thunk_sub_1019AA10_100DD404;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100E040A _thunk_sub_1018DEB0_100E040A;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100E0B9B _thunk_sub_1018DEB0_100E0B9B;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100E2290 _thunk_sub_1018DEB0_100E2290;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x100E3C09 _thunk_sub_1018DEF3_100E3C09;

// [propagated: _thunk_sub_10190490, 0.80]
@ 0x100E45F0 _thunk_sub_10190490_100E45F0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100E78CC _thunk_sub_1018DEB0_100E78CC;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100E8C43 _thunk_sub_1018DEB0_100E8C43;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100E913C _thunk_sub_1018DEB0_100E913C;

// [propagated: _thunk_sub_100C6AB0, 0.80]
@ 0x100EC676 _thunk_sub_100C6AB0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100EE82A _thunk_sub_1018DEB0_100EE82A;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100EF104 _thunk_sub_1018DEB0_100EF104;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100F0E99 _thunk_sub_1018DEB0_100F0E99;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100F10F2 _thunk_sub_1018DEB0_100F10F2;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100F1E6A _thunk_sub_1018DEB0_100F1E6A;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100F289E _thunk_sub_1018DEB0_100F289E;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100F2D8A _thunk_sub_1018DEB0_100F2D8A;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100F320E _thunk_sub_1018DEB0_100F320E;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100F4E27 _thunk_sub_1018DEB0_100F4E27;

// [propagated: _thunk_sub_1010E2A0, 0.80]
@ 0x100F5F28 _thunk_sub_1010E2A0;

// [propagated: _thunk_sub_1010E2A0, 0.80]
@ 0x100F5F44 _thunk_sub_1010E2A0_100F5F44;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100F7CD2 _thunk_sub_1018DEB0_100F7CD2;

// [propagated: _thunk_sub_100FAA40, 0.80]
@ 0x100FAEED _thunk_sub_100FAA40;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100FB2DB _thunk_sub_1018DEB0_100FB2DB;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x100FD80E _thunk_sub_1018DEB0_100FD80E;

// [propagated: _thunk_sub_10010540, 0.80]
@ 0x101012E0 _thunk_sub_10010540_101012E0;

// [propagated: _thunk_sub_10068ED0, 0.80]
@ 0x101014E0 _thunk_sub_10068ED0_101014E0;

// [propagated: _thunk_sub_10100580, 0.80]
@ 0x10101550 _thunk_sub_10100580;

// [propagated: _thunk_sub_10010540, 0.80]
@ 0x101017E0 _thunk_sub_10010540_101017E0;

// [propagated: _thunk_sub_10010540, 0.80]
@ 0x10101890 _thunk_sub_10010540_10101890;

// [propagated: _thunk_sub_10068ED0, 0.80]
@ 0x10101EE0 _thunk_sub_10068ED0_10101EE0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x101032CA _thunk_sub_1018DEB0_101032CA;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x101032CC _thunk_sub_1018DEB0_101032CC;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10103588 _thunk_sub_1018DEB0_10103588;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x101035F3 _thunk_sub_1018DEB0_101035F3;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x101036D5 _thunk_sub_1018DEB0_101036D5;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x101083A7 _thunk_sub_1018DEB0_101083A7;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10109001 _thunk_sub_1018DEB0_10109001;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10109015 _thunk_sub_1018DEB0_10109015;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1010B7DA _thunk_sub_1018DEB0_1010B7DA;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1010B7DF _thunk_sub_1018DEB0_1010B7DF;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1010C831 _thunk_sub_1018DEB0_1010C831;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1010E278 _thunk_sub_1018DEB0_1010E278;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1010E27D _thunk_sub_1018DEB0_1010E27D;

// [propagated: _thunk_sub_10068ED0, 0.80]
@ 0x1010E590 _thunk_sub_10068ED0_1010E590;

// [propagated: _thunk_sub_100100D0, 0.80]
@ 0x1010F442 _thunk_sub_100100D0;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x10111A9F _thunk_sub_1018B1F4_10111A9F;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x10111C8E _thunk_sub_1018B1F4_10111C8E;

// [propagated: _thunk_sub_100D0EB0, 0.80]
@ 0x10112610 _thunk_sub_100D0EB0;

// [propagated: _thunk_sub_100D0EB0, 0.80]
@ 0x1011265A _thunk_sub_100D0EB0_1011265A;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x10112D80 _thunk_sub_1018DEF3_10112D80;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x1011316E _thunk_sub_1018B1F4_1011316E;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1011418F _thunk_sub_1018DEB0_1011418F;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1011435F _thunk_sub_1018DEB0_1011435F;

// [propagated: _thunk_sub_101B3870, 0.80]
@ 0x10116995 _thunk_sub_101B3870;

// [propagated: _thunk_sub_1000FDD0, 0.80]
@ 0x101172B0 _thunk_sub_1000FDD0_101172B0;

// [propagated: _thunk_sub_10131130, 0.80]
@ 0x10119A50 _thunk_sub_10131130;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1011A374 _thunk_sub_1018DEB0_1011A374;

// [propagated: _thunk_sub_100EA360, 0.80]
@ 0x1011A990 _thunk_sub_100EA360;

// [propagated: _thunk_sub_10131130, 0.80]
@ 0x1011B9D0 _thunk_sub_10131130_1011B9D0;

// [propagated: _thunk_sub_1011D020, 0.80]
@ 0x1011D000 _thunk_sub_1011D020;

// [propagated: _thunk_sub_10101640, 0.80]
@ 0x1011D630 _thunk_sub_10101640;

// [propagated: _thunk_sub_1010F950, 0.80]
@ 0x1011DD5E _thunk_sub_1010F950;

// [propagated: _thunk_sub_10190490, 0.80]
@ 0x1011E3E0 _thunk_sub_10190490_1011E3E0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1011F870 _thunk_sub_1018DEB0_1011F870;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x101210FC _thunk_sub_1018DEB0_101210FC;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1012139F _thunk_sub_1018DEB0_1012139F;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1012155E _thunk_sub_1018DEB0_1012155E;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10121EF8 _thunk_sub_1018DEB0_10121EF8;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x101223BD _thunk_sub_1018DEB0_101223BD;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10122B45 _thunk_sub_1018DEB0_10122B45;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10122CD0 _thunk_sub_1018DEB0_10122CD0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10124B09 _thunk_sub_1018DEB0_10124B09;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x101252C0 _thunk_sub_1018DEB0_101252C0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10125370 _thunk_sub_1018DEB0_10125370;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x101258E7 _thunk_sub_1018DEB0_101258E7;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10129BA5 _thunk_sub_1018DEB0_10129BA5;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10129C0D _thunk_sub_1018DEB0_10129C0D;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10129C3B _thunk_sub_1018DEB0_10129C3B;

// [propagated: _thunk_sub_100FF0E0, 0.80]
@ 0x1012AE00 _thunk_sub_100FF0E0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1012C240 _thunk_sub_1018DEB0_1012C240;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1012C4D4 _thunk_sub_1018DEB0_1012C4D4;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1012CCFE _thunk_sub_1018DEB0_1012CCFE;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1012D70D _thunk_sub_1018DEB0_1012D70D;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1012E432 _thunk_sub_1018DEB0_1012E432;

// [propagated: _thunk_sub_10129D00, 0.80]
@ 0x1012E460 _thunk_sub_10129D00;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x10130600 _thunk_sub_1018B1F4_10130600;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x10130780 _thunk_sub_1018B1F4_10130780;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x10130900 _thunk_sub_1018B1F4_10130900;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x10130A80 _thunk_sub_1018B1F4_10130A80;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10136DD2 _thunk_sub_1018DEB0_10136DD2;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10136DD4 _thunk_sub_1018DEB0_10136DD4;

// [propagated: _thunk_sub_10016550, 0.80]
@ 0x101376A0 _thunk_sub_10016550;

// [propagated: _thunk_sub_10139190, 0.80]
@ 0x10137A7B _thunk_sub_10139190;

// [propagated: _thunk_sub_10139110, 0.80]
@ 0x10137BFB _thunk_sub_10139110;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x10138FE0 _thunk_sub_10019B20_10138FE0;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x10139030 _thunk_sub_10019B20_10139030;

// [propagated: _thunk_sub_10010540, 0.80]
@ 0x10139CA0 _thunk_sub_10010540_10139CA0;

// [propagated: _thunk_sub_1016B630, 0.80]
@ 0x1013AC40 _thunk_sub_1016B630;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1013B4C4 _thunk_sub_1018DEB0_1013B4C4;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1013B4EC _thunk_sub_1018DEB0_1013B4EC;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1013B96C _thunk_sub_1018DEB0_1013B96C;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1013B971 _thunk_sub_1018DEB0_1013B971;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1013B999 _thunk_sub_1018DEB0_1013B999;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x1013B9E0 _thunk_sub_10019B20_1013B9E0;

// [propagated: _thunk_sub_10044480, 0.80]
@ 0x1013C80B _thunk_sub_10044480;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x1013DDA0 _thunk_sub_10019B20_1013DDA0;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x1013DDF0 _thunk_sub_10019B20_1013DDF0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x101439E2 _thunk_sub_1018DEB0_101439E2;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x101439E7 _thunk_sub_1018DEB0_101439E7;

// [propagated: _thunk_sub_1018DEBE, 0.80]
@ 0x10143A20 _thunk_sub_1018DEBE;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x10145E70 _thunk_sub_10019B20_10145E70;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x10145EC0 _thunk_sub_10019B20_10145EC0;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x10145F10 _thunk_sub_10019B20_10145F10;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x10145F60 _thunk_sub_10019B20_10145F60;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x10145FB0 _thunk_sub_10019B20_10145FB0;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x10146230 _thunk_sub_1018DEF3_10146230;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x101465B0 _thunk_sub_10019B20_101465B0;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x10146AE0 _thunk_sub_10019B20_10146AE0;

// [propagated: _thunk_sub_1014B2E0, 0.80]
@ 0x10146B60 _thunk_sub_1014B2E0;

// [propagated: _thunk_sub_1001BE70, 0.80]
@ 0x101478E0 _thunk_sub_1001BE70;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x101481E0 _thunk_sub_10019B20_101481E0;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x101485C0 _thunk_sub_10019B20_101485C0;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x10148BA0 _thunk_sub_10019B20_10148BA0;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x10148F80 _thunk_sub_10019B20_10148F80;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x10149360 _thunk_sub_10019B20_10149360;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x10149740 _thunk_sub_10019B20_10149740;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x10149B20 _thunk_sub_10019B20_10149B20;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x10149F00 _thunk_sub_10019B20_10149F00;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x1014A2E0 _thunk_sub_10019B20_1014A2E0;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x1014A6C0 _thunk_sub_10019B20_1014A6C0;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x1014AAA0 _thunk_sub_10019B20_1014AAA0;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x1014AE80 _thunk_sub_10019B20_1014AE80;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x1014B260 _thunk_sub_10019B20_1014B260;

// [propagated: _thunk_sub_1018B214, 0.80]
@ 0x1014E380 _thunk_sub_1018B214_1014E380;

// [propagated: _thunk_sub_100100B0, 0.80]
@ 0x101511AF _thunk_sub_100100B0_101511AF;

// [propagated: _thunk_sub_1000B360, 0.80]
@ 0x10158340 _thunk_sub_1000B360_10158340;

// [propagated: _thunk_sub_1000B360, 0.80]
@ 0x10158370 _thunk_sub_1000B360_10158370;

// [propagated: _thunk_sub_10159810, 0.80]
@ 0x101585C0 _thunk_sub_10159810;

// [propagated: _thunk_sub_10159A00, 0.80]
@ 0x10158630 _thunk_sub_10159A00;

// [propagated: _thunk_sub_10159100, 0.80]
@ 0x10158790 _thunk_sub_10159100;

// [propagated: _thunk_sub_1018D4D0, 0.80]
@ 0x10158AE0 _thunk_sub_1018D4D0;

// [propagated: _thunk_sub_1018D4D0, 0.80]
@ 0x10158B40 _thunk_sub_1018D4D0_10158B40;

// [propagated: _thunk_sub_1000C7C0, 0.80]
@ 0x10158D20 _thunk_sub_1000C7C0;

// [propagated: _thunk_sub_10159590, 0.80]
@ 0x10158D40 _thunk_sub_10159590;

// [propagated: _thunk_sub_10159810, 0.80]
@ 0x10158D70 _thunk_sub_10159810_10158D70;

// [propagated: _thunk_sub_1006B4A0, 0.80]
@ 0x10159BD0 _thunk_sub_1006B4A0;

// [propagated: _thunk_sub_1015B630, 0.80]
@ 0x1015A451 _thunk_sub_1015B630;

// [propagated: _thunk_sub_1018B234, 0.80]
@ 0x1015AC40 _thunk_sub_1018B234;

// [propagated: _thunk_sub_1018FA60, 0.80]
@ 0x1015AC50 _thunk_sub_1018FA60;

// [propagated: _thunk_sub_1015DC00, 0.80]
@ 0x1015B4B1 _thunk_sub_1015DC00;

// [propagated: _thunk_sub_100144D0, 0.80]
@ 0x1015B83E _thunk_sub_100144D0_1015B83E;

// [propagated: _thunk_sub_100144D0, 0.80]
@ 0x1015B841 _thunk_sub_100144D0_1015B841;

// [propagated: _thunk_sub_1015AC50, 0.80]
@ 0x1015BEF0 _thunk_sub_1015AC50;

// [propagated: _thunk_sub_1018DEC3, 0.80]
@ 0x1015D8C0 _thunk_sub_1018DEC3_1015D8C0;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x1015DC00 _thunk_sub_1018B1F4_1015DC00;

// [propagated: _thunk_sub_1018DEC3, 0.80]
@ 0x1015E520 _thunk_sub_1018DEC3_1015E520;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x1015EBF0 _thunk_sub_1018DEF3_1015EBF0;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x1015EE50 _thunk_sub_1018DEF3_1015EE50;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x1015F040 _thunk_sub_1018DEF3_1015F040;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x1015F2A0 _thunk_sub_1018DEF3_1015F2A0;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x1015F620 _thunk_sub_1018DEF3_1015F620;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x1015F8D0 _thunk_sub_1018DEF3_1015F8D0;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x1015FC40 _thunk_sub_1018DEF3_1015FC40;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x1015FDF0 _thunk_sub_1018DEF3_1015FDF0;

// [propagated: _thunk_sub_100100B0, 0.80]
@ 0x10160A9E _thunk_sub_100100B0_10160A9E;

// [propagated: _thunk_sub_10166930, 0.80]
@ 0x10162360 _thunk_sub_10166930;

// [propagated: _thunk_sub_101643A0, 0.80]
@ 0x10162870 _thunk_sub_101643A0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10163CAD _thunk_sub_1018DEB0_10163CAD;

// [propagated: _thunk_sub_10019B20, 0.80]
@ 0x10164440 _thunk_sub_10019B20_10164440;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x10164B70 _thunk_sub_1018DEF3_10164B70;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x10164B90 _thunk_sub_1018DEF3_10164B90;

// [propagated: _thunk_sub_1000A1E0, 0.80]
@ 0x101650E0 _thunk_sub_1000A1E0_101650E0;

// [propagated: _thunk_sub_10166930, 0.80]
@ 0x10165BF0 _thunk_sub_10166930_10165BF0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10165E24 _thunk_sub_1018DEB0_10165E24;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x10166910 _thunk_sub_1018DEF3_10166910;

// [propagated: _thunk_sub_10028500, 0.80]
@ 0x10167040 _thunk_sub_10028500;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10167A04 _thunk_sub_1018DEB0_10167A04;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10167A06 _thunk_sub_1018DEB0_10167A06;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10167CCA _thunk_sub_1018DEB0_10167CCA;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10167CCC _thunk_sub_1018DEB0_10167CCC;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x101683B9 _thunk_sub_1018DEB0_101683B9;

// [propagated: _thunk_sub_1000A1E0, 0.80]
@ 0x10168BD8 _thunk_sub_1000A1E0_10168BD8;

// [propagated: _thunk_sub_1016BA50, 0.80]
@ 0x1016B580 _thunk_sub_1016BA50;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1016D38B _thunk_sub_1018DEB0_1016D38B;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1016D3B2 _thunk_sub_1018DEB0_1016D3B2;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x1016D870 _thunk_sub_1018DEF3_1016D870;

// [propagated: _thunk_sub_1018DEF3, 0.80]
@ 0x1016D8D0 _thunk_sub_1018DEF3_1016D8D0;

// [propagated: _thunk_sub_1018B1F4, 0.80]
@ 0x1016D990 _thunk_sub_1018B1F4_1016D990;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x101716BC _thunk_sub_1018DEB0_101716BC;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10176C44 _thunk_sub_1018DEB0_10176C44;

// [propagated: _thunk_sub_1006B0A0, 0.80]
@ 0x1017B060 _thunk_sub_1006B0A0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1017DC56 _thunk_sub_1018DEB0_1017DC56;

// [propagated: _thunk_sub_1000B360, 0.80]
@ 0x1017FE50 _thunk_sub_1000B360_1017FE50;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10180E80 _thunk_sub_1018DEB0_10180E80;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10180E89 _thunk_sub_1018DEB0_10180E89;

// [propagated: _thunk_sub_10008AD0, 0.80]
@ 0x10180F50 _thunk_sub_10008AD0_10180F50;

// [propagated: _thunk_sub_101812C0, 0.80]
@ 0x101813B0 _thunk_sub_101812C0;

// [propagated: _thunk_sub_10168550, 0.80]
@ 0x10181420 _thunk_sub_10168550;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x101818A0 _thunk_sub_1018DEB0_101818A0;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10181950 _thunk_sub_1018DEB0_10181950;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10181DF0 _thunk_sub_1018DEB0_10181DF0;

// [propagated: _thunk_sub_1019C100, 0.80]
@ 0x10182E10 _thunk_sub_1019C100;

// [propagated: _thunk_sub_1019C100, 0.80]
@ 0x10182E50 _thunk_sub_1019C100_10182E50;

// [propagated: _thunk_sub_10168580, 0.80]
@ 0x101835E0 _thunk_sub_10168580;

// [propagated: _thunk_sub_10180520, 0.80]
@ 0x10184071 _thunk_sub_10180520;

// [propagated: _thunk_sub_1000B360, 0.80]
@ 0x101852B0 _thunk_sub_1000B360_101852B0;

// [propagated: _thunk_sub_1018DEBE, 0.80]
@ 0x101856B1 _thunk_sub_1018DEBE_101856B1;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10186C60 _thunk_sub_1018DEB0_10186C60;

// [propagated: _thunk_sub_1000A1E0, 0.80]
@ 0x10187407 _thunk_sub_1000A1E0_10187407;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10188327 _thunk_sub_1018DEB0_10188327;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10188943 _thunk_sub_1018DEB0_10188943;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10189131 _thunk_sub_1018DEB0_10189131;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x101898D3 _thunk_sub_1018DEB0_101898D3;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10189E29 _thunk_sub_1018DEB0_10189E29;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1018A485 _thunk_sub_1018DEB0_1018A485;

// [propagated: _thunk_sub_1018A680, 0.80]
@ 0x1018A600 _thunk_sub_1018A680;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1018C67A _thunk_sub_1018DEB0_1018C67A;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1018D0EE _thunk_sub_1018DEB0_1018D0EE;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1018D141 _thunk_sub_1018DEB0_1018D141;

// [propagated: _thunk_sub_1018BC40, 0.80]
@ 0x1018D300 _thunk_sub_1018BC40;

// [propagated: _thunk_sub_1018E4A6, 0.80]
@ 0x1018E3D5 _thunk_sub_1018E4A6;

// [propagated: _thunk_sub_1018E4A6, 0.80]
@ 0x1018E403 _thunk_sub_1018E4A6_1018E403;

// [propagated: _thunk_sub_1018E4A6, 0.80]
@ 0x1018E454 _thunk_sub_1018E4A6_1018E454;

// [propagated: _thunk_sub_1018DEC3, 0.80]
@ 0x1018E630 _thunk_sub_1018DEC3_1018E630;

// [propagated: _thunk_sub_1018DF24, 0.80]
@ 0x1018E688 _thunk_sub_1018DF24;

// [propagated: _thunk_sub_101A15BC, 0.80]
@ 0x1018F400 _thunk_sub_101A15BC;

// [propagated: _thunk_sub_101A15BC, 0.80]
@ 0x1018F42B _thunk_sub_101A15BC_1018F42B;

// [propagated: _thunk_sub_10191B37, 0.80]
@ 0x1018FA4E _thunk_sub_10191B37;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x1018FB90 _thunk_sub_1018DEB0_1018FB90;

// [propagated: _thunk_sub_10191BD7, 0.80]
@ 0x10190490 _thunk_sub_10191BD7;

// [propagated: _thunk_sub_10191BD7, 0.80]
@ 0x101905C3 _thunk_sub_10191BD7_101905C3;

// [propagated: _thunk_sub_10191BD7, 0.80]
@ 0x101905EA _thunk_sub_10191BD7_101905EA;

// [propagated: _thunk_sub_10191BD7, 0.80]
@ 0x10190657 _thunk_sub_10191BD7_10190657;

// [propagated: _thunk_sub_10192E80, 0.80]
@ 0x10192B40 _thunk_sub_10192E80;

// [propagated: _thunk_sub_10192FCB, 0.80]
@ 0x101931D7 _thunk_sub_10192FCB;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10196830 _thunk_sub_1018DEB0_10196830;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10196CDE _thunk_sub_1018DEB0_10196CDE;

// [propagated: _thunk_sub_1018DEB0, 0.80]
@ 0x10196D46 _thunk_sub_1018DEB0_10196D46;

// [propagated: _thunk_sub_101B34D0, 0.80]
@ 0x10197940 _thunk_sub_101B34D0;

// [propagated: _thunk_sub_1019A410, 0.80]
@ 0x1019A3C0 _thunk_sub_1019A410;

// [propagated: _thunk_sub_1019A789, 0.80]
@ 0x1019A8B3 _thunk_sub_1019A789;

// [propagated: _thunk_sub_10197930, 0.80]
@ 0x1019AA10 _thunk_sub_10197930_1019AA10;

// [propagated: _thunk_sub_101B34D0, 0.80]
@ 0x1019C050 _thunk_sub_101B34D0_1019C050;

// [propagated: _thunk_sub_1019C644, 0.80]
@ 0x1019C73D _thunk_sub_1019C644;

// [propagated: _thunk_sub_1019C644, 0.80]
@ 0x1019C742 _thunk_sub_1019C644_1019C742;

// [propagated: _thunk_sub_101931D7, 0.80]
@ 0x1019D214 _thunk_sub_101931D7_1019D214;

// [propagated: _thunk_sub_101931D7, 0.80]
@ 0x1019D239 _thunk_sub_101931D7_1019D239;

// [propagated: _thunk_sub_1019C8FC, 0.80]
@ 0x1019FAB7 _thunk_sub_1019C8FC;

// [propagated: _thunk_sub_1019C8FC, 0.80]
@ 0x1019FACE _thunk_sub_1019C8FC_1019FACE;

// [propagated: _thunk_sub_1019FB90, 0.80]
@ 0x1019FB30 _thunk_sub_1019FB90;

// [propagated: _thunk_sub_101A0455, 0.80]
@ 0x101A0529 _thunk_sub_101A0455;

// [propagated: _thunk_sub_101A4A00, 0.80]
@ 0x101A1B8A _thunk_sub_101A4A00;

// [propagated: _thunk_sub_101A1C20, 0.80]
@ 0x101A1BC0 _thunk_sub_101A1C20;

// [propagated: _thunk_sub_101A0D4B, 0.80]
@ 0x101A1C6E _thunk_sub_101A0D4B;

// [propagated: _thunk_sub_101A0D4B, 0.80]
@ 0x101A1CDC _thunk_sub_101A0D4B_101A1CDC;

// [propagated: _thunk_sub_101A213C, 0.80]
@ 0x101A20CC _thunk_sub_101A213C;

// [propagated: _thunk_sub_101A32C6, 0.80]
@ 0x101A2B9F _thunk_sub_101A32C6;

// [propagated: _thunk_sub_101931D7, 0.80]
@ 0x101A3868 _thunk_sub_101931D7_101A3868;

// [propagated: _thunk_sub_101A6B40, 0.80]
@ 0x101A6535 _thunk_sub_101A6B40;

// [propagated: _thunk_sub_101A6B40, 0.80]
@ 0x101A653F _thunk_sub_101A6B40_101A653F;

// [propagated: _thunk_sub_101A65A0, 0.80]
@ 0x101A6570 _thunk_sub_101A65A0;

// [propagated: _thunk_sub_101B1530, 0.80]
@ 0x101AA230 _thunk_sub_101B1530;

// [propagated: _thunk_sub_101931D7, 0.80]
@ 0x101AB288 _thunk_sub_101931D7_101AB288;

// [propagated: _thunk_sub_101B3752, 0.80]
@ 0x101AD430 _thunk_sub_101B3752;

// [propagated: _thunk_sub_101931D7, 0.80]
@ 0x101B0E8B _thunk_sub_101931D7_101B0E8B;

// [propagated: _thunk_sub_101931D7, 0.80]
@ 0x101B0E8D _thunk_sub_101931D7_101B0E8D;

// [propagated: _thunk_sub_101B1772, 0.80]
@ 0x101B17E4 _thunk_sub_101B1772;

// [propagated: _thunk_sub_101B20F0, 0.80]
@ 0x101B1BF0 _thunk_sub_101B20F0;

// [propagated: _thunk_sub_101A9E32, 0.80]
@ 0x101B20CF _thunk_sub_101A9E32;

// [propagated: _thunk_sub_1019A9FB, 0.80]
@ 0x101B2293 _thunk_sub_1019A9FB;

// [propagated: _thunk_sub_101B20F0, 0.80]
@ 0x101B25DE _thunk_sub_101B20F0_101B25DE;

// [propagated: _thunk_sub_101B20F0, 0.80]
@ 0x101B25E3 _thunk_sub_101B20F0_101B25E3;

// [propagated: _thunk_sub_101B2ED0, 0.80]
@ 0x101B29AD _thunk_sub_101B2ED0;

// [propagated: _thunk_sub_101B2ED0, 0.80]
@ 0x101B29F0 _thunk_sub_101B2ED0_101B29F0;

// [propagated: _thunk_sub_101B2C83, 0.80]
@ 0x101B2B74 _thunk_sub_101B2C83;

// [propagated: _thunk_sub_10197A50, 0.80]
@ 0x101B3210 _thunk_sub_10197A50;

// [propagated: _thunk_sub_101B3928, 0.80]
@ 0x101B38E0 _thunk_sub_101B3928;

// [propagated: _thunk_sub_101B3928, 0.80]
@ 0x101B3928 _thunk_sub_101B3928_101B3928;

// [propagated: _thunk_sub_101AAF50, 0.80]
@ 0x101B3A30 _thunk_sub_101AAF50;

// [propagated: _thunk_sub_101B4714, 0.80]
@ 0x101B44D4 _thunk_sub_101B4714;

// [propagated: _thunk_sub_101B20F0, 0.80]
@ 0x101B4A29 _thunk_sub_101B20F0_101B4A29;

// [propagated: _thunk_sub_101B20F0, 0.80]
@ 0x101B4AA6 _thunk_sub_101B20F0_101B4AA6;

// [propagated: _thunk_sub_101B20F0, 0.80]
@ 0x101B511D _thunk_sub_101B20F0_101B511D;


// TTerrainShaderD3D.dll, separate module with preferred base 0x10000000.
@ 0x1000AB90 void __thiscall TTerrainShaderHAL_DoSingleTimeStep_Optimized(void* owner);
@ 0x1000B950 bool __thiscall TTerrainShaderHAL_GenerateNormalInit(void* owner, int width, int height);
@ 0x1000C3F0 void __thiscall TTerrainShaderHAL_DelVertexShader(void* owner, unsigned long* handle);
@ 0x1000C450 void __thiscall TTerrainShaderHAL_CreateShaders(void* owner);
@ 0x1000C700 void __thiscall TTerrainShaderHAL_DestroyShaders(void* owner);
@ 0x1000A8B0 bool __thiscall TTerrainShaderHAL_OnLostDevice(void* owner);
@ 0x1000A910 bool __thiscall TTerrainShaderHAL_OnResetDevice(void* owner);
@ 0x10010D30 bool __thiscall TTerrainShaderHAL_IsHighEndWater(void* owner);
@ 0x10010D50 bool __thiscall TTerrainShaderHAL_IsHighEndTerrain(void* owner);
