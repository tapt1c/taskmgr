#include "process_policy.h"
static int equal_name(const char *a,const char *b)
{
    unsigned char left,right;
    do {
        left=(unsigned char)*a++;right=(unsigned char)*b++;
        if(left>='a' && left<='z') left=(unsigned char)(left-'a'+'A');
        if(right>='a' && right<='z') right=(unsigned char)(right-'a'+'A');
        if(left!=right) return 0;
    } while(left);
    return 1;
}
int tm_process_name_protected(const char *path)
{
    static const char *const names[]={
        "KERNEL32.DLL","KRNL386.EXE","WIN386.EXE","USER.EXE","USER32.DLL",
        "GDI.EXE","GDI32.DLL","MSGSRV32.EXE","MPREXE.EXE","MMTASK.TSK",
        "SYSTEM","SYSTEM IDLE PROCESS","SMSS.EXE","CSRSS.EXE","WININIT.EXE",
        "WINLOGON.EXE","SERVICES.EXE","LSASS.EXE"
    };
    const char *base,*p;
    unsigned int i;
    if(!path || !*path) return 1;
    base=path;
    for(p=path;*p;++p) if(*p=='\\' || *p=='/') base=p+1;
    if(!*base) return 1;
    for(i=0;i<sizeof(names)/sizeof(names[0]);++i)
        if(equal_name(base,names[i])) return 1;
    return 0;
}
