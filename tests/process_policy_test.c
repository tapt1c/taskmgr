#include <assert.h>
#include <stdio.h>
#include "process_policy.h"
int main(void)
{
    static const char *const blocked[]={
        "KERNEL32.DLL","kernel32.dll","C:\\WINDOWS\\SYSTEM\\Kernel32.Dll",
        "C:/Windows/System/msgsrv32.exe","MPREXE.EXE","KRNL386.EXE",
        "USER.EXE","GDI.EXE","USER32.DLL","GDI32.DLL","WIN386.EXE",
        "MMTASK.TSK","SYSTEM","System Idle Process","SMSS.EXE","CSRSS.EXE",
        "WININIT.EXE","WINLOGON.EXE","SERVICES.EXE","LSASS.EXE","","C:\\"
    };
    static const char *const allowed[]={
        "NOTEPAD.EXE","C:\\Windows\\Notepad.exe","C:/tools/task.exe",
        "kernel32.dll.exe","my-msgsrv32.exe","notepad.exe.bak"
    };
    unsigned int i;
    for(i=0;i<sizeof(blocked)/sizeof(blocked[0]);++i)
        assert(tm_process_name_protected(blocked[i]));
    for(i=0;i<sizeof(allowed)/sizeof(allowed[0]);++i)
        assert(!tm_process_name_protected(allowed[i]));
    assert(tm_process_name_protected(NULL));
    puts("PASS: 29 process-name policy cases");
    return 0;
}
