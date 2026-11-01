#ifndef TM95_APPLICATIONS_H
#define TM95_APPLICATIONS_H
#include "system.h"
BOOL applications_init(HWND list,HWND owner,HINSTANCE instance,const char *ini);
void applications_close(void);
void applications_refresh(TM_SYSTEM *sys);
void applications_resize(int width);
void applications_localize(void);
void applications_menu(HMENU menu);
void applications_menu_state(BOOL active);
BOOL applications_command(UINT command);
void applications_notify(NMHDR *header);
#endif
