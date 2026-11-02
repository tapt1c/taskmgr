/* External UI smoke test; built for Wine/NT, not shipped to Windows 95.
   Exercises real windows and messages, without test hooks in the application. */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
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
static DWORD black_samples(HWND w)
{
    HDC dc;
    RECT r;
    DWORD black=0;
    int x,y;
    RedrawWindow(w,NULL,NULL,RDW_INVALIDATE | RDW_UPDATENOW);
    Sleep(50);
    dc=GetDC(w);GetClientRect(w,&r);
    for(y=0;y<r.bottom;y+=7) for(x=0;x<r.right;x+=7)
        if(GetPixel(dc,x,y)==RGB(0,0,0)) ++black;
    ReleaseDC(w,dc);
    return black;
}
/* Measure the two actual history plots at the right edge of the panel.
   This catches regressions where the graphs become thin strips even though
   the overall panel remains visible. Tiny text strokes are ignored. */
static int history_plot_height(HWND w)
{
    HDC dc;
    RECT r;
    COLORREF pixel;
    int y,run=0,bands=0,shortest=10000;
    RedrawWindow(w,NULL,NULL,RDW_INVALIDATE | RDW_UPDATENOW);Sleep(50);
    dc=GetDC(w);GetClientRect(w,&r);
    for(y=0;y<=r.bottom;++y) {
        pixel=y<r.bottom?GetPixel(dc,r.right-30,y):CLR_INVALID;
        if(pixel==RGB(0,0,0) || pixel==RGB(0,80,40) || pixel==RGB(0,255,0)) ++run;
        else {
            if(run>=20) { ++bands;if(run<shortest) shortest=run; }
            run=0;
        }
    }
    ReleaseDC(w,dc);
    return bands>=2?shortest:0;
}
static HANDLE app_process;
static LPVOID remote_buffer;
static HWND apps_list,fixture_window;
static HICON fixture_icon;
static HANDLE fixture_thread_handle;
static int remote_find(HWND window)
{
    LVFINDINFOA info;
    ZeroMemory(&info,sizeof(info));info.flags=LVFI_PARAM;info.lParam=(LPARAM)window;
    if(!WriteProcessMemory(app_process,remote_buffer,&info,sizeof(info),NULL)) return -1;
    return (int)SendMessageA(apps_list,LVM_FINDITEMA,(WPARAM)-1,(LPARAM)remote_buffer);
}
static BOOL remote_item(int index,LVITEMA *item)
{
    ZeroMemory(item,sizeof(*item));item->mask=LVIF_IMAGE | LVIF_PARAM;item->iItem=index;
    if(index<0 || !WriteProcessMemory(app_process,remote_buffer,item,sizeof(*item),NULL)) return FALSE;
    if(!SendMessageA(apps_list,LVM_GETITEMA,0,(LPARAM)remote_buffer)) return FALSE;
    return ReadProcessMemory(app_process,remote_buffer,item,sizeof(*item),NULL)!=0;
}
static BOOL icon_is_colored(int index)
{
    RECT r;
    HDC dc;
    COLORREF color;
    int x,y,colored=0;
    r.left=LVIR_ICON;r.top=r.right=r.bottom=0;
    SendMessageA(apps_list,LVM_ENSUREVISIBLE,index,FALSE);
    if(!WriteProcessMemory(app_process,remote_buffer,&r,sizeof(r),NULL)) return FALSE;
    if(!SendMessageA(apps_list,LVM_GETITEMRECT,index,(LPARAM)remote_buffer)) return FALSE;
    if(!ReadProcessMemory(app_process,remote_buffer,&r,sizeof(r),NULL)) return FALSE;
    RedrawWindow(apps_list,NULL,NULL,RDW_INVALIDATE | RDW_UPDATENOW);Sleep(50);
    dc=GetDC(apps_list);
    for(y=r.top;y<r.bottom;++y) for(x=r.left;x<r.right;++x) {
        color=GetPixel(dc,x,y);
        if(color!=CLR_INVALID && GetRValue(color)>100 && GetGValue(color)>100 && GetBValue(color)<100) ++colored;
    }
    ReleaseDC(apps_list,dc);
    return colored>2;
}
static void select_column(HWND dialog,int index)
{
    SendDlgItemMessageA(dialog,IDC_COLUMNS_LIST,LB_SETCURSEL,index,0);
    SendMessageA(dialog,WM_COMMAND,MAKEWPARAM(IDC_COLUMNS_LIST,LBN_SELCHANGE),(LPARAM)GetDlgItem(dialog,IDC_COLUMNS_LIST));
}
static void show_column(HWND dialog)
{
    CheckDlgButton(dialog,IDC_COLUMN_VISIBLE,BST_CHECKED);
    SendMessageA(dialog,WM_COMMAND,MAKEWPARAM(IDC_COLUMN_VISIBLE,BN_CLICKED),0);
}
static LRESULT CALLBACK fixture_proc(HWND w,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==WM_GETICON) return (LRESULT)fixture_icon;
    if(message==WM_DESTROY) { PostQuitMessage(0);return 0; }
    return DefWindowProcA(w,message,wp,lp);
}
static DWORD WINAPI fixture_thread(LPVOID ready)
{
    WNDCLASSA cls={0};
    MSG message;
    cls.lpfnWndProc=fixture_proc;cls.hInstance=GetModuleHandleA(NULL);
    fixture_icon=LoadIconA(cls.hInstance,MAKEINTRESOURCEA(1));
    cls.hIcon=fixture_icon;cls.lpszClassName="TM95IconFixture";
    RegisterClassA(&cls);
    fixture_window=CreateWindowExA(0,cls.lpszClassName,"00 - Icon example",WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        520,40,240,100,NULL,NULL,cls.hInstance,NULL);
    SetClassLongPtrA(fixture_window,GCLP_HICONSM,(LONG_PTR)fixture_icon);
    SendMessageA(fixture_window,WM_SETICON,ICON_SMALL,(LPARAM)fixture_icon);
    PeekMessageA(&message,NULL,0,0,PM_NOREMOVE);
    SetEvent((HANDLE)ready);
    while(GetMessageA(&message,NULL,0,0)>0) { TranslateMessage(&message);DispatchMessageA(&message); }
    return 0;
}
static void applications_workflow(HWND tab,HANDLE process)
{
    static const DWORD modes[4]={LVS_REPORT,LVS_ICON,LVS_SMALLICON,LVS_LIST};
    HWND dialog,header;
    LVITEMA item;
    int i,index;
    BOOL modes_ok=TRUE,columns_ok;
    app_process=process;
    remote_buffer=VirtualAllocEx(process,NULL,2048,MEM_COMMIT | MEM_RESERVE,PAGE_READWRITE);
    verify(remote_buffer!=NULL,"can inspect the real applications control");
    if(!remote_buffer) return;
    apps_list=FindWindowExA(GetWindow(target,GW_CHILD),NULL,"SysListView32",NULL);
    /* Find the visible list via the page, irrespective of sibling Z-order. */
    change_page(tab,0);
    {
        HWND page=GetWindow(target,GW_CHILD);
        while(page) {
            char name[64];GetClassNameA(page,name,sizeof(name));
            if(!strcmp(name,"Taskmgr95Page") && IsWindowVisible(page)) {
                apps_list=GetDlgItem(page,302);break;
            }
            page=GetWindow(page,GW_HWNDNEXT);
        }
    }
    {
        HANDLE ready=CreateEventA(NULL,FALSE,FALSE,NULL);
        DWORD thread_id;
        fixture_thread_handle=CreateThread(NULL,0,fixture_thread,ready,0,&thread_id);
        WaitForSingleObject(ready,5000);CloseHandle(ready);
    }
    SendMessageA(target,WM_COMMAND,ID_REFRESH,0);
    index=remote_find(fixture_window);
    verify(index>=0 && remote_item(index,&item) && item.iImage>=0 && icon_is_colored(index),"application icon is displayed, including the EXE fallback");
    for(i=0;i<4;++i) {
        SendMessageA(target,WM_COMMAND,ID_VIEW_DETAILS+i,0);
        if(((DWORD)GetWindowLongA(apps_list,GWL_STYLE)&LVS_TYPEMASK)!=modes[i]) modes_ok=FALSE;
    }
    verify(modes_ok,"all four view modes work");
    SendMessageA(target,WM_COMMAND,ID_VIEW_ICONS,0);
    verify(SendMessageA(apps_list,LVM_GETIMAGELIST,LVSIL_SMALL,0)==0,"icons can be hidden");
    SendMessageA(target,WM_COMMAND,ID_VIEW_ICONS,0);
    SendMessageA(target,WM_COMMAND,ID_VIEW_DETAILS,0);
    PostMessageA(target,WM_COMMAND,ID_VIEW_COLUMNS,0);
    dialog=wait_dialog();verify(dialog!=NULL,"column editor opens");
    if(dialog) {
        select_column(dialog,2);show_column(dialog);
        SetDlgItemTextA(dialog,IDC_COLUMN_WIDTH,"77");
        SendMessageA(dialog,WM_COMMAND,IDC_COLUMN_UP,0);
        select_column(dialog,3);show_column(dialog);
        select_column(dialog,4);show_column(dialog);
        SendMessageA(dialog,WM_COMMAND,IDOK,0);
    }
    header=(HWND)SendMessageA(apps_list,LVM_GETHEADER,0,0);
    columns_ok=Header_GetItemCount(header)==5 && ListView_GetColumnWidth(apps_list,1)==77;
    SendMessageA(target,WM_COMMAND,ID_REFRESH,0);
    verify(columns_ok && Header_GetItemCount(header)==5,"column visibility, order and width survive refresh");
    /* A user-resized Task column must not be reset on window resize. */
    ListView_SetColumnWidth(apps_list,0,190);
    MoveWindow(target,8,8,780,580,TRUE);
    verify(ListView_GetColumnWidth(apps_list,0)==190,"manual Task width survives window resize");
    SendMessageA(target,WM_COMMAND,ID_SORT_PID,0);
    verify(remote_find(fixture_window)>=0,"sorting retains the application row");
    SendMessageA(target,WM_COMMAND,ID_VIEW_SMALL,0);
    VirtualFreeEx(process,remote_buffer,0,MEM_RELEASE);remote_buffer=NULL;
}
static HWND process_list(HWND tab)
{
    HWND page;
    change_page(tab,1);
    for(page=GetWindow(target,GW_CHILD);page;page=GetWindow(page,GW_HWNDNEXT))
        if(IsWindowVisible(page) && GetDlgItem(page,303)) return GetDlgItem(page,303);
    return NULL;
}
static BOOL process_cell(HWND list,int row,int column,WCHAR *text)
{
    LVITEMW item={0};
    item.iSubItem=column;item.cchTextMax=256;item.pszText=(WCHAR *)((BYTE *)remote_buffer+512);
    text[0]=0;
    if(row<0 || !WriteProcessMemory(app_process,remote_buffer,&item,sizeof(item),NULL)) return FALSE;
    if(!SendMessageA(list,LVM_GETITEMTEXTW,row,(LPARAM)remote_buffer)) return FALSE;
    if(!ReadProcessMemory(app_process,item.pszText,text,256*sizeof(*text),NULL)) return FALSE;
    text[255]=0;return TRUE;
}
static void memory_workflow(HWND list,int own_row)
{
    WCHAR text[256];
    SIZE_T before=0,after=0,previous=(SIZE_T)-1,value;
    BYTE *allocation;
    int i;
    BOOL sorted=TRUE,unknown=FALSE;
    if(process_cell(list,own_row,6,text)) before=(SIZE_T)wcstoull(text,NULL,10);
    verify(before>0,"RAM column contains a real working set reading");
    allocation=VirtualAlloc(NULL,16*1024*1024,MEM_COMMIT | MEM_RESERVE,PAGE_READWRITE);
    if(allocation) for(i=0;i<16*1024*1024;i+=4096) allocation[i]=1;
    SendMessageA(target,WM_COMMAND,ID_REFRESH,0);
    /* At this point the table is still sorted by image name. Find our PID again. */
    {
        LVFINDINFOA find={0};
        find.flags=LVFI_PARAM;find.lParam=GetCurrentProcessId();
        WriteProcessMemory(app_process,remote_buffer,&find,sizeof(find),NULL);
        own_row=(int)SendMessageA(list,LVM_FINDITEMA,(WPARAM)-1,(LPARAM)remote_buffer);
    }
    if(process_cell(list,own_row,6,text)) after=(SIZE_T)wcstoull(text,NULL,10);
    verify(allocation && after>=before+8192,"RAM reading increases after touching allocated pages");
    SendMessageA(target,WM_COMMAND,ID_PROC_SORT_MEMORY,0);
    SendMessageA(target,WM_COMMAND,ID_PROC_SORT_DESCENDING,0);
    for(i=0;i<ListView_GetItemCount(list);++i) {
        if(!process_cell(list,i,6,text)) { sorted=FALSE;break; }
        if(!wcscmp(text,L"N/A")) { unknown=TRUE;continue; }
        value=(SIZE_T)wcstoull(text,NULL,10);
        if(unknown || value>previous) sorted=FALSE;
        previous=value;
    }
    verify(sorted,"RAM sorts numerically with unavailable readings last");
    if(allocation) VirtualFree(allocation,0,MEM_RELEASE);
}
static void processes_workflow(HWND tab,HANDLE process)
{
    HWND list=process_list(tab),dialog,header;
    LVFINDINFOA find={0};
    LVITEMW item={0};
    WCHAR text[256],user[256],*base;
    DWORD length=256;
    int index=-1;
    app_process=process;
    remote_buffer=VirtualAllocEx(process,NULL,2048,MEM_COMMIT | MEM_RESERVE,PAGE_READWRITE);
    verify(list && remote_buffer,"can inspect the processes control");
    if(!list || !remote_buffer) return;
    SendMessageA(target,WM_COMMAND,ID_REFRESH,0);
    find.flags=LVFI_PARAM;find.lParam=GetCurrentProcessId();
    WriteProcessMemory(process,remote_buffer,&find,sizeof(find),NULL);
    index=(int)SendMessageA(list,LVM_FINDITEMA,(WPARAM)-1,(LPARAM)remote_buffer);
    item.iSubItem=1;item.cchTextMax=256;item.pszText=(WCHAR *)((BYTE *)remote_buffer+512);
    WriteProcessMemory(process,remote_buffer,&item,sizeof(item),NULL);
    text[0]=user[0]=0;
    SendMessageA(list,LVM_GETITEMTEXTW,index,(LPARAM)remote_buffer);
    ReadProcessMemory(process,item.pszText,text,sizeof(text),NULL);text[255]=0;
    base=wcsrchr(text,L'\\');base=base?base+1:text;
    verify(index>=0 && GetUserNameW(user,&length) && !_wcsicmp(base,user),"own process shows its token user name");
    verify(Header_GetItemCount((HWND)SendMessageA(list,LVM_GETHEADER,0,0))==7 &&
        ListView_GetColumnWidth(list,2)==83,"alpha 7 preferences survive adding the RAM column");
    memory_workflow(list,index);
    PostMessageA(target,WM_COMMAND,ID_VIEW_COLUMNS,0);
    dialog=wait_dialog();verify(dialog!=NULL,"process column editor opens");
    if(dialog) {
        select_column(dialog,1);SetDlgItemTextA(dialog,IDC_COLUMN_WIDTH,"133");
        SendMessageA(dialog,WM_COMMAND,IDC_COLUMN_DOWN,0);
        select_column(dialog,5);
        CheckDlgButton(dialog,IDC_COLUMN_VISIBLE,BST_UNCHECKED);
        SendMessageA(dialog,WM_COMMAND,IDC_COLUMN_VISIBLE,0);
        select_column(dialog,6);SetDlgItemTextA(dialog,IDC_COLUMN_WIDTH,"99");
        SendMessageA(dialog,WM_COMMAND,IDC_COLUMN_UP,0);
        SendMessageA(dialog,WM_COMMAND,IDOK,0);
    }
    header=(HWND)SendMessageA(list,LVM_GETHEADER,0,0);
    verify(Header_GetItemCount(header)==6 && ListView_GetColumnWidth(list,2)==133 && ListView_GetColumnWidth(list,5)==99,
        "process columns can be hidden, reordered and resized");
    ListView_SetColumnWidth(list,0,171);
    SendMessageA(target,WM_COMMAND,ID_PROC_SORT_PID,0);
    SendMessageA(target,WM_COMMAND,ID_PROC_SORT_DESCENDING,0);
    /* Selection remains a PID even when PID is moved and the table is sorted. */
    WriteProcessMemory(process,remote_buffer,&find,sizeof(find),NULL);
    index=(int)SendMessageA(list,LVM_FINDITEMA,(WPARAM)-1,(LPARAM)remote_buffer);
    ZeroMemory(&item,sizeof(item));item.state=item.stateMask=LVIS_SELECTED | LVIS_FOCUSED;
    WriteProcessMemory(process,remote_buffer,&item,sizeof(item),NULL);
    SendMessageA(list,LVM_SETITEMSTATE,index,(LPARAM)remote_buffer);
    MoveWindow(target,8,8,400,450,TRUE);
    SendMessageA(target,WM_COMMAND,ID_REFRESH,0);
    index=(int)SendMessageA(list,LVM_GETNEXTITEM,(WPARAM)-1,LVNI_SELECTED);
    ZeroMemory(&item,sizeof(item));item.mask=LVIF_PARAM;item.iItem=index;
    WriteProcessMemory(process,remote_buffer,&item,sizeof(item),NULL);
    SendMessageA(list,LVM_GETITEMW,0,(LPARAM)remote_buffer);
    ReadProcessMemory(process,remote_buffer,&item,sizeof(item),NULL);
    verify(index>=0 && (DWORD)item.lParam==GetCurrentProcessId() && ListView_GetColumnWidth(list,0)==171,
        "PID selection and manual process width survive sorting, resize and refresh");
    VirtualFreeEx(process,remote_buffer,0,MEM_RELEASE);remote_buffer=NULL;
}
int main(int argc,char **argv)
{
    STARTUPINFOA start={0};
    PROCESS_INFORMATION process;
    HWND tab,dialog;
    RECT bounds;
    WCHAR title[100];
    int i,page,graph_height;
    char command[1024], hold[8];
    DWORD exit_code;
    if(argc!=2) { fprintf(stderr,"usage: gui_smoke.exe path-to-taskmgr95.exe\n");return 2; }
    start.cb=sizeof(start);
    snprintf(command,sizeof(command),"\"%s\"",argv[1]);
    if(!CreateProcessA(NULL,command,NULL,NULL,FALSE,0,NULL,NULL,&start,&process)) return 2;
    WaitForInputIdle(process.hProcess,5000);
    expected_pid=process.dwProcessId;
    for(i=0;i<200 && !target;++i) { EnumWindows(find_target,0);Sleep(30); }
    verify(target!=NULL,"application starts");
    if(!target) return 1;
    GetWindowTextW(target,title,100);
    verify(wcsstr(title,L"alpha 9")!=NULL,"alpha 9 build is running");
    GetWindowRect(target,&bounds);
    verify(bounds.right-bounds.left==400 && bounds.bottom-bounds.top==450,"initial window matches 400x450 reference");
    tab=GetDlgItem(target,301);
    verify(tab!=NULL,"tab control exists");
    verify(SendMessageA(tab,TCM_GETITEMCOUNT,0,0)==3,"three tabs");
    SendMessageA(target,WM_COMMAND,ID_ENGLISH,0);
    PostMessageA(target,WM_COMMAND,ID_DRIVER,0);
    dialog=wait_dialog();verify(dialog!=NULL,"NT refuses VxD loading with an error dialog");
    if(dialog) SendMessageA(dialog,WM_COMMAND,IDOK,0);
    Sleep(100);
    PostMessageA(target,WM_COMMAND,ID_CAD,0);
    dialog=wait_dialog();verify(dialog!=NULL,"NT refuses the Win9x CAD hook");
    if(dialog) SendMessageA(dialog,WM_COMMAND,IDOK,0);
    Sleep(100);
    for(page=0;page<3;++page) {
        change_page(tab,page);Sleep(1100);check_page(page);
    }
    SendMessageA(target,WM_COMMAND,ID_PAUSE,0);
    for(i=0;i<6;++i) {
        page=i%3;change_page(tab,page);
        MoveWindow(target,8,8,i%2?400:780,i%2?450:580,TRUE);
        SendMessageA(target,WM_COMMAND,ID_REFRESH,0);
        check_page(page);
    }
    applications_workflow(tab,process.hProcess);
    processes_workflow(tab,process.hProcess);
    SendMessageA(target,WM_COMMAND,ID_RUSSIAN,0);
    GetWindowTextW(target,title,100);
    verify(title[0]==0x414 && title[1]==0x438,"Russian Unicode title");
    MoveWindow(target,8,8,400,450,TRUE);
    for(page=0;page<3;++page) { change_page(tab,page);check_page(page); }
    verify(performance!=NULL,"performance control found");
    verify((GetWindowLongA(performance,GWL_STYLE)&WS_HSCROLL)==0,"reference minimum needs no horizontal scrolling");
    verify((GetWindowLongA(performance,GWL_STYLE)&WS_VSCROLL)==0,"reference minimum needs no vertical scrolling");
    GetClientRect(performance,&bounds);
    verify(bounds.right>=320 && bounds.bottom>=300,"all compact panels fit the viewport");
    verify(black_samples(performance)>50,"compact graphs are actually painted");
    graph_height=history_plot_height(performance);
    printf("Minimum history plot height: %d pixels\n",graph_height);
    verify(graph_height>=52,"both history plots retain useful height at 400x450");
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
    change_page(tab,2);MoveWindow(target,8,8,400,450,TRUE);
    GetExitCodeProcess(process.hProcess,&exit_code);
    verify(exit_code==STILL_ACTIVE,"application survives UI stress");
    PostMessageA(target,WM_CLOSE,0,0);
    verify(WaitForSingleObject(process.hProcess,5000)==WAIT_OBJECT_0,"first process closes cleanly");
    CloseHandle(process.hProcess);CloseHandle(process.hThread);
    target=NULL;
    if(!CreateProcessA(NULL,command,NULL,NULL,FALSE,0,NULL,NULL,&start,&process)) return 2;
    WaitForInputIdle(process.hProcess,5000);
    expected_pid=process.dwProcessId;
    for(i=0;i<200 && !target;++i) { EnumWindows(find_target,0);Sleep(30); }
    verify(target!=NULL,"application restarts with saved preferences");
    if(target) {
        HWND page_window=GetWindow(target,GW_CHILD),restored=NULL;
        while(page_window) {
            char name[64];GetClassNameA(page_window,name,sizeof(name));
            if(!strcmp(name,"Taskmgr95Page") && IsWindowVisible(page_window)) { restored=GetDlgItem(page_window,302);break; }
            page_window=GetWindow(page_window,GW_HWNDNEXT);
        }
        verify(restored && ((DWORD)GetWindowLongA(restored,GWL_STYLE)&LVS_TYPEMASK)==LVS_SMALLICON,
            "view mode persists after restart");
        SendMessageA(target,WM_COMMAND,ID_VIEW_DETAILS,0);
        verify(restored && Header_GetItemCount((HWND)SendMessageA(restored,LVM_GETHEADER,0,0))==5 &&
            ListView_GetColumnWidth(restored,0)==190 && ListView_GetColumnWidth(restored,1)==77,
            "column choices and widths persist after restart");
        tab=GetDlgItem(target,301);
        {
            HWND list=process_list(tab);
            verify(list && Header_GetItemCount((HWND)SendMessageA(list,LVM_GETHEADER,0,0))==6 &&
                ListView_GetColumnWidth(list,0)==171 && ListView_GetColumnWidth(list,2)==133 &&
                ListView_GetColumnWidth(list,5)==99 &&
                (GetMenuState(GetSubMenu(GetMenu(target),2),ID_PROC_SORT_PID,MF_BYCOMMAND)&MF_CHECKED),
                "process preferences persist independently after restart");
            change_page(tab,0);
        }
        SendMessageA(target,WM_COMMAND,ID_SORT_TASK,0);
        /* Show a compact, useful pair of columns in the captured example. */
        PostMessageA(target,WM_COMMAND,ID_VIEW_COLUMNS,0);
        dialog=wait_dialog();
        if(dialog) {
            int col;
            for(col=1;col<5;++col) {
                select_column(dialog,col);
                CheckDlgButton(dialog,IDC_COLUMN_VISIBLE,col==2?BST_CHECKED:BST_UNCHECKED);
                SendMessageA(dialog,WM_COMMAND,MAKEWPARAM(IDC_COLUMN_VISIBLE,BN_CLICKED),0);
            }
            CheckDlgButton(dialog,IDC_COLUMN_AUTOFIT,BST_CHECKED);
            SendMessageA(dialog,WM_COMMAND,IDC_COLUMN_AUTOFIT,0);
            SendMessageA(dialog,WM_COMMAND,IDOK,0);
        }
    }
    if(target) {
        tab=GetDlgItem(target,301);change_page(tab,1);
        PostMessageA(target,WM_COMMAND,ID_VIEW_COLUMNS,0);
        dialog=wait_dialog();
        if(dialog) {
            for(i=3;i<7;++i) {
                select_column(dialog,i);CheckDlgButton(dialog,IDC_COLUMN_VISIBLE,i==5?BST_CHECKED:BST_UNCHECKED);
                SendMessageA(dialog,WM_COMMAND,IDC_COLUMN_VISIBLE,0);
            }
            SendMessageA(dialog,WM_COMMAND,IDOK,0);
        }
    }
    printf("%d checks, %d failures\n",checks,failures);
    fflush(stdout);
    if(GetEnvironmentVariableA("TM95_CAPTURE_HOLD",hold,sizeof(hold))) Sleep(10000);
    if(fixture_window) PostMessageA(fixture_window,WM_CLOSE,0,0);
    if(fixture_thread_handle) { WaitForSingleObject(fixture_thread_handle,3000);CloseHandle(fixture_thread_handle); }
    PostMessageA(target,WM_CLOSE,0,0);
    WaitForSingleObject(process.hProcess,5000);
    CloseHandle(process.hProcess);CloseHandle(process.hThread);
    return failures?1:0;
}
