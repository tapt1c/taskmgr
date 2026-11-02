#ifndef TM95_PROCESS_MEMORY_H
#define TM95_PROCESS_MEMORY_H
#include <windows.h>
void process_memory_init(void);
void process_memory_close(void);
/* Resident working set, in KiB. FALSE means unavailable, never zero by default. */
BOOL process_memory_kb(DWORD pid,SIZE_T *kb);
#endif
