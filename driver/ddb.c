#include "vxd_abi.h"
#pragma data_seg("_LTEXT", "CODE")
void control(void);
/* DDB must be the first bytes of the locked segment (export ordinal 1). */
struct DDB TM95_DDB = {
    0, 0x400, 0, 0, 3, 0,
#ifdef TM95_PROBE
    {'T','M','9','5','T','E','S','T'},
#else
    {'T','M','9','5','S','T','A','T'},
#endif
    0x80000000UL, control, 0,0,0,0,0,0,0,0,0,sizeof(struct DDB),0,0,0
};
typedef char ddb_size_check[sizeof(struct DDB)==80?1:-1];
