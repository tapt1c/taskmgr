#ifndef TM_SYSTEM_H
#define TM_SYSTEM_H
#include <windows.h>
#include <tlhelp32.h>
typedef HANDLE (WINAPI *TM_SNAPSHOT)(DWORD, DWORD);
typedef BOOL (WINAPI *TM_PROCESS)(HANDLE, LPPROCESSENTRY32);
typedef struct {
    TM_SNAPSHOT snapshot;
    TM_PROCESS first, next;
    MEMORYSTATUS memory;
    DWORD processes, threads;
    BOOL processes_valid;
} TM_SYSTEM;
BOOL tm_system_init(TM_SYSTEM *sys);
HANDLE tm_process_snapshot(TM_SYSTEM *sys);
void tm_memory_sample(TM_SYSTEM *sys);
#endif
