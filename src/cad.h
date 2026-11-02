#ifndef TM95_CAD_H
#define TM95_CAD_H
#include <windows.h>
BOOL cad_enabled(void);
BOOL cad_enable(HINSTANCE instance,BOOL persist);
BOOL cad_disable(BOOL persist);
BOOL cad_poll(void);
BOOL cad_requested(void);
BOOL cad_resident_argument(void);
void cad_close(void);
#endif
