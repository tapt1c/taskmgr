/* Experimental Windows 95 VxD. No DDK headers required.
 * ABI facts cross-checked against JHRobotics/vmdisp9x (vmm.h/vxd_main.c).
 * This driver has NOT been executed in Windows 95. No CPU capability is claimed.
 * VKD filter ABI: CL=scancode, CF=reject. Service 0x000d0012.
 * Swallowed Delete makes are converted to breaks so an already forwarded E0
 * prefix is consumed normally by VKD; never strand its prefix state.
 */
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
#define TM95_RING0
#include "protocol.h"
#pragma data_seg("_LTEXT", "CODE")
#pragma code_seg("_LTEXT", "CODE")
#pragma const_seg("_LTEXT", "CODE")
#include "cad_policy.h"
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
static volatile TM95_CAD_STATE cad;
static volatile DWORD cad_device,previous_keyboard;
void keyboard_filter(void);
static int __stdcall keyboard_input(DWORD scan)
{
    return tm95_cad_key(&cad,scan&255,system_time());
}
void __declspec(naked) keyboard_filter(void)
{
    __asm {
        jmp short keyboard_body
        jmp dword ptr [previous_keyboard]
    keyboard_body:
        pushad
        push ecx
        call keyboard_input
        test eax,eax
        jz forward_key
        popad
        or cl,80h
        jmp dword ptr [previous_keyboard]
    forward_key:
        popad
        jmp dword ptr [previous_keyboard]
    }
}
static DWORD install_keyboard(void);
#pragma aux install_keyboard = \
    "push esi" "mov eax,000d0012h" "mov esi,offset keyboard_filter" \
    "int 20h" "dd 00010090h" "jc install_failed" \
    "mov previous_keyboard,esi" "mov eax,1" "jmp install_done" \
    "install_failed:" "xor eax,eax" "install_done:" "pop esi" \
    value [eax] modify [eax]
static DWORD remove_keyboard(void);
#pragma aux remove_keyboard = \
    "push esi" "mov eax,000d0012h" "mov esi,offset keyboard_filter" \
    "int 20h" "dd 0001011ch" "setnc al" "movzx eax,al" "pop esi" \
    value [eax] modify [eax]
static DWORD disarm(void)
{
    cad.armed=0;
    if(previous_keyboard && !remove_keyboard()) return 170;
    previous_keyboard=0;cad_device=0;return 0;
}
static DWORD __stdcall query(struct DIOC *request)
{
    TM95_VXD_SAMPLE *out;
    TM95_CAD_REQUEST *in;
    if(!request) return 87;
    if(request->code==0) return 0;
    if(request->code==0xffffffffUL) {
        if(request->device==cad_device) return disarm();
        return 0;
    }
    if(request->returned) *request->returned=0;
    if(request->code==TM95_IOCTL_CAD) {
        if(!request->input || request->input_size!=sizeof(TM95_CAD_REQUEST) || request->output_size) return 87;
        in=(TM95_CAD_REQUEST *)request->input;
        if(in->size!=sizeof(*in) || in->version!=TM95_PROTOCOL_VERSION || in->enable>1) return 87;
        if(cad_device && cad_device!=request->device) return 170;
        if(!in->enable) return disarm();
        if(!previous_keyboard && !install_keyboard()) return 50;
        cad_device=request->device;cad.heartbeat=system_time();cad.armed=1;
        return 0;
    }
    if(request->code!=TM95_IOCTL_QUERY) return 50;
    if(!request->returned || !request->output || request->output_size<sizeof(TM95_VXD_SAMPLE) || request->input_size) return 87;
    out=(TM95_VXD_SAMPLE *)request->output;
    out->size=sizeof(*out);out->version=TM95_PROTOCOL_VERSION;
    out->capabilities=TM95_CAP_UPTIME|TM95_CAP_PAGE_COUNTS|TM95_CAP_CAD;
    out->uptime_ms=system_time();out->free_pages=free_pages();out->system_pages=system_pages();
    out->cad_events=cad.events;out->cad_armed=cad.armed;
    if(cad_device==request->device) cad.heartbeat=out->uptime_ms;
    *request->returned=sizeof(*out);
    return 0;
}
void __declspec(naked) control(void)
{
    __asm {
        cmp eax,1ch
        je dynamic_exit
        cmp eax,23h
        je io_control
        clc
        ret
    dynamic_exit:
        pushad
        call disarm
        test eax,eax
        jnz cannot_unload
        popad
        clc
        ret
    cannot_unload:
        popad
        stc
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
