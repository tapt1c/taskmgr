/* Real policy code with a controlled Tool Help provider. No process is killed. */
#include <windows.h>
#include <stdio.h>
#include "system.h"
static const char *image;
static BOOL snapshot_failure, enumeration_failure;
static int checks,failures;
static HANDLE WINAPI snapshot(DWORD flags,DWORD pid)
{
    (void)flags;(void)pid;
    return snapshot_failure?INVALID_HANDLE_VALUE:CreateEventA(NULL,FALSE,FALSE,NULL);
}
static BOOL WINAPI first(HANDLE handle,LPPROCESSENTRY32 entry)
{
    (void)handle;
    if(enumeration_failure) return FALSE;
    entry->th32ProcessID=123456;
    lstrcpyA(entry->szExeFile,image);
    return TRUE;
}
static BOOL WINAPI next(HANDLE handle,LPPROCESSENTRY32 entry)
{
    (void)handle;(void)entry;return FALSE;
}
/* system.c's unrelated driver calls are not exercised by this test. */
BOOL driver_sample(TM95_VXD_SAMPLE *sample) { (void)sample;return FALSE; }
void driver_close(void) { }
static void verify(BOOL condition,const char *name)
{
    ++checks;
    if(!condition) { ++failures;printf("FAIL: %s\n",name); }
}
int main(void)
{
    TM_SYSTEM sys;
    ZeroMemory(&sys,sizeof(sys));sys.snapshot=snapshot;sys.first=first;sys.next=next;
    image="C:\\Windows\\Notepad.exe";
    verify(tm_process_end_allowed(&sys,123456),"ordinary process can be ended");
    image="C:\\WINDOWS\\SYSTEM\\KERNEL32.DLL";
    verify(!tm_process_end_allowed(&sys,123456),"fresh snapshot overrides previously safe PID");
    image="MsGsRv32.ExE";
    verify(!tm_process_end_allowed(&sys,123456),"critical name is case insensitive");
    image="NOTEPAD.EXE";
    verify(!tm_process_end_allowed(&sys,0),"PID zero protected");
    verify(!tm_process_end_allowed(&sys,GetCurrentProcessId()),"own process protected");
    verify(!tm_process_end_allowed(&sys,654321),"missing PID cannot be verified");
    snapshot_failure=TRUE;
    verify(!tm_process_end_allowed(&sys,123456),"snapshot failure denies termination");
    snapshot_failure=FALSE;enumeration_failure=TRUE;
    verify(!tm_process_end_allowed(&sys,123456),"enumeration failure denies termination");
    enumeration_failure=FALSE;image="";
    verify(!tm_process_end_allowed(&sys,123456),"missing image name denies termination");
    sys.snapshot=NULL;
    verify(!tm_process_end_allowed(&sys,123456),"missing Tool Help denies termination");
    printf("%d backend policy checks, %d failures\n",checks,failures);
    return failures?1:0;
}
