/* Exercise production extraction/cleanup with real files. Only VxD opening and
   its Win9x platform/IOCTL boundary are modeled; this does not load a VxD. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "driver.h"
static int mode,device_calls,bad_name,corrupt_read,probe_calls,builtin_calls,active_probe,created_count;
static char extracted[4][MAX_PATH],created_directory[4][MAX_PATH];
static DWORD WINAPI win9x_version(void) { return 0xc0000004UL; }
static HANDLE WINAPI open_file(LPCSTR name,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,
    DWORD disposition,DWORD flags,HANDLE template_file)
{
    if(!strncmp(name,"\\\\.\\",4)) {
        const char *base=strrchr(name,'\\');
        const char *file=name+4;
        int probe=base && !strcmp(base+1,"TM95TEST.VXD");
        if(!strcmp(file,"VWIN32")) { ++builtin_calls;return CreateEventA(NULL,FALSE,FALSE,NULL); }
        if(probe) ++probe_calls;else ++device_calls;
        if(!base || (!probe && strcmp(base+1,"TM95STAT.VXD")) || disposition!=CREATE_NEW) {
            bad_name=1;SetLastError(ERROR_INVALID_PARAMETER);return INVALID_HANDLE_VALUE;
        }
        if((!probe && (mode==2 || mode==3)) || (probe && mode==3) ||
            (mode==1 && strcmp(file,"TM95STAT.VXD"))) {
            SetLastError(ERROR_FILE_NOT_FOUND);return INVALID_HANDLE_VALUE;
        }
        if(GetFileAttributesA(file)==INVALID_FILE_ATTRIBUTES) { SetLastError(ERROR_FILE_NOT_FOUND);return INVALID_HANDLE_VALUE; }
        active_probe=probe;
        return CreateEventA(NULL,FALSE,FALSE,NULL);
    }
    if(disposition==CREATE_NEW) {
        char *slash;
        if(created_count>=4) { SetLastError(ERROR_NOT_ENOUGH_MEMORY);return INVALID_HANDLE_VALUE; }
        lstrcpyA(extracted[created_count],name);lstrcpyA(created_directory[created_count],name);
        slash=strrchr(created_directory[created_count],'\\');if(slash) *slash=0;
        ++created_count;
    }
    return CreateFileA(name,access,share,security,disposition,flags,template_file);
}
static BOOL WINAPI read_file(HANDLE file,LPVOID buffer,DWORD count,LPDWORD got,LPOVERLAPPED overlapped)
{
    BOOL result=ReadFile(file,buffer,count,got,overlapped);
    if(result && *got && corrupt_read) { ((BYTE *)buffer)[0]^=1;corrupt_read=0; }
    return result;
}
static BOOL WINAPI control(HANDLE device,DWORD code,LPVOID input,DWORD input_size,
    LPVOID output,DWORD output_size,LPDWORD returned,LPOVERLAPPED overlapped)
{
    TM95_VXD_SAMPLE *sample=(TM95_VXD_SAMPLE *)output;
    (void)device;(void)input;(void)input_size;(void)overlapped;
    if(active_probe && code==TM95_IOCTL_QUERY && output_size==sizeof(TM95_PROBE_SAMPLE)) {
        TM95_PROBE_SAMPLE *probe=(TM95_PROBE_SAMPLE *)output;
        probe->size=sizeof(*probe);probe->magic=TM95_PROBE_MAGIC;*returned=sizeof(*probe);return TRUE;
    }
    if(code!=TM95_IOCTL_QUERY || output_size!=sizeof(*sample)) { SetLastError(ERROR_INVALID_PARAMETER);return FALSE; }
    ZeroMemory(sample,sizeof(*sample));sample->size=sizeof(*sample);
    sample->version=TM95_PROTOCOL_VERSION;sample->capabilities=7;
    *returned=sizeof(*sample);return TRUE;
}
#define GetVersion win9x_version
#define CreateFileA open_file
#define ReadFile read_file
#define DeviceIoControl control
#include "../src/driver.c"
#undef GetVersion
#undef CreateFileA
#undef ReadFile
#undef DeviceIoControl
int driver_loader_regression(void)
{
    HINSTANCE instance=GetModuleHandleA(NULL);
    char before[MAX_PATH],after[MAX_PATH];
    int scenario,i,failures=0;
    for(scenario=0;scenario<5;++scenario) {
        BOOL result;
        mode=scenario==1?1:scenario>=3?scenario-1:0;
        corrupt_read=scenario==2;bad_name=device_calls=probe_calls=builtin_calls=active_probe=created_count=0;
        GetCurrentDirectoryA(sizeof(before),before);
        result=driver_enable(instance);
        if(result!=(scenario<2) || bad_name ||
            (scenario==0 && device_calls!=1) || (scenario==1 && device_calls!=2) ||
            (scenario==2 && (device_calls || driver_error()!=ERROR_INVALID_DATA)) ||
            (scenario>=3 && (device_calls!=2 || driver_error()!=ERROR_FILE_NOT_FOUND || builtin_calls!=1 ||
                probe_calls!=(scenario==3?1:2))) || (scenario<3 && (probe_calls || builtin_calls))) {
            ++failures;printf("FAIL: loader scenario %d, stage %s, error %lu\n",scenario,driver_stage(),driver_error());
        }
        driver_close();
        GetCurrentDirectoryA(sizeof(after),after);
        if(strcmp(before,after) || created_count!=(scenario>=3?2:1)) ++failures;
        for(i=0;i<created_count;++i) if(GetFileAttributesA(extracted[i])!=INVALID_FILE_ATTRIBUTES ||
            GetFileAttributesA(created_directory[i])!=INVALID_FILE_ATTRIBUTES) {
            ++failures;printf("FAIL: loader cleanup scenario %d\n",scenario);
        }
        if(scenario>=3) {
            char report[2048];
            FILE *log=fopen("taskmgr95-vxd.log","rb");
            size_t size=log?fread(report,1,sizeof(report)-1,log):0;
            if(log) fclose(log);
            report[size]=0;
            if(!strstr(report,"VWIN32 open error: 0") ||
                !strstr(report,scenario==3?"Probe open error: 0":"Probe open error: 2") ||
                !strstr(report,scenario==3?"Probe query attempted: 1":"Probe query attempted: 0") ||
                strcmp(driver_stage(),"CreateFile VxD (name)")) {
                ++failures;printf("FAIL: loader diagnostic/original error scenario %d\n",scenario);
            }
        }
    }
    return failures;
}
