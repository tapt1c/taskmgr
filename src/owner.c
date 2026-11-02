#include "owner.h"
#define SID_BYTES 68
#define NAME_LENGTH 256
#define OWNER_CACHE 16
typedef struct {
    BYTE sid[SID_BYTES];
    DWORD length,tick;
    BOOL used,resolved;
    WCHAR name[NAME_LENGTH];
} OWNER_ENTRY;
static OWNER_ENTRY cache[OWNER_CACHE];
static BOOL available,win9x;
static WCHAR session_name[NAME_LENGTH];
static BOOL (WINAPI *open_token)(HANDLE,DWORD,PHANDLE);
static BOOL (WINAPI *token_info)(HANDLE,TOKEN_INFORMATION_CLASS,LPVOID,DWORD,PDWORD);
static BOOL (WINAPI *lookup_sid)(LPCWSTR,PSID,LPWSTR,LPDWORD,LPWSTR,LPDWORD,PSID_NAME_USE);
static DWORD (WINAPI *sid_length)(PSID);
static void copy_name(WCHAR *dest,const WCHAR *source,int count)
{
    int i=0;
    while(i<count-1 && source[i]) { dest[i]=source[i];++i; }
    dest[i]=0;
}
void owner_init(void)
{
    HMODULE advapi;
    available=FALSE;
    advapi=GetModuleHandleA("ADVAPI32.DLL");
    if(!advapi) return;
    win9x=(GetVersion() & 0x80000000UL)!=0;
    if(win9x) {
        char name[NAME_LENGTH];
        DWORD length=NAME_LENGTH;
        BOOL (WINAPI *get_user)(LPSTR,LPDWORD);
        /* Decorative session attribution requested for Win95, not token ownership. */
        get_user=(BOOL (WINAPI *)(LPSTR,LPDWORD))GetProcAddress(advapi,"GetUserNameA");
        if(get_user && get_user(name,&length) && name[0])
            available=MultiByteToWideChar(CP_ACP,0,name,-1,session_name,NAME_LENGTH)!=0;
        return;
    }
    open_token=(BOOL (WINAPI *)(HANDLE,DWORD,PHANDLE))GetProcAddress(advapi,"OpenProcessToken");
    token_info=(BOOL (WINAPI *)(HANDLE,TOKEN_INFORMATION_CLASS,LPVOID,DWORD,PDWORD))GetProcAddress(advapi,"GetTokenInformation");
    lookup_sid=(BOOL (WINAPI *)(LPCWSTR,PSID,LPWSTR,LPDWORD,LPWSTR,LPDWORD,PSID_NAME_USE))GetProcAddress(advapi,"LookupAccountSidW");
    sid_length=(DWORD (WINAPI *)(PSID))GetProcAddress(advapi,"GetLengthSid");
    available=open_token && token_info && lookup_sid && sid_length;
}
BOOL owner_available(void) { return available; }
BOOL owner_name(DWORD pid,WCHAR *name,int count)
{
    HANDLE process,token=NULL;
    union { TOKEN_USER alignment;BYTE bytes[1024]; } data;
    TOKEN_USER *user=(TOKEN_USER *)data.bytes;
    WCHAR account[NAME_LENGTH],domain[NAME_LENGTH];
    DWORD size=0,length,now=GetTickCount(),account_size=NAME_LENGTH,domain_size=NAME_LENGTH;
    SID_NAME_USE type;
    OWNER_ENTRY *entry=NULL;
    int i,j,slot=0;
    BOOL equal,ok;
    if(count<1) return FALSE;
    name[0]=0;
    if(!available) return FALSE;
    if(win9x) { copy_name(name,session_name,count);return TRUE; }
    if(!pid) return FALSE;
    /* Limited query is optional on newer NT; the fallback works on older NT. */
    process=OpenProcess(0x1000,FALSE,pid);
    if(!process) process=OpenProcess(PROCESS_QUERY_INFORMATION,FALSE,pid);
    if(!process) return FALSE;
    ok=open_token(process,TOKEN_QUERY,&token);
    CloseHandle(process);
    if(!ok) return FALSE;
    ok=token_info(token,TokenUser,data.bytes,sizeof(data.bytes),&size);
    if(!ok || size<sizeof(TOKEN_USER)) { CloseHandle(token);return FALSE; }
    length=sid_length(user->User.Sid);
    if(!length || length>SID_BYTES) { CloseHandle(token);return FALSE; }
    for(i=0;i<OWNER_CACHE;++i) {
        if(cache[i].used && cache[i].length==length && (DWORD)(now-cache[i].tick)<60000) {
            equal=TRUE;
            for(j=0;j<(int)length;++j) if(cache[i].sid[j]!=((BYTE *)user->User.Sid)[j]) { equal=FALSE;break; }
            if(equal) { entry=&cache[i];break; }
        }
        if(!cache[i].used || (DWORD)(now-cache[i].tick)>(DWORD)(now-cache[slot].tick)) slot=i;
    }
    if(!entry) {
        entry=&cache[slot];ZeroMemory(entry,sizeof(*entry));
        entry->used=TRUE;entry->length=length;entry->tick=now;
        for(j=0;j<(int)length;++j) entry->sid[j]=((BYTE *)user->User.Sid)[j];
        account[0]=domain[0]=0;
        entry->resolved=lookup_sid(NULL,user->User.Sid,account,&account_size,domain,&domain_size,&type) && account[0];
        if(entry->resolved) {
            i=0;
            if(domain_size && domain_size+account_size+2<=NAME_LENGTH) {
                while(domain[i]) { entry->name[i]=domain[i];++i; }
                entry->name[i++]='\\';
            }
            j=0;
            while(i<NAME_LENGTH-1 && account[j]) entry->name[i++]=account[j++];
            entry->name[i]=0;
        }
    }
    CloseHandle(token);
    if(entry->resolved) copy_name(name,entry->name,count);
    return entry->resolved;
}
