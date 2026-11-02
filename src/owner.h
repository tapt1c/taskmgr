#ifndef TM95_OWNER_H
#define TM95_OWNER_H
#include <windows.h>
void owner_init(void);
BOOL owner_available(void);
BOOL owner_name(DWORD pid,WCHAR *name,int count);
#endif
