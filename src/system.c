#include "system.h"
#include "process_policy.h"
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
    if (GetVersion() & 0x80000000UL) {
        HKEY start;
        DWORD value, size=sizeof(value), type;
        if (RegOpenKeyExA(HKEY_DYN_DATA,"PerfStats\\StartStat",0,KEY_READ,&start)==ERROR_SUCCESS) {
            sys->cpu_started=RegQueryValueExA(start,"KERNEL\\CPUUsage",NULL,&type,(LPBYTE)&value,&size)==ERROR_SUCCESS;
            RegCloseKey(start);
        }
        RegOpenKeyExA(HKEY_DYN_DATA,"PerfStats\\StatData",0,KEY_READ,&sys->cpu_key);
    }
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
    sys->vxd_valid=driver_sample(&sys->vxd);
    sys->cpu_valid=FALSE;
    if(sys->cpu_key) {
        DWORD size=sizeof(DWORD), type, value;
        if(RegQueryValueExA(sys->cpu_key,"KERNEL\\CPUUsage",NULL,&type,(LPBYTE)&value,&size)==ERROR_SUCCESS &&
            type==REG_DWORD && size==sizeof(DWORD) && value<=100) {
            sys->cpu_valid=TRUE;sys->cpu_percent=value;
        }
    }
}

void tm_system_close(TM_SYSTEM *sys)
{
    if(sys->cpu_key) {
        RegCloseKey(sys->cpu_key);sys->cpu_key=NULL;
    }
    if(sys->cpu_started) {
        HKEY stop;
        DWORD value,size=sizeof(value),type;
        if(RegOpenKeyExA(HKEY_DYN_DATA,"PerfStats\\StopStat",0,KEY_READ,&stop)==ERROR_SUCCESS) {
            RegQueryValueExA(stop,"KERNEL\\CPUUsage",NULL,&type,(LPBYTE)&value,&size);
            RegCloseKey(stop);
        }
    }
    driver_close();
}

/* A fresh snapshot is required: never trust a name cached in a list row.
   Callers pin the process with an open handle before the final check/action. */
BOOL tm_process_end_allowed(TM_SYSTEM *sys,DWORD pid)
{
    HANDLE snapshot;
    PROCESSENTRY32 entry;
    BOOL allowed=FALSE;
    if(!pid || pid==GetCurrentProcessId()) return FALSE;
    snapshot=tm_process_snapshot(sys);
    if(snapshot==INVALID_HANDLE_VALUE) return FALSE;
    ZeroMemory(&entry,sizeof(entry));entry.dwSize=sizeof(entry);
    if(sys->first(snapshot,&entry)) {
        do {
            if(entry.th32ProcessID==pid) {
                allowed=!tm_process_name_protected(entry.szExeFile);
                break;
            }
            entry.dwSize=sizeof(entry);
        } while(sys->next(snapshot,&entry));
    }
    CloseHandle(snapshot);
    return allowed;
}
