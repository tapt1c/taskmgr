/* Exercise production extraction/cleanup with real files. Only VxD opening and
   its Win9x platform/IOCTL boundary are modeled; this does not load a VxD. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "driver.h"
static int mode,device_calls,bad_name,corrupt_read;
static char extracted[MAX_PATH],created_directory[MAX_PATH];
static DWORD WINAPI win9x_version(void) { return 0xc0000004UL; }
static HANDLE WINAPI open_file(LPCSTR name,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,
    DWORD disposition,DWORD flags,HANDLE template_file)
{
    if(!strncmp(name,"\\\\.\\",4)) {
        const char *base=strrchr(name,'\\');
        const char *file=name+4;
        ++device_calls;
        if(!base || strcmp(base+1,"TM95STAT.VXD")) { bad_name=1;SetLastError(ERROR_FILE_NOT_FOUND);return INVALID_HANDLE_VALUE; }
        if(mode==2 || (mode==1 && strcmp(file,"TM95STAT.VXD"))) {
            SetLastError(ERROR_FILE_NOT_FOUND);return INVALID_HANDLE_VALUE;
        }
        if(GetFileAttributesA(file)==INVALID_FILE_ATTRIBUTES) { SetLastError(ERROR_FILE_NOT_FOUND);return INVALID_HANDLE_VALUE; }
        return CreateEventA(NULL,FALSE,FALSE,NULL);
    }
    if(disposition==CREATE_NEW) {
        char *slash;
        lstrcpyA(extracted,name);lstrcpyA(created_directory,name);
        slash=strrchr(created_directory,'\\');if(slash) *slash=0;
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
    int scenario,failures=0;
    for(scenario=0;scenario<4;++scenario) {
        BOOL result;
        mode=scenario==1?1:scenario==3?2:0;
        corrupt_read=scenario==2;bad_name=device_calls=0;
        extracted[0]=created_directory[0]=0;
        GetCurrentDirectoryA(sizeof(before),before);
        result=driver_enable(instance);
        if(result!=(scenario<2) || bad_name ||
            (scenario==0 && device_calls!=1) || (scenario==1 && device_calls!=2) ||
            (scenario==2 && (device_calls || driver_error()!=ERROR_INVALID_DATA)) ||
            (scenario==3 && (device_calls!=2 || driver_error()!=ERROR_FILE_NOT_FOUND))) {
            ++failures;printf("FAIL: loader scenario %d, stage %s, error %lu\n",scenario,driver_stage(),driver_error());
        }
        driver_close();
        GetCurrentDirectoryA(sizeof(after),after);
        if(strcmp(before,after) || !extracted[0] || !created_directory[0] ||
            GetFileAttributesA(extracted)!=INVALID_FILE_ATTRIBUTES ||
            GetFileAttributesA(created_directory)!=INVALID_FILE_ATTRIBUTES) {
            ++failures;printf("FAIL: loader cleanup/directory restoration scenario %d\n",scenario);
        }
    }
    return failures;
}
