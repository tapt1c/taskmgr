#ifndef TM_SYSTEM_H
#define TM_SYSTEM_H
#include <windows.h>
#include <tlhelp32.h>
#include "driver.h"
typedef HANDLE (WINAPI *TM_SNAPSHOT)(DWORD, DWORD);
typedef BOOL (WINAPI *TM_PROCESS)(HANDLE, LPPROCESSENTRY32);
typedef struct {
    TM_SNAPSHOT snapshot;
    TM_PROCESS first, next;
    MEMORYSTATUS memory;
    DWORD processes, threads;
    BOOL processes_valid;
    BOOL cpu_valid;
    DWORD cpu_percent;
    HKEY cpu_key;
    BOOL cpu_started;
    BOOL vxd_valid;
    TM95_VXD_SAMPLE vxd;
} TM_SYSTEM;
BOOL tm_system_init(TM_SYSTEM *sys);
HANDLE tm_process_snapshot(TM_SYSTEM *sys);
BOOL tm_process_end_allowed(TM_SYSTEM *sys,DWORD pid);
void tm_system_close(TM_SYSTEM *sys);
void tm_memory_sample(TM_SYSTEM *sys);
#endif
