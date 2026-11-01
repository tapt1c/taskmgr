/* Experimental read-only Windows 95 VxD. No DDK headers required.
 * ABI facts cross-checked against JHRobotics/vmdisp9x (vmm.h/vxd_main.c).
 * This driver has NOT been executed in Windows 95. No CPU capability is claimed.
 */
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
#define TM95_RING0
#include "protocol.h"
#pragma data_seg("_LTEXT", "CODE")
#pragma code_seg("_LTEXT", "CODE")
#pragma const_seg("_LTEXT", "CODE")
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
    0, 0x400, 0, 0, 2, 0, {'T','M','9','5','S','T','A','T'},
    0x80000000UL, control, 0,0,0,0,0,0,0,0,0,sizeof(struct DDB),0,0,0
};
typedef char ddb_size_check[sizeof(struct DDB)==80?1:-1];
struct DIOC {
    DWORD internal1, vm, internal2, code;
    void *input;
    DWORD input_size;
    void *output;
    DWORD output_size;
    DWORD *returned;
    DWORD overlapped, device, process;
};
static DWORD system_time(void);
#pragma aux system_time = "int 20h" "dd 0001003fh" value [eax] modify exact [eax]
static DWORD free_pages(void);
#pragma aux free_pages = "push 0" "int 20h" "dd 0001005ah" "add esp,4" value [eax] modify [eax ecx edx]
static DWORD system_pages(void);
#pragma aux system_pages = "push 0" "int 20h" "dd 0001005bh" "add esp,4" value [eax] modify [eax ecx edx]
static DWORD __stdcall query(struct DIOC *request)
{
    TM95_VXD_SAMPLE *out;
    if(!request) return 87;
    if(request->code==0 || request->code==0xffffffffUL) return 0;
    if(request->code!=1) return 50;
    if(!request->returned || !request->output || request->output_size<sizeof(TM95_VXD_SAMPLE) || request->input_size) return 87;
    *request->returned=0;
    out=(TM95_VXD_SAMPLE *)request->output;
    out->size=sizeof(*out);out->version=TM95_PROTOCOL_VERSION;out->capabilities=TM95_CAP_UPTIME|TM95_CAP_PAGE_COUNTS;
    out->uptime_ms=system_time();out->free_pages=free_pages();out->system_pages=system_pages();
    *request->returned=sizeof(*out);
    return 0;
}
void __declspec(naked) control(void)
{
    __asm {
        cmp eax,23h
        je io_control
        clc
        ret
    io_control:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        push esi
        call query
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        clc
        ret
    }
}
