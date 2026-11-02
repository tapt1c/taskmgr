#include "driver.h"
#include "resource.h"
static HANDLE device=INVALID_HANDLE_VALUE;
static char driver_path[MAX_PATH],attempt_name[MAX_PATH+8];
static DWORD resource_length;
static DWORD last_error;
static const char *last_stage="idle";
const char *driver_stage(void) { return last_stage; }
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
    last_stage="DeviceIoControl";
    if(!DeviceIoControl(device,TM95_IOCTL_QUERY,NULL,0,sample,sizeof(*sample),&returned,NULL)) {
        last_error=GetLastError();return FALSE;
    }
    if(returned!=sizeof(*sample) || sample->size!=sizeof(*sample) ||
        sample->version!=TM95_PROTOCOL_VERSION || (sample->capabilities&(TM95_CAP_UPTIME|TM95_CAP_PAGE_COUNTS))!=(TM95_CAP_UPTIME|TM95_CAP_PAGE_COUNTS)) {
        last_error=ERROR_INVALID_DATA;return FALSE;
    }
    last_error=0;return TRUE;
}
static BOOL enable_driver(HINSTANCE instance)
{
    HRSRC resource;
    HGLOBAL loaded;
    const void *bytes;
    DWORD length,written=0,temp_length;
    char directory[MAX_PATH],temporary[MAX_PATH],short_path[MAX_PATH],name[MAX_PATH+8];
    DWORD short_length;
    HANDLE file;
    TM95_VXD_SAMPLE sample;
    if(device!=INVALID_HANDLE_VALUE) return TRUE;
    /* Never attempt to load a VxD into NT/Wine. */
    if(!(GetVersion() & 0x80000000UL)) { last_stage="VxD requires Win9x";last_error=ERROR_NOT_SUPPORTED;return FALSE; }
    last_error=0;resource_length=0;attempt_name[0]=0;last_stage="resource";
    resource=FindResourceA(instance,MAKEINTRESOURCEA(IDR_VXD),RT_RCDATA);
    if(!resource) { last_error=GetLastError();return FALSE; }
    length=SizeofResource(instance,resource);resource_length=length;
    loaded=LoadResource(instance,resource);
    bytes=loaded?LockResource(loaded):NULL;
    if(!bytes || !length) { last_error=ERROR_INVALID_DATA;return FALSE; }
    last_stage="temporary path";
    temp_length=GetTempPathA(sizeof(directory),directory);
    if(!temp_length) { last_error=GetLastError();return FALSE; }
    if(temp_length>MAX_PATH-20) { last_error=ERROR_FILENAME_EXCED_RANGE;return FALSE; }
    if(!GetTempFileNameA(directory,"TM9",0,temporary)) { last_error=GetLastError();return FALSE; }
    last_stage="extract file";
    file=CreateFileA(temporary,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) { last_error=GetLastError();DeleteFileA(temporary);return FALSE; }
    if(!WriteFile(file,bytes,length,&written,NULL)) {
        last_error=GetLastError();CloseHandle(file);DeleteFileA(temporary);return FALSE;
    }
    if(written!=length) {
        last_error=ERROR_WRITE_FAULT;CloseHandle(file);DeleteFileA(temporary);return FALSE;
    }
    CloseHandle(file);
    /* Replace .TMP rather than appending .TMP.VXD: keep the basename DOS 8.3. */
    lstrcpyA(driver_path,temporary);
    lstrcpyA(driver_path+lstrlenA(driver_path)-3,"VXD");
    last_stage="rename VXD";
    /* MoveFile never overwrites another task's driver file. */
    if(!MoveFileA(temporary,driver_path)) {
        last_error=GetLastError();DeleteFileA(temporary);driver_path[0]=0;return FALSE;
    }
    /* Win9x VxD loading uses DOS paths even when TEMP contains long components. */
    last_stage="DOS 8.3 path";
    short_length=GetShortPathNameA(driver_path,short_path,sizeof(short_path));
    if(!short_length || short_length>=sizeof(short_path)) {
        last_error=short_length?ERROR_FILENAME_EXCED_RANGE:GetLastError();driver_close();return FALSE;
    }
    lstrcpyA(name,"\\\\.\\");lstrcatA(name,short_path);
    lstrcpyA(attempt_name,name);last_stage="CreateFile VxD (8.3)";
    device=CreateFileA(name,0,0,NULL,0,FILE_FLAG_DELETE_ON_CLOSE,NULL);
    if(device==INVALID_HANDLE_VALUE) { last_error=GetLastError();driver_close();return FALSE; }
    if(!driver_sample(&sample)) { driver_close();return FALSE; }
    last_error=0;return TRUE;
}

BOOL driver_cad(BOOL enable)
{
    TM95_CAD_REQUEST request;
    DWORD returned=0;
    last_stage="VxD Ctrl+Alt+Del";
    if(device==INVALID_HANDLE_VALUE) { last_error=ERROR_INVALID_HANDLE;return FALSE; }
    request.size=sizeof(request);request.version=TM95_PROTOCOL_VERSION;request.enable=enable?1:0;
    if(!DeviceIoControl(device,TM95_IOCTL_CAD,&request,sizeof(request),NULL,0,&returned,NULL)) {
        last_error=GetLastError();return FALSE;
    }
    last_error=0;return TRUE;
}

BOOL driver_enable(HINSTANCE instance)
{
    BOOL result=enable_driver(instance);
    if(!result && (GetVersion()&0x80000000UL)) {
        char report[1024];
        HANDLE file;
        DWORD length,written;
        length=(DWORD)wsprintfA(report,"Taskmgr95 VxD protocol %lu\r\nGetVersion: %08lX\r\nStage: %s\r\nError: %lu\r\nDevice: %s\r\nEmbedded bytes: %lu\r\n",TM95_PROTOCOL_VERSION,GetVersion(),last_stage,last_error,attempt_name,resource_length);
        file=CreateFileA(".\\taskmgr95-vxd.log",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
        if(file!=INVALID_HANDLE_VALUE) { WriteFile(file,report,length,&written,NULL);CloseHandle(file); }
    }
    return result;
}
