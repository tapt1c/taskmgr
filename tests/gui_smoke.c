/* External UI smoke test; built for Wine/NT, not shipped to Windows 95.
   Exercises real windows and messages, without test hooks in the application. */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include "resource.h"
static HWND target,performance;
static DWORD expected_pid;
static BOOL CALLBACK find_target(HWND w,LPARAM unused)
{
    DWORD pid;char cls[80];
    (void)unused;GetWindowThreadProcessId(w,&pid);GetClassNameA(w,cls,sizeof(cls));
    if(pid==expected_pid && !strcmp(cls,"Taskmgr95Main")) target=w;
    return TRUE;
}
static int lists, buttons, failures, checks;
static BOOL CALLBACK count_visible(HWND w,LPARAM unused)
{
    char cls[80];
    RECT r,parent;
    (void)unused;
    GetClassNameA(w,cls,sizeof(cls));
    if(!strcmp(cls,"Taskmgr95Performance")) performance=w;
    if(!IsWindowVisible(w)) return TRUE;
    GetClassNameA(w,cls,sizeof(cls));
    if(!strcmp(cls,"SysListView32")) ++lists;
    if(!strcmp(cls,"Button")) ++buttons;
    if(!strcmp(cls,"SysListView32") || !strcmp(cls,"Button")) {
        GetWindowRect(w,&r);
        MapWindowPoints(NULL,GetParent(w),(POINT *)&r,2);
        GetClientRect(GetParent(w),&parent);
        if(r.left<0 || r.top<0 || r.right>parent.right || r.bottom>parent.bottom) ++failures;
    }
    return TRUE;
}
static void verify(int condition,const char *name)
{
    ++checks;
    if(!condition) { ++failures;printf("FAIL: %s\n",name); }
}
static void change_page(HWND tab,int page)
{
    /* Let the tab control generate its own notification in the target process.
       WM_NOTIFY cannot marshal a caller-owned NMHDR across processes. */
    SendMessageA(tab,TCM_SETCURFOCUS,page,0);
    Sleep(30);
}
static void check_page(int page)
{
    lists=buttons=0;
    EnumChildWindows(target,count_visible,0);
    verify(lists==(page==2?0:1),"only the selected page has a visible list");
    verify(buttons==(page==0?3:page==1?1:0),"buttons match the selected page");
}
static BOOL CALLBACK find_dialog(HWND w,LPARAM result)
{
    char cls[40];
    GetClassNameA(w,cls,sizeof(cls));
    if(!strcmp(cls,"#32770") && GetWindow(w,GW_OWNER)==target) *(HWND *)result=w;
    return TRUE;
}
static HWND wait_dialog(void)
{
    int i;
    HWND dialog=NULL;
    for(i=0;i<100 && !dialog;++i) { EnumWindows(find_dialog,(LPARAM)&dialog);Sleep(30); }
    return dialog;
}
static DWORD paint_hash(HWND w)
{
    HDC dc;
    RECT r;
    DWORD hash=0;
    int x,y;
    RedrawWindow(w,NULL,NULL,RDW_INVALIDATE | RDW_UPDATENOW);
    Sleep(50);
    dc=GetDC(w);GetClientRect(w,&r);
    for(y=0;y<r.bottom;y+=7) for(x=0;x<r.right;x+=7)
        hash=(hash*33)^GetPixel(dc,x,y);
    ReleaseDC(w,dc);
    return hash;
}
int main(int argc,char **argv)
{
    STARTUPINFOA start={0};
    PROCESS_INFORMATION process;
    HWND tab,dialog;
    DWORD before;
    WCHAR title[100];
    int i,page;
    char command[1024], hold[8];
    DWORD exit_code;
    if(argc!=2) { fprintf(stderr,"usage: gui_smoke.exe path-to-taskmgr95.exe\n");return 2; }
    start.cb=sizeof(start);
    snprintf(command,sizeof(command),"\"%s\"",argv[1]);
    if(!CreateProcessA(NULL,command,NULL,NULL,FALSE,0,NULL,NULL,&start,&process)) return 2;
    expected_pid=process.dwProcessId;
    for(i=0;i<200 && !target;++i) { EnumWindows(find_target,0);Sleep(30); }
    verify(target!=NULL,"application starts");
    if(!target) return 1;
    tab=GetDlgItem(target,301);
    verify(tab!=NULL,"tab control exists");
    verify(SendMessageA(tab,TCM_GETITEMCOUNT,0,0)==3,"three tabs");
    SendMessageA(target,WM_COMMAND,ID_ENGLISH,0);
    for(page=0;page<3;++page) {
        change_page(tab,page);Sleep(2200);check_page(page);
    }
    SendMessageA(target,WM_COMMAND,ID_PAUSE,0);
    for(i=0;i<60;++i) {
        page=i%3;change_page(tab,page);
        MoveWindow(target,8,8,i%2?420:780,i%2?330:580,TRUE);
        SendMessageA(target,WM_COMMAND,ID_REFRESH,0);
        check_page(page);
    }
    SendMessageA(target,WM_COMMAND,ID_RUSSIAN,0);
    GetWindowTextW(target,title,100);
    verify(title[0]==0x414 && title[1]==0x438,"Russian Unicode title");
    MoveWindow(target,8,8,420,330,TRUE);
    for(page=0;page<3;++page) { change_page(tab,page);check_page(page); }
    verify(performance!=NULL,"performance control found");
    verify((GetWindowLongA(performance,GWL_STYLE)&WS_HSCROLL)!=0,"small window has horizontal scrolling");
    verify((GetWindowLongA(performance,GWL_STYLE)&WS_VSCROLL)!=0,"small window has vertical scrolling");
    before=paint_hash(performance);
    SendMessageA(performance,WM_HSCROLL,SB_BOTTOM,0);
    verify(paint_hash(performance)!=before,"horizontal scrolling changes the rendered viewport");
    before=paint_hash(performance);
    SendMessageA(performance,WM_VSCROLL,SB_BOTTOM,0);
    verify(paint_hash(performance)!=before,"vertical scrolling changes the rendered viewport");
    MoveWindow(target,8,8,780,580,TRUE);
    verify((GetWindowLongA(performance,GWL_STYLE)&WS_HSCROLL)==0,"horizontal scrollbar hides after expansion");
    verify((GetWindowLongA(performance,GWL_STYLE)&WS_VSCROLL)==0,"vertical scrollbar hides after expansion");
    SendMessageA(target,WM_COMMAND,ID_ENGLISH,0);
    GetWindowTextW(target,title,100);
    verify(title[0]=='T' && title[1]=='a',"switch back to English");
    PostMessageA(target,WM_COMMAND,ID_ABOUT,0);
    dialog=wait_dialog();verify(dialog!=NULL,"Shell About dialog opens");
    if(dialog) PostMessageA(dialog,WM_COMMAND,IDOK,0);
    Sleep(200);
    PostMessageA(target,WM_COMMAND,ID_NEW,0);
    dialog=wait_dialog();verify(dialog!=NULL,"Run dialog opens");
    if(dialog) PostMessageA(dialog,WM_COMMAND,IDCANCEL,0);
    Sleep(200);
    /* Stay on Performance for a screenshot; caller closes after capture. */
    SendMessageA(target,WM_COMMAND,ID_RUSSIAN,0);
    change_page(tab,2);MoveWindow(target,8,8,420,330,TRUE);
    GetExitCodeProcess(process.hProcess,&exit_code);
    verify(exit_code==STILL_ACTIVE,"application survives UI stress");
    printf("%d checks, %d failures\n",checks,failures);
    fflush(stdout);
    if(GetEnvironmentVariableA("TM95_CAPTURE_HOLD",hold,sizeof(hold))) Sleep(45000);
    PostMessageA(target,WM_CLOSE,0,0);
    WaitForSingleObject(process.hProcess,5000);
    CloseHandle(process.hProcess);CloseHandle(process.hThread);
    return failures?1:0;
}
