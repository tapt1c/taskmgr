#ifndef TM95_DRIVER_H
#define TM95_DRIVER_H
#include "../driver/protocol.h"
BOOL driver_enable(HINSTANCE instance);
void driver_close(void);
BOOL driver_sample(TM95_VXD_SAMPLE *sample);
DWORD driver_error(void);
#endif
