#ifndef TM95_PROCESSES_H
#define TM95_PROCESSES_H
#include "system.h"
#include <commctrl.h>
BOOL processes_init(HWND list,HWND owner,HINSTANCE instance,const char *ini);
void processes_close(void);
void processes_refresh(TM_SYSTEM *sys);
void processes_resize(int width);
void processes_localize(void);
void processes_menu(HMENU menu);
void processes_menu_state(void);
BOOL processes_command(UINT command);
void processes_notify(NMHDR *header);
#endif
