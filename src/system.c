#include "system.h"
/* Tool Help is native on Win95. Resolve it to also permit graceful failure
   on systems (e.g. early NT) where these exports do not exist. */
BOOL tm_system_init(TM_SYSTEM *sys)
{
    HMODULE kernel;
    ZeroMemory(sys, sizeof(*sys));
    kernel = GetModuleHandleA("KERNEL32.DLL");
    sys->snapshot = (TM_SNAPSHOT)GetProcAddress(kernel, "CreateToolhelp32Snapshot");
    sys->first = (TM_PROCESS)GetProcAddress(kernel, "Process32First");
    sys->next = (TM_PROCESS)GetProcAddress(kernel, "Process32Next");
    /* NT Tool Help names include A; Win95 uses the unsuffixed exports. */
    if (!sys->first) sys->first = (TM_PROCESS)GetProcAddress(kernel, "Process32FirstA");
    if (!sys->next) sys->next = (TM_PROCESS)GetProcAddress(kernel, "Process32NextA");
    return sys->snapshot && sys->first && sys->next;
}
HANDLE tm_process_snapshot(TM_SYSTEM *sys)
{
    if (!sys->snapshot || !sys->first || !sys->next) return INVALID_HANDLE_VALUE;
    return sys->snapshot(TH32CS_SNAPPROCESS, 0);
}
void tm_memory_sample(TM_SYSTEM *sys)
{
    sys->memory.dwLength = sizeof(sys->memory);
    GlobalMemoryStatus(&sys->memory);
}
