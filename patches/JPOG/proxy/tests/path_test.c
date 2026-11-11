#include "../d3d9_main.c"
WrappedD3D9 *WrappedD3D9_Create(void *real) { (void)real; return NULL; }
void main(void) {
    char small[8], large[MAX_PATH];
    g_hInstance=GetModuleHandleA(NULL);
    if(get_dll_sibling_path(small,sizeof(small),"d3d9_remix.dll")) ExitProcess(1);
    if(!get_dll_sibling_path(large,sizeof(large),"d3d9_remix.dll")) ExitProcess(2);
    ExitProcess(0);
}
