#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#include "system.h"
#include "performance.h"
#include "applications.h"
#include "processes.h"
#include "cad.h"
#include "i18n.h"
#include <shellapi.h>

#define APP_CLASS "Taskmgr95Main"
#define PAGE_CLASS "Taskmgr95Page"
#define ID_TABS 301
#define ID_APPS 302
#define ID_PROCS 303
#define ID_END 304
#define ID_SWITCH 305

static HINSTANCE instance;
static HWND main_window, tabs, apps, procs, status_bar, end_button, switch_button;
static HWND new_button, graph, pages[3], process_end_button;
static HFONT ui_font;
static TM_SYSTEM system_info;
static BOOL creating = TRUE;
static BOOL refreshing;
static LRESULT (WINAPI *main_default)(HWND,UINT,WPARAM,LPARAM)=DefWindowProcA;
static HMENU create_menu(void);
static void layout(void);
static void localize(void);
static int page;
static BOOL paused, on_top;

static void error_message(HWND owner, const char *operation)
{
    language_error(owner,operation,GetLastError());
}
static HWND control(HWND parent, const char *cls, const char *text, DWORD style, int id)
{
    HWND w = CreateWindowExA(0, cls, text, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | style,
        0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, instance, NULL);
    if (w) SendMessageA(w, WM_SETFONT, (WPARAM)ui_font, TRUE);
    return w;
}
static BOOL selected(HWND list, LPARAM *key)
{
    LVITEMA item;
    int index = (int)SendMessageA(list, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    if (index < 0) return FALSE;
    ZeroMemory(&item, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = index;
    if (!SendMessageA(list, LVM_GETITEMA, 0, (LPARAM)&item)) return FALSE;
    *key = item.lParam;
    return TRUE;
}
static void restore_selection(HWND list, LPARAM key, BOOL valid)
{
    LVFINDINFOA find;
    LVITEMA item;
    int index;
    if (!valid) return;
    ZeroMemory(&find, sizeof(find));
    find.flags = LVFI_PARAM;
    find.lParam = key;
    index = (int)SendMessageA(list, LVM_FINDITEMA, (WPARAM)-1, (LPARAM)&find);
    if (index < 0) return;
    ZeroMemory(&item, sizeof(item));
    item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    item.state = item.stateMask;
    SendMessageA(list, LVM_SETITEMSTATE, index, (LPARAM)&item);
}
static void update_actions(void)
{
    LPARAM key;
    BOOL has, can_end=FALSE;
    DWORD pid=0;
    if (creating || refreshing) return;
    has = page < 2 && selected(page == 0 ? apps : procs, &key);
    if(has) {
        if(page==0) GetWindowThreadProcessId((HWND)key,&pid);
        else pid=(DWORD)key;
        can_end=tm_process_end_allowed(&system_info,pid);
    }
    EnableWindow(end_button, has && can_end && page == 0);
    EnableWindow(process_end_button, has && can_end && page == 1);
    EnableWindow(switch_button, has && page == 0);
}
static void update_status(void)
{
    RECT r;
    BOOL compact;
    GetClientRect(main_window,&r);compact=r.right<480;
    language_status(status_bar,0,system_info.processes_valid?T_STATPROC:T_STATSTALE,system_info.processes,system_info.processes_valid);
    language_status(status_bar,1,system_info.cpu_valid?(compact?T_STATCPU_SHORT:T_STATCPU):(compact?T_STATCPUNA_SHORT:T_STATCPUNA),system_info.cpu_percent,system_info.cpu_valid);
    language_status(status_bar,2,compact?T_STATMEM_SHORT:T_STATMEM,system_info.memory.dwMemoryLoad,TRUE);
}
static void refresh(void)
{
    LPARAM app_key = 0, process_key = 0;
    BOOL had_app, had_process;
    BOOL draw_apps = IsWindowVisible(apps), draw_procs = IsWindowVisible(procs);
    int app_top = ListView_GetTopIndex(apps), proc_top = ListView_GetTopIndex(procs);
    refreshing=TRUE;
    had_app = selected(apps, &app_key);
    had_process = selected(procs, &process_key);
    if (draw_apps) SendMessageA(apps, WM_SETREDRAW, FALSE, 0);
    if (draw_procs) SendMessageA(procs, WM_SETREDRAW, FALSE, 0);
    applications_refresh(&system_info);
    processes_refresh(&system_info);
    restore_selection(apps, app_key, had_app);
    restore_selection(procs, process_key, had_process);
    if (draw_apps) SendMessageA(apps, WM_SETREDRAW, TRUE, 0);
    if (draw_procs) SendMessageA(procs, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(apps, NULL, TRUE); InvalidateRect(procs, NULL, TRUE);
    if (app_top > 0) ListView_EnsureVisible(apps, app_top, FALSE);
    if (proc_top > 0) ListView_EnsureVisible(procs, proc_top, FALSE);
    tm_memory_sample(&system_info);
    performance_update(graph, &system_info, TRUE);
    update_status();
    refreshing=FALSE;
    update_actions();
}
static LRESULT CALLBACK page_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_COMMAND || msg == WM_NOTIFY)
        return SendMessageA(main_window, msg, wp, lp);
    return DefWindowProcA(w, msg, wp, lp);
}
static void layout(void)
{
    RECT r, content, sr;
    int width, height, parts[3], i, cw, ch, button_width, process_width, gap=6;
    if (creating || !tabs) return;
    GetClientRect(main_window, &r);
    width=r.right; height=r.bottom;
    if (width<1 || height<1) return;
    SendMessageA(status_bar, WM_SIZE, 0, 0);
    GetWindowRect(status_bar, &sr);
    parts[0]=width/3;parts[1]=width*2/3;parts[2]=-1;
    SendMessageA(status_bar, SB_SETPARTS, 3, (LPARAM)parts);
    MoveWindow(tabs,8,8,width-16,height-(sr.bottom-sr.top)-16,TRUE);
    GetClientRect(tabs,&content);
    TabCtrl_AdjustRect(tabs,FALSE,&content);
    MapWindowPoints(tabs,main_window,(POINT *)&content,2);
    InflateRect(&content,-6,-6);
    cw=content.right-content.left;ch=content.bottom-content.top;
    for(i=0;i<3;++i) {
        ShowWindow(pages[i], i==page ? SW_SHOW : SW_HIDE);
        MoveWindow(pages[i],content.left,content.top,cw,ch,TRUE);
    }
    SetWindowPos(pages[page],HWND_TOP,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    process_width=language_russian()?150:116;
    button_width=process_width;
    if(button_width>(cw-2*gap)/3) button_width=(cw-2*gap)/3;
    MoveWindow(apps,0,0,cw,ch-34,TRUE);
    applications_resize(cw);
    MoveWindow(procs,0,0,cw,ch-34,TRUE);
    processes_resize(cw);
    MoveWindow(graph,0,0,cw,ch,TRUE);
    MoveWindow(end_button,cw-3*button_width-2*gap,ch-25,button_width,24,TRUE);
    MoveWindow(switch_button,cw-2*button_width-gap,ch-25,button_width,24,TRUE);
    MoveWindow(new_button,cw-button_width,ch-25,button_width,24,TRUE);
    MoveWindow(process_end_button,cw-process_width,ch-25,process_width,24,TRUE);
    InvalidateRect(main_window,NULL,TRUE);
    update_status();
    update_actions();
    if(page==1) processes_menu_state();
    else applications_menu_state(page==0);
}
static INT_PTR CALLBACK run_dialog(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;
    if (msg == WM_INITDIALOG) {
        language_text(w,T_NEW);
        language_text(GetDlgItem(w,IDC_RUNPROMPT),T_RUNPROMPT);
        language_text(GetDlgItem(w,IDCANCEL),T_CANCEL);
        SendDlgItemMessageA(w, IDC_COMMAND, EM_SETLIMITTEXT, 1023, 0); return TRUE;
    }
    if (msg == WM_COMMAND) {
        if (LOWORD(wp) == IDCANCEL) { EndDialog(w, 0); return TRUE; }
        if (LOWORD(wp) == IDOK) {
            char command[1024];
            STARTUPINFOA start;
            PROCESS_INFORMATION process;
            GetDlgItemTextA(w, IDC_COMMAND, command, sizeof(command));
            if (!command[0]) return TRUE;
            ZeroMemory(&start, sizeof(start)); start.cb = sizeof(start);
            ZeroMemory(&process, sizeof(process));
            if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &start, &process)) {
                error_message(w, "CreateProcess"); return TRUE;
            }
            CloseHandle(process.hThread); CloseHandle(process.hProcess);
            EndDialog(w, 1); return TRUE;
        }
    }
    return FALSE;
}
static void end_selected(void)
{
    LPARAM key;
    HANDLE process;
    DWORD pid;
    if (page == 0 && selected(apps, &key)) {
        if (!IsWindow((HWND)key)) return;
        GetWindowThreadProcessId((HWND)key,&pid);
        if(!tm_process_end_allowed(&system_info,pid)) {
            language_message(main_window,T_PROTECTED,T_TITLE,MB_OK);return;
        }
        /* Closing is asynchronous: a hung application cannot block our UI. */
        if (!PostMessageA((HWND)key, WM_CLOSE, 0, 0)) error_message(main_window, "Close window");
    } else if (page == 1 && selected(procs, &key)) {
        pid = (DWORD)key;
        if (!pid || pid == GetCurrentProcessId()) {
            language_message(main_window,T_PROTECTED,T_TITLE,MB_OK); return;
        }
        process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (!process) { error_message(main_window, "OpenProcess"); return; }
        if(!tm_process_end_allowed(&system_info,pid)) {
            CloseHandle(process);
            language_message(main_window,T_PROTECTED,T_TITLE,MB_OK);return;
        }
        if (language_message(main_window,T_CONFIRMEND,
            T_ENDPROCESS, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES) {
            if(!tm_process_end_allowed(&system_info,pid))
                language_message(main_window,T_PROTECTED,T_TITLE,MB_OK);
            else if (!TerminateProcess(process, 1)) error_message(main_window, "TerminateProcess");
        }
        CloseHandle(process);
    }
}
static void priority_selected(DWORD priority)
{
    LPARAM key;
    HANDLE process;
    if (page != 1 || !selected(procs, &key)) return;
    process = OpenProcess(PROCESS_SET_INFORMATION, FALSE, (DWORD)key);
    if (!process) { error_message(main_window, "OpenProcess"); return; }
    if (priority != HIGH_PRIORITY_CLASS || language_message(main_window,
        T_CONFIRMHIGH,T_PRIORITY,
        MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) == IDYES)
        if (!SetPriorityClass(process, priority)) error_message(main_window, "SetPriorityClass");
    CloseHandle(process);
}
static void new_task(HWND w)
{
    typedef void (WINAPI *RUN_DIALOG)(HWND,HICON,LPCVOID,LPCVOID,LPCVOID,UINT);
    HMODULE shell=LoadLibraryA("SHELL32.DLL");
    RUN_DIALOG run=NULL;
    /* Ordinal 61 is the historical RunFileDlg, NOT a stable public API.
       On NT it expects UTF-16; do not pass ANSI there. A custom fallback
       remains available when the export is missing. */
    if(shell)
        run=(RUN_DIALOG)GetProcAddress(shell,(LPCSTR)61);
    if(run) run(w,NULL,NULL,language_unicode()?(LPCVOID)trw(T_NEW):(LPCVOID)tr(T_NEW),
        language_unicode()?(LPCVOID)trw(T_RUNPROMPT):(LPCVOID)tr(T_RUNPROMPT),0);
    else DialogBoxParamA(instance,MAKEINTRESOURCEA(IDD_RUN),w,run_dialog,0);
    if(shell) FreeLibrary(shell);
}
static HMENU create_menu(void)
{
    HMENU bar=CreateMenu(),file=CreatePopupMenu(),options=CreatePopupMenu();
    HMENU priority=CreatePopupMenu(),help=CreatePopupMenu(),language=CreatePopupMenu(),view=CreatePopupMenu();
    if(page==1) processes_menu(view);
    else applications_menu(view);
    language_menu(file,MF_STRING,ID_NEW,T_NEW);
    AppendMenuA(file,MF_SEPARATOR,0,NULL);
    language_menu(file,MF_STRING,ID_EXIT,T_EXIT);
    language_menu(options,MF_STRING | (on_top?MF_CHECKED:0),ID_TOP,T_TOP);
    language_menu(options,MF_STRING | (paused?MF_CHECKED:0),ID_PAUSE,T_PAUSE);
    language_menu(options,MF_STRING,ID_REFRESH,T_REFRESH);
    language_menu(options,MF_STRING | (system_info.vxd_valid?MF_CHECKED:0),ID_DRIVER,T_DRIVER);
    language_menu(options,MF_STRING | (cad_enabled()?MF_CHECKED:0),ID_CAD,T_CAD);
    language_menu(priority,MF_STRING,ID_IDLE,T_IDLE);
    language_menu(priority,MF_STRING,ID_NORMAL,T_NORMAL);
    language_menu(priority,MF_STRING,ID_HIGH,T_HIGH);
    language_menu(options,MF_POPUP,(UINT_PTR)priority,T_PRIORITY);
    AppendMenuA(language,MF_STRING | (!language_russian()?MF_CHECKED:0),ID_ENGLISH,"English");
    AppendMenuA(language,MF_STRING | (language_russian()?MF_CHECKED:0),ID_RUSSIAN,"Russian");
    language_menu(options,MF_POPUP,(UINT_PTR)language,T_LANGUAGE);
    language_menu(help,MF_STRING,ID_ABOUT,T_ABOUT);
    language_menu(bar,MF_POPUP,(UINT_PTR)file,T_FILE);
    language_menu(bar,MF_POPUP,(UINT_PTR)options,T_OPTIONS);
    language_menu(bar,MF_POPUP,(UINT_PTR)view,T_VIEW);
    language_menu(bar,MF_POPUP,(UINT_PTR)help,T_HELP);
    return bar;
}
static void localize(void)
{
    TCITEMA tab;
    HMENU old=GetMenu(main_window);
    int i;
    SetMenu(main_window,create_menu());
    if(old) DestroyMenu(old);
    DrawMenuBar(main_window);
    language_text(main_window,T_TITLE);
    language_text(end_button,T_ENDTASK);
    language_text(process_end_button,T_ENDPROCESS);
    language_text(switch_button,T_SWITCH);
    language_text(new_button,T_NEW);
    ZeroMemory(&tab,sizeof(tab));tab.mask=TCIF_TEXT;
    for(i=0;i<3;++i) {
        tab.pszText=(LPSTR)(language_unicode()?(const void *)trw(T_APPS+i):(const void *)tr(T_APPS+i));
        SendMessageA(tabs,language_unicode()?TCM_SETITEMW:TCM_SETITEMA,i,(LPARAM)&tab);
    }
    applications_localize();
    processes_localize();
    performance_update(graph,&system_info,FALSE);
    layout();
}
static LRESULT CALLBACK window_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    LPARAM key;
    switch (msg) {
    case WM_CREATE: {
        TCITEMA tab;
        int i;
        main_window=w;
        ui_font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
        tabs=control(w,WC_TABCONTROLA,"",WS_TABSTOP,ID_TABS);
        status_bar=control(w,STATUSCLASSNAMEA,"",SBARS_SIZEGRIP,0);
        for(i=0;i<3;++i)
            pages[i]=CreateWindowExA(WS_EX_CONTROLPARENT,PAGE_CLASS,"",
                WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                0,0,0,0,w,NULL,instance,NULL);
        apps=control(pages[0],WC_LISTVIEWA,"",LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_AUTOARRANGE | WS_BORDER | WS_TABSTOP,ID_APPS);
        procs=control(pages[1],WC_LISTVIEWA,"",LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP,ID_PROCS);
        end_button=control(pages[0],"BUTTON","",WS_TABSTOP,ID_END);
        switch_button=control(pages[0],"BUTTON","",WS_TABSTOP,ID_SWITCH);
        new_button=control(pages[0],"BUTTON","",WS_TABSTOP,ID_NEW);
        process_end_button=control(pages[1],"BUTTON","",WS_TABSTOP,ID_END);
        graph=control(pages[2],TM95_PERF_CLASS,"",WS_HSCROLL | WS_VSCROLL,0);
        if(!tabs || !apps || !procs || !status_bar || !end_button || !switch_button || !new_button || !process_end_button || !graph || !pages[0] || !pages[1] || !pages[2]) return -1;
        ListView_SetBkColor(apps,GetSysColor(COLOR_WINDOW));
        ListView_SetTextBkColor(apps,GetSysColor(COLOR_WINDOW));
        ListView_SetBkColor(procs,GetSysColor(COLOR_WINDOW));
        ListView_SetTextBkColor(procs,GetSysColor(COLOR_WINDOW));
        ZeroMemory(&tab,sizeof(tab));tab.mask=TCIF_TEXT;tab.pszText="";
        for(i=0;i<3;++i) SendMessageA(tabs,TCM_INSERTITEMA,i,(LPARAM)&tab);
        if(!applications_init(apps,w,instance,".\\taskmgr95.ini")) return -1;
        if(!processes_init(procs,w,instance,".\\taskmgr95.ini")) return -1;
        tm_system_init(&system_info);
        creating=FALSE;
        localize();refresh();
        if(!SetTimer(w,1,1000,NULL)) return -1;
        return 0;
    }
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lp)->ptMinTrackSize.x = 400;
        ((MINMAXINFO *)lp)->ptMinTrackSize.y = 450;
        return 0;
    case WM_SIZE: layout(); return 0;
    case WM_NOTIFY:
        applications_notify((NMHDR *)lp);
        processes_notify((NMHDR *)lp);
        if (((NMHDR *)lp)->idFrom == ID_TABS && ((NMHDR *)lp)->code == TCN_SELCHANGE) {
            page = TabCtrl_GetCurSel(tabs); localize();
        }
        if (((NMHDR *)lp)->code == LVN_ITEMCHANGED) update_actions();
        return 0;
    case WM_CLOSE:
        if(cad_enabled()) ShowWindow(w,SW_HIDE);else DestroyWindow(w);
        return 0;
    case WM_TIMER:
        if(cad_poll()) { ShowWindow(w,SW_RESTORE);SetForegroundWindow(w); }
        if(!paused) refresh();
        CheckMenuItem(GetMenu(w),ID_CAD,MF_BYCOMMAND | (cad_enabled()?MF_CHECKED:MF_UNCHECKED));
        return 0;
    case WM_COMMAND:
        if(page==0 && applications_command(LOWORD(wp))) { applications_refresh(&system_info);update_actions();return 0; }
        if(page==1 && processes_command(LOWORD(wp))) { refresh();return 0; }
        switch (LOWORD(wp)) {
        case ID_NEW: new_task(w); refresh(); break;
        case ID_EXIT: DestroyWindow(w); break;
        case ID_REFRESH: refresh(); break;
        case ID_END: end_selected(); break;
        case ID_SWITCH:
            if (page == 0 && selected(apps, &key) && IsWindow((HWND)key)) {
                if (IsIconic((HWND)key)) ShowWindowAsync((HWND)key, SW_RESTORE);
                SetForegroundWindow((HWND)key);
            } break;
        case ID_TOP:
            on_top = !on_top;
            SetWindowPos(w, on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
            CheckMenuItem(GetMenu(w), ID_TOP, MF_BYCOMMAND | (on_top ? MF_CHECKED : MF_UNCHECKED)); break;
        case ID_PAUSE:
            paused = !paused;
            CheckMenuItem(GetMenu(w), ID_PAUSE, MF_BYCOMMAND | (paused ? MF_CHECKED : MF_UNCHECKED)); break;
        case ID_IDLE: priority_selected(IDLE_PRIORITY_CLASS); break;
        case ID_NORMAL: priority_selected(NORMAL_PRIORITY_CLASS); break;
        case ID_HIGH: priority_selected(HIGH_PRIORITY_CLASS); break;
        case ID_CAD:
            if(cad_enabled()) {
                if(!cad_disable(TRUE)) error_message(w,"Ctrl+Alt+Del");
            } else if(!cad_enable(instance,TRUE)) error_message(w,driver_error()?driver_stage():"Ctrl+Alt+Del");
            refresh();localize();break;
        case ID_DRIVER:
            if(system_info.vxd_valid) {
                if(cad_enabled() && !cad_disable(TRUE)) { error_message(w,"Ctrl+Alt+Del");break; }
                driver_close();
            }
            else if(!driver_enable(instance)) {
                SetLastError(driver_error());error_message(w,driver_stage());
            }
            refresh();localize();break;
        case ID_ENGLISH:
        case ID_RUSSIAN:
            language_init(LOWORD(wp)==ID_RUSSIAN);
            localize(); refresh();
            WritePrivateProfileStringA("Interface","Language",language_russian()?"ru":"en",".\\taskmgr95.ini");
            break;
        case ID_ABOUT: {
            int (WINAPI *about_w)(HWND,LPCWSTR,LPCWSTR,HICON)=NULL;
            if(language_unicode()) about_w=(int (WINAPI *)(HWND,LPCWSTR,LPCWSTR,HICON))
                GetProcAddress(GetModuleHandleA("SHELL32.DLL"),"ShellAboutW");
            if(about_w) about_w(w,trw(T_TITLE),trw(T_ABOUTTEXT),LoadIconA(NULL,IDI_APPLICATION));
            else ShellAboutA(w,tr(T_TITLE),tr(T_ABOUTTEXT),LoadIconA(NULL,IDI_APPLICATION));
            break;
        }
        }
        return 0;
    case WM_DESTROY: cad_close();applications_close();processes_close();KillTimer(w, 1); tm_system_close(&system_info); PostQuitMessage(0); return 0;
    }
    return main_default(w, msg, wp, lp);
}
void WINAPI WinMainCRTStartup(void)
{
    WNDCLASSA wc;
    MSG msg;
    HWND w;
    BOOL result;
    instance = GetModuleHandleA(NULL);
    {
        HWND existing=FindWindowA(APP_CLASS,NULL);
        if(existing) { ShowWindow(existing,SW_RESTORE);SetForegroundWindow(existing);ExitProcess(0); }
    }
    {
        char language[8];
        GetPrivateProfileStringA("Interface","Language",GetACP()==1251?"ru":"en",language,sizeof(language),".\\taskmgr95.ini");
        language_init(language[0]=='r');
    }
    InitCommonControls();
    ZeroMemory(&wc, sizeof(wc));
    wc.hInstance = instance; wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpfnWndProc = window_proc; wc.lpszClassName = APP_CLASS;
    if(language_unicode()) {
        WNDCLASSW wide;
        ATOM (WINAPI *register_w)(const WNDCLASSW *);
        memcpy(&wide,&wc,sizeof(wide));
        wide.lpszClassName=L"Taskmgr95Main";
        register_w=(ATOM (WINAPI *)(const WNDCLASSW *))GetProcAddress(GetModuleHandleA("USER32.DLL"),"RegisterClassW");
        main_default=(LRESULT (WINAPI *)(HWND,UINT,WPARAM,LPARAM))GetProcAddress(GetModuleHandleA("USER32.DLL"),"DefWindowProcW");
        if(!register_w || !main_default || !register_w(&wide)) ExitProcess(1);
    } else if (!RegisterClassA(&wc)) ExitProcess(1);
    wc.lpfnWndProc = page_proc; wc.lpszClassName = PAGE_CLASS;
    if (!RegisterClassA(&wc)) ExitProcess(1);
    if (!performance_register(instance)) ExitProcess(1);
    if(language_unicode()) {
        HWND (WINAPI *create_w)(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID);
        create_w=(HWND (WINAPI *)(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID))
            GetProcAddress(GetModuleHandleA("USER32.DLL"),"CreateWindowExW");
        if(!create_w) ExitProcess(1);
        w=create_w(WS_EX_CONTROLPARENT,L"Taskmgr95Main",trw(T_TITLE),
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,
            400,450,NULL,create_menu(),instance,NULL);
    } else w = CreateWindowExA(WS_EX_CONTROLPARENT, APP_CLASS, "Task Manager 95",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
        400, 450, NULL, create_menu(), instance, NULL);
    if (!w) ExitProcess(1);
    if(cad_requested() || cad_resident_argument()) {
        if(!cad_enable(instance,FALSE)) error_message(w,driver_stage());
        localize();
    }
    ShowWindow(w,cad_resident_argument() && cad_enabled()?SW_HIDE:SW_SHOWDEFAULT);UpdateWindow(w);
    while ((result = GetMessageA(&msg, NULL, 0, 0)) > 0) {
        if (!IsDialogMessageA(w, &msg)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    }
    ExitProcess(result == -1 ? 1 : (UINT)msg.wParam);
}
