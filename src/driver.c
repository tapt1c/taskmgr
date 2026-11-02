#include "driver.h"
#include "resource.h"
#include "version.h"
static HANDLE device=INVALID_HANDLE_VALUE;
static BOOL file_created;
static char driver_path[MAX_PATH],driver_directory[MAX_PATH];
static char attempt_name[MAX_PATH+8],attempt_file[MAX_PATH];
static DWORD resource_length,verified_bytes,absolute_error,relative_error;
static DWORD last_error;
static const char *last_stage="idle";
const char *driver_stage(void) { return last_stage; }
DWORD driver_error(void) { return last_error; }
void driver_close(void)
{
    if(device!=INVALID_HANDLE_VALUE) CloseHandle(device);
    device=INVALID_HANDLE_VALUE;
    if(file_created && driver_path[0]) DeleteFileA(driver_path);
    file_created=FALSE;
    driver_path[0]=0;
    if(driver_directory[0]) RemoveDirectoryA(driver_directory);
    driver_directory[0]=0;
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
static BOOL verify_file(const char *path,const BYTE *expected,DWORD length)
{
    HANDLE file;
    BYTE buffer[512];
    DWORD offset=0,count,got,i,error=0;
    last_stage="read back VxD file";
    file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) { last_error=GetLastError();return FALSE; }
    while(offset<length) {
        count=length-offset;if(count>sizeof(buffer)) count=sizeof(buffer);
        if(!ReadFile(file,buffer,count,&got,NULL)) { error=GetLastError();break; }
        if(got!=count) { error=ERROR_READ_FAULT;break; }
        for(i=0;i<got;++i) if(buffer[i]!=expected[offset+i]) { error=ERROR_INVALID_DATA;break; }
        if(error) break;
        offset+=got;
    }
    if(!error) {
        if(!ReadFile(file,buffer,1,&got,NULL)) error=GetLastError();
        else if(got) error=ERROR_INVALID_DATA;
    }
    CloseHandle(file);
    if(error) { last_error=error;return FALSE; }
    verified_bytes=offset;return TRUE;
}
static BOOL open_device(const char *path,const char *basename)
{
    char original[MAX_PATH];
    DWORD length,open_error;
    lstrcpyA(attempt_name,"\\\\.\\");lstrcatA(attempt_name,path);
    last_stage="CreateFile VxD (full path)";
    device=CreateFileA(attempt_name,0,0,NULL,CREATE_NEW,FILE_FLAG_DELETE_ON_CLOSE,NULL);
    if(device!=INVALID_HANDLE_VALUE) return TRUE;
    absolute_error=GetLastError();
    if(absolute_error!=ERROR_FILE_NOT_FOUND && absolute_error!=ERROR_PATH_NOT_FOUND) {
        last_error=absolute_error;return FALSE;
    }
    /* Also support the documented name-only VxD form. Scope the directory change
       to this synchronous call and restore it before sampling or any UI work. */
    last_stage="get original directory";
    length=GetCurrentDirectoryA(sizeof(original),original);
    if(!length || length>=sizeof(original)) {
        last_error=length?ERROR_FILENAME_EXCED_RANGE:GetLastError();return FALSE;
    }
    last_stage="set VxD directory";
    if(!SetCurrentDirectoryA(driver_directory)) { last_error=GetLastError();return FALSE; }
    lstrcpyA(attempt_name,"\\\\.\\");lstrcatA(attempt_name,basename);
    device=CreateFileA(attempt_name,0,0,NULL,CREATE_NEW,FILE_FLAG_DELETE_ON_CLOSE,NULL);
    open_error=device==INVALID_HANDLE_VALUE?GetLastError():0;
    relative_error=open_error;
    last_stage="restore original directory";
    if(!SetCurrentDirectoryA(original)) { last_error=GetLastError();return FALSE; }
    last_stage="CreateFile VxD (name)";
    if(open_error) { last_error=open_error;return FALSE; }
    return TRUE;
}
static BOOL load_resource(HINSTANCE instance,UINT resource_id,const char *basename)
{
    HRSRC resource;
    HGLOBAL loaded;
    const void *bytes;
    DWORD length,written=0,temp_length;
    char directory[MAX_PATH],temporary[MAX_PATH],short_path[MAX_PATH];
    DWORD short_length;
    HANDLE file;
    if(device!=INVALID_HANDLE_VALUE) return TRUE;
    /* Never attempt to load a VxD into NT/Wine. */
    if(!(GetVersion() & 0x80000000UL)) { last_stage="VxD requires Win9x";last_error=ERROR_NOT_SUPPORTED;return FALSE; }
    last_error=0;resource_length=verified_bytes=absolute_error=relative_error=0;
    attempt_name[0]=attempt_file[0]=0;last_stage="resource";
    resource=FindResourceA(instance,MAKEINTRESOURCEA(resource_id),RT_RCDATA);
    if(!resource) { last_error=GetLastError();return FALSE; }
    length=SizeofResource(instance,resource);resource_length=length;
    loaded=LoadResource(instance,resource);
    bytes=loaded?LockResource(loaded):NULL;
    if(!bytes || !length) { last_error=ERROR_INVALID_DATA;return FALSE; }
    last_stage="temporary path";
    temp_length=GetTempPathA(sizeof(directory),directory);
    if(!temp_length) { last_error=GetLastError();return FALSE; }
    if(temp_length>MAX_PATH-32) { last_error=ERROR_FILENAME_EXCED_RANGE;return FALSE; }
    if(!GetTempFileNameA(directory,"TM9",0,temporary)) { last_error=GetLastError();return FALSE; }
    /* Keep the filename equal to the module/DDB identity in our own directory. */
    last_stage="create VxD directory";
    if(!DeleteFileA(temporary)) { last_error=GetLastError();return FALSE; }
    if(!CreateDirectoryA(temporary,NULL)) { last_error=GetLastError();return FALSE; }
    lstrcpyA(driver_directory,temporary);
    lstrcpyA(driver_path,temporary);lstrcatA(driver_path,"\\");lstrcatA(driver_path,basename);
    lstrcpyA(attempt_file,driver_path);
    last_stage="extract VxD file";
    file=CreateFileA(driver_path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) { last_error=GetLastError();driver_close();return FALSE; }
    file_created=TRUE;
    if(!WriteFile(file,bytes,length,&written,NULL)) {
        last_error=GetLastError();CloseHandle(file);driver_close();return FALSE;
    }
    if(written!=length) {
        last_error=ERROR_WRITE_FAULT;CloseHandle(file);driver_close();return FALSE;
    }
    CloseHandle(file);
    /* Win9x VxD loading uses DOS paths even when TEMP contains long components. */
    last_stage="DOS 8.3 path";
    short_length=GetShortPathNameA(driver_path,short_path,sizeof(short_path));
    if(!short_length || short_length>=sizeof(short_path)) {
        last_error=short_length?ERROR_FILENAME_EXCED_RANGE:GetLastError();driver_close();return FALSE;
    }
    lstrcpyA(attempt_file,short_path);
    if(!verify_file(short_path,(const BYTE *)bytes,length) || !open_device(short_path,basename)) {
        driver_close();return FALSE;
    }
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

static void diagnose_loader(HINSTANCE instance,HANDLE log)
{
    DWORD original_error=last_error,builtin_error,probe_error,query_error=ERROR_INVALID_HANDLE;
    DWORD returned=0,written,length;
    const char *original_stage=last_stage;
    char report[768];
    HANDLE builtin;
    TM95_PROBE_SAMPLE sample;
    BOOL opened,queried=FALSE;
    /* Opening an existing OS device tests the namespace without any IOCTL. */
    builtin=CreateFileA("\\\\.\\VWIN32",0,0,NULL,0,0,NULL);
    builtin_error=builtin==INVALID_HANDLE_VALUE?GetLastError():0;
    if(builtin!=INVALID_HANDLE_VALUE) CloseHandle(builtin);
    opened=load_resource(instance,IDR_VXD_PROBE,"TM95TEST.VXD");
    probe_error=opened?0:last_error;
    if(opened) {
        ZeroMemory(&sample,sizeof(sample));queried=TRUE;
        if(!DeviceIoControl(device,TM95_IOCTL_QUERY,NULL,0,&sample,sizeof(sample),&returned,NULL)) query_error=GetLastError();
        else query_error=returned==sizeof(sample) && sample.size==sizeof(sample) && sample.magic==TM95_PROBE_MAGIC?0:ERROR_INVALID_DATA;
    }
    length=(DWORD)wsprintfA(report,"VWIN32 open error: %lu\r\nProbe open error: %lu\r\nProbe query attempted: %u\r\nProbe query error: %lu\r\nProbe readback bytes: %lu\r\nProbe device: %s\r\n",builtin_error,probe_error,(UINT)queried,query_error,verified_bytes,attempt_name);
    WriteFile(log,report,length,&written,NULL);
    driver_close();
    /* Probe diagnostics must not replace the original error shown in the UI. */
    last_error=original_error;last_stage=original_stage;
}
BOOL driver_enable(HINSTANCE instance)
{
    TM95_VXD_SAMPLE sample;
    BOOL result=load_resource(instance,IDR_VXD,"TM95STAT.VXD");
    if(result && !driver_sample(&sample)) { driver_close();result=FALSE; }
    if(!result && (GetVersion()&0x80000000UL)) {
        char report[1024];
        HANDLE file;
        DWORD length,written;
        length=(DWORD)wsprintfA(report,"Taskmgr95 " TM95_VERSION_STRING " VxD protocol %lu\r\nGetVersion: %08lX\r\nStage: %s\r\nError: %lu\r\nDevice: %s\r\nFile: %s\r\nEmbedded bytes: %lu\r\nReadback bytes: %lu\r\nFull-path error: %lu\r\nName-only error: %lu\r\n",TM95_PROTOCOL_VERSION,GetVersion(),last_stage,last_error,attempt_name,attempt_file,resource_length,verified_bytes,absolute_error,relative_error);
        file=CreateFileA(".\\taskmgr95-vxd.log",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
        if(file!=INVALID_HANDLE_VALUE) {
            WriteFile(file,report,length,&written,NULL);
            if((last_error==ERROR_FILE_NOT_FOUND || last_error==ERROR_PATH_NOT_FOUND) &&
                resource_length && verified_bytes==resource_length) diagnose_loader(instance,file);
            CloseHandle(file);
        }
    }
    return result;
}
