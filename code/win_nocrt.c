
#include "nocrt.c"

// @Incomplete: We don't parse the command line at all.
void WinMainCRTStartup() {
    void *instance = GetModuleHandleW(0);
    ASSERT_TYPE_SIZES;
    int result = WinMain(instance, 0, 0, 0);
    ExitProcess(result);
}
