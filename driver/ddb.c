typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
#pragma data_seg("_LTEXT", "CODE")
void control(void);
#pragma pack(push,1)
struct DDB {
    DWORD next;
    WORD sdk, device;
    BYTE major, minor;
    WORD flags;
    char name[8];
    DWORD order;
    void (*control)(void);
    DWORD v86, pm, v86cs, pmcs, reference, services, service_count;
    DWORD win32_services, previous, size, reserved1, reserved2, reserved3;
};
#pragma pack(pop)
/* DDB must be the first bytes of the locked segment (export ordinal 1). */
struct DDB TM95_DDB = {
    0, 0x400, 0, 0, 3, 0, {'T','M','9','5','S','T','A','T'},
    0x80000000UL, control, 0,0,0,0,0,0,0,0,0,sizeof(struct DDB),0,0,0
};
typedef char ddb_size_check[sizeof(struct DDB)==80?1:-1];
