#include "process_memory.h"
#include <psapi.h>
static HMODULE psapi;
static BOOL (WINAPI *query_memory)(HANDLE,PPROCESS_MEMORY_COUNTERS,DWORD);
void process_memory_init(void)
{
    process_memory_close();
    /* PSAPI does not exist on Win9x. Keep it out of the static import table. */
    if(GetVersion() & 0x80000000UL) return;
    query_memory=(BOOL (WINAPI *)(HANDLE,PPROCESS_MEMORY_COUNTERS,DWORD))
        GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),"K32GetProcessMemoryInfo");
    if(!query_memory) {
        psapi=LoadLibraryA("PSAPI.DLL");
        if(psapi) query_memory=(BOOL (WINAPI *)(HANDLE,PPROCESS_MEMORY_COUNTERS,DWORD))
            GetProcAddress(psapi,"GetProcessMemoryInfo");
    }
}
void process_memory_close(void)
{
    query_memory=NULL;
    if(psapi) FreeLibrary(psapi);
    psapi=NULL;
}
BOOL process_memory_kb(DWORD pid,SIZE_T *kb)
{
    PROCESS_MEMORY_COUNTERS counters;
    HANDLE process;
    BOOL ok;
    if(!query_memory || !pid) return FALSE;
    /* New NT can query with limited rights; XP needs QUERY_INFORMATION | VM_READ. */
    process=OpenProcess(0x1000,FALSE,pid);
    if(process) {
        ZeroMemory(&counters,sizeof(counters));counters.cb=sizeof(counters);
        ok=query_memory(process,&counters,sizeof(counters));
        CloseHandle(process);
        if(ok) { *kb=counters.WorkingSetSize/1024;return TRUE; }
    }
    process=OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,FALSE,pid);
    if(!process) return FALSE;
    ZeroMemory(&counters,sizeof(counters));counters.cb=sizeof(counters);
    ok=query_memory(process,&counters,sizeof(counters));
    CloseHandle(process);
    if(ok) *kb=counters.WorkingSetSize/1024;
    return ok;
}
