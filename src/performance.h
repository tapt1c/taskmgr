#ifndef TM95_PERFORMANCE_H
#define TM95_PERFORMANCE_H
#include "system.h"
#define TM95_PERF_CLASS "Taskmgr95Performance"
BOOL performance_register(HINSTANCE instance);
void performance_update(HWND window, const TM_SYSTEM *sys, BOOL sample);
#endif
