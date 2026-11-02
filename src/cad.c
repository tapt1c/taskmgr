#include "cad.h"
#include "driver.h"
static BOOL enabled;
static DWORD seen_events;
static const char run_key[]="Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char run_name[]="Taskmgr95.CtrlAltDel";
BOOL cad_enabled(void) { return enabled; }
static BOOL command_line(char *command)
{
    char path[MAX_PATH];
    DWORD length=GetModuleFileNameA(NULL,path,sizeof(path));
    if(!length || length>=sizeof(path)) { SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE; }
    wsprintfA(command,"\"%s\" /resident",path);return TRUE;
}
static BOOL autostart(BOOL install)
{
    HKEY key;
    LONG error;
    char command[MAX_PATH+24];
    if(install && !command_line(command)) return FALSE;
    error=RegCreateKeyExA(HKEY_CURRENT_USER,run_key,0,NULL,0,KEY_SET_VALUE,NULL,&key,NULL);
    if(error==ERROR_SUCCESS) {
        if(install) error=RegSetValueExA(key,run_name,0,REG_SZ,(const BYTE *)command,lstrlenA(command)+1);
        else {
            error=RegDeleteValueA(key,run_name);
            if(error==ERROR_FILE_NOT_FOUND) error=ERROR_SUCCESS;
        }
        RegCloseKey(key);
    }
    if(error!=ERROR_SUCCESS) { SetLastError((DWORD)error);return FALSE; }
    return TRUE;
}
BOOL cad_requested(void)
{
    HKEY key;
    char command[MAX_PATH+24],saved[MAX_PATH+24];
    DWORD size=sizeof(saved),type=0;
    int i;
    LONG error;
    if(!(GetVersion()&0x80000000UL) || !command_line(command)) return FALSE;
    if(RegOpenKeyExA(HKEY_CURRENT_USER,run_key,0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS) return FALSE;
    error=RegQueryValueExA(key,run_name,NULL,&type,(BYTE *)saved,&size);RegCloseKey(key);
    if(error!=ERROR_SUCCESS || type!=REG_SZ || !size || size>sizeof(saved) || saved[size-1]) return FALSE;
    for(i=0;command[i] && command[i]==saved[i];++i) { }
    return !command[i] && !saved[i];
}
BOOL cad_resident_argument(void)
{
    const char *p=GetCommandLineA(),*arg="/resident";
    if(*p=='"') { ++p;while(*p && *p!='"') ++p;if(*p) ++p; }
    else while(*p && *p!=' ' && *p!='\t') ++p;
    while(*p==' ' || *p=='\t') ++p;
    while(*arg && *p==*arg) { ++p;++arg; }
    return !*arg && (!*p || *p==' ' || *p=='\t');
}
BOOL cad_enable(HINSTANCE instance,BOOL persist)
{
    TM95_VXD_SAMPLE sample;
    if(enabled) return TRUE;
    if(!driver_enable(instance) || !driver_sample(&sample) || !(sample.capabilities&TM95_CAP_CAD)) {
        SetLastError(driver_error()?driver_error():ERROR_NOT_SUPPORTED);return FALSE;
    }
    if(!driver_cad(TRUE)) { SetLastError(driver_error());return FALSE; }
    seen_events=sample.cad_events;
    if(persist && !autostart(TRUE)) {
        DWORD error=GetLastError();driver_cad(FALSE);SetLastError(error);return FALSE;
    }
    enabled=TRUE;return TRUE;
}
BOOL cad_disable(BOOL persist)
{
    if(persist && !autostart(FALSE)) return FALSE;
    if(enabled && !driver_cad(FALSE)) { SetLastError(driver_error());return FALSE; }
    enabled=FALSE;return TRUE;
}
BOOL cad_poll(void)
{
    TM95_VXD_SAMPLE sample;
    if(!enabled) return FALSE;
    if(!driver_sample(&sample) || !sample.cad_armed) { enabled=FALSE;return FALSE; }
    if(sample.cad_events==seen_events) return FALSE;
    seen_events=sample.cad_events;return TRUE;
}
void cad_close(void)
{
    if(enabled) driver_cad(FALSE);
    enabled=FALSE;
}
