
#include "nocrt.c"

// @Robustness: I have no idea how DLL startup works.
// From MSDN: "When a DLL is loaded using LoadLibrary, existing threads do not call the entry-point function of the newly loaded DLL."
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms682583(v=vs.85).aspx
int _DllMainCRTStartup() {
    return 1;
}
