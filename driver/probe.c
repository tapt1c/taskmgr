/* Inert load/IOCTL probe. No VMM calls, hooks, timers or process inspection. */
#include "vxd_abi.h"
#define TM95_RING0
#include "protocol.h"
#pragma data_seg("_LTEXT","CODE")
#pragma code_seg("_LTEXT","CODE")
#pragma const_seg("_LTEXT","CODE")
static DWORD __stdcall query(struct DIOC *request)
{
    TM95_PROBE_SAMPLE *out;
    if(!request) return 87;
    if(request->code==0 || request->code==0xffffffffUL) return 0;
    if(request->returned) *request->returned=0;
    if(request->code!=1) return 50;
    if(!request->returned || !request->output || request->output_size!=sizeof(*out) || request->input_size) return 87;
    out=(TM95_PROBE_SAMPLE *)request->output;
    out->size=sizeof(*out);out->magic=TM95_PROBE_MAGIC;
    *request->returned=sizeof(*out);return 0;
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
