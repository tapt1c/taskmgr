#include "driver.h"
#include "resource.h"
static HANDLE device=INVALID_HANDLE_VALUE;
static char driver_path[MAX_PATH];
static DWORD last_error;
DWORD driver_error(void) { return last_error; }
void driver_close(void)
{
    if(device!=INVALID_HANDLE_VALUE) CloseHandle(device);
    device=INVALID_HANDLE_VALUE;
    if(driver_path[0]) DeleteFileA(driver_path);
    driver_path[0]=0;
}
BOOL driver_sample(TM95_VXD_SAMPLE *sample)
{
    DWORD returned=0;
    ZeroMemory(sample,sizeof(*sample));
    if(device==INVALID_HANDLE_VALUE) return FALSE;
    if(!DeviceIoControl(device,TM95_IOCTL_QUERY,NULL,0,sample,sizeof(*sample),&returned,NULL)) {
        last_error=GetLastError();return FALSE;
    }
    if(returned!=sizeof(*sample) || sample->size!=sizeof(*sample) ||
        sample->version!=TM95_PROTOCOL_VERSION || sample->capabilities!=3) {
        last_error=ERROR_INVALID_DATA;return FALSE;
    }
    return TRUE;
}
BOOL driver_enable(HINSTANCE instance)
{
    HRSRC resource;
    HGLOBAL loaded;
    const void *bytes;
    DWORD length,written=0,temp_length;
    char directory[MAX_PATH],temporary[MAX_PATH],name[MAX_PATH+8];
    HANDLE file;
    TM95_VXD_SAMPLE sample;
    if(device!=INVALID_HANDLE_VALUE) return TRUE;
    /* Never attempt to load a VxD into NT/Wine. */
    if(!(GetVersion() & 0x80000000UL)) { last_error=ERROR_NOT_SUPPORTED;return FALSE; }
    resource=FindResourceA(instance,MAKEINTRESOURCEA(IDR_VXD),RT_RCDATA);
    if(!resource) { last_error=GetLastError();return FALSE; }
    length=SizeofResource(instance,resource);
    loaded=LoadResource(instance,resource);
    bytes=loaded?LockResource(loaded):NULL;
    if(!bytes || !length) { last_error=ERROR_INVALID_DATA;return FALSE; }
    temp_length=GetTempPathA(sizeof(directory),directory);
    if(!temp_length || temp_length>MAX_PATH-20 ||
        !GetTempFileNameA(directory,"TM9",0,temporary)) { last_error=GetLastError();return FALSE; }
    file=CreateFileA(temporary,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) { last_error=GetLastError();DeleteFileA(temporary);return FALSE; }
    if(!WriteFile(file,bytes,length,&written,NULL) || written!=length) {
        last_error=GetLastError();CloseHandle(file);DeleteFileA(temporary);return FALSE;
    }
    CloseHandle(file);
    lstrcpyA(driver_path,temporary);lstrcatA(driver_path,".vxd");
    /* MoveFile never overwrites another task's driver file. */
    if(!MoveFileA(temporary,driver_path)) {
        last_error=GetLastError();DeleteFileA(temporary);driver_path[0]=0;return FALSE;
    }
    lstrcpyA(name,"\\\\.\\");lstrcatA(name,driver_path);
    device=CreateFileA(name,0,0,NULL,0,FILE_FLAG_DELETE_ON_CLOSE,NULL);
    if(device==INVALID_HANDLE_VALUE) { last_error=GetLastError();driver_close();return FALSE; }
    if(!driver_sample(&sample)) { driver_close();return FALSE; }
    return TRUE;
}
