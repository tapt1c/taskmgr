#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#include "system.h"

#define APP_CLASS "Taskmgr95Main"
#define GRAPH_CLASS "Taskmgr95Graph"
#define ID_TABS 301
#define ID_APPS 302
#define ID_PROCS 303
#define ID_END 304
#define ID_SWITCH 305
#define HISTORY 120
static HINSTANCE instance;
static HWND main_window, tabs, apps, procs, status_bar, end_button, switch_button;
static HWND new_button, graph, memory_text;
static HFONT ui_font;
static TM_SYSTEM system_info;
static DWORD history[HISTORY];
static unsigned int samples;
static int page;
static BOOL paused, on_top;

static void error_message(HWND owner, const char *operation)
{
    char text[256];
    wsprintfA(text, "%s failed (Windows error %lu).", operation, GetLastError());
    MessageBoxA(owner, text, "Task Manager 95", MB_OK | MB_ICONEXCLAMATION);
}
static HWND control(const char *cls, const char *text, DWORD style, int id)
{
    HWND w = CreateWindowExA(0, cls, text, WS_CHILD | WS_VISIBLE | style,
        0, 0, 0, 0, main_window, (HMENU)(INT_PTR)id, instance, NULL);
    if (w) SendMessageA(w, WM_SETFONT, (WPARAM)ui_font, TRUE);
    return w;
}
static void column(HWND list, int index, char *name, int width)
{
    LVCOLUMNA c;
    ZeroMemory(&c, sizeof(c));
    c.mask = LVCF_TEXT | LVCF_WIDTH;
    c.pszText = name;
    c.cx = width;
    SendMessageA(list, LVM_INSERTCOLUMNA, index, (LPARAM)&c);
}
static int row(HWND list, const char *name, LPARAM key)
{
    LVITEMA item;
    ZeroMemory(&item, sizeof(item));
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = (int)SendMessageA(list, LVM_GETITEMCOUNT, 0, 0);
    item.pszText = (LPSTR)name;
    item.lParam = key;
    return (int)SendMessageA(list, LVM_INSERTITEMA, 0, (LPARAM)&item);
}
static void cell(HWND list, int index, int sub, const char *text)
{
    LVITEMA item;
    ZeroMemory(&item, sizeof(item));
    item.iSubItem = sub;
    item.pszText = (LPSTR)text;
    SendMessageA(list, LVM_SETITEMTEXTA, index, (LPARAM)&item);
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
    BOOL has = page < 2 && selected(page == 0 ? apps : procs, &key);
    EnableWindow(end_button, has);
    EnableWindow(switch_button, has && page == 0);
}
static BOOL CALLBACK enumerate_window(HWND w, LPARAM unused)
{
    char title[256], pid_text[32];
    DWORD pid;
    int index;
    (void)unused;
    if (!IsWindowVisible(w) || GetWindow(w, GW_OWNER) || w == main_window) return TRUE;
    if (!GetWindowTextA(w, title, sizeof(title))) return TRUE;
    GetWindowThreadProcessId(w, &pid);
    index = row(apps, title, (LPARAM)w);
    wsprintfA(pid_text, "%lu", pid);
    cell(apps, index, 1, pid_text);
    return TRUE;
}
static void refresh(void)
{
    HANDLE snapshot;
    PROCESSENTRY32 entry;
    LPARAM app_key = 0, process_key = 0;
    BOOL had_app, had_process;
    char text[768], value[32];
    int index;
    unsigned int i;
    had_app = selected(apps, &app_key);
    had_process = selected(procs, &process_key);
    SendMessageA(apps, WM_SETREDRAW, FALSE, 0);
    SendMessageA(procs, WM_SETREDRAW, FALSE, 0);
    SendMessageA(apps, LVM_DELETEALLITEMS, 0, 0);
    EnumWindows(enumerate_window, 0);
    snapshot = tm_process_snapshot(&system_info);
    system_info.processes_valid = FALSE;
    /* Keep old rows on snapshot failure rather than presenting an empty system. */
    if (snapshot != INVALID_HANDLE_VALUE) {
        ZeroMemory(&entry, sizeof(entry));
        entry.dwSize = sizeof(entry);
        if (system_info.first(snapshot, &entry)) {
            SendMessageA(procs, LVM_DELETEALLITEMS, 0, 0);
            system_info.processes = system_info.threads = 0;
            do {
                const char *base = entry.szExeFile;
                const char *p;
                for (p = entry.szExeFile; *p; ++p) if (*p == '\\' || *p == '/') base = p + 1;
                index = row(procs, base, (LPARAM)entry.th32ProcessID);
                wsprintfA(value, "%lu", entry.th32ProcessID); cell(procs, index, 1, value);
                wsprintfA(value, "%lu", entry.cntThreads); cell(procs, index, 2, value);
                wsprintfA(value, "%ld", entry.pcPriClassBase); cell(procs, index, 3, value);
                cell(procs, index, 4, entry.szExeFile);
                ++system_info.processes;
                system_info.threads += entry.cntThreads;
                entry.dwSize = sizeof(entry);
            } while (system_info.next(snapshot, &entry));
            system_info.processes_valid = GetLastError() == ERROR_NO_MORE_FILES;
        }
        CloseHandle(snapshot);
    }
    restore_selection(apps, app_key, had_app);
    restore_selection(procs, process_key, had_process);
    SendMessageA(apps, WM_SETREDRAW, TRUE, 0);
    SendMessageA(procs, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(apps, NULL, TRUE); InvalidateRect(procs, NULL, TRUE);
    tm_memory_sample(&system_info);
    if (samples == HISTORY) {
        for (i = 1; i < HISTORY; ++i) history[i-1] = history[i];
        --samples;
    }
    history[samples++] = system_info.memory.dwMemoryLoad;
    wsprintfA(text,
        "Physical memory (KB)\r\nTotal: %lu    Available: %lu\r\nLoad: %lu%%\r\n\r\n"
        "Page-file capacity (KB)\r\nTotal: %lu    Available: %lu\r\n\r\n"
        "CPU: unavailable (VxD not implemented)\r\nThreads: %lu%s",
        system_info.memory.dwTotalPhys / 1024, system_info.memory.dwAvailPhys / 1024,
        system_info.memory.dwMemoryLoad, system_info.memory.dwTotalPageFile / 1024,
        system_info.memory.dwAvailPageFile / 1024, system_info.threads,
        system_info.processes_valid ? "" : " (process data unavailable or stale)");
    SetWindowTextA(memory_text, text);
    if (system_info.processes_valid) wsprintfA(text, "Processes: %lu", system_info.processes);
    else lstrcpyA(text, "Processes: unavailable / stale");
    SendMessageA(status_bar, SB_SETTEXTA, 0, (LPARAM)text);
    SendMessageA(status_bar, SB_SETTEXTA, 1, (LPARAM)"CPU: N/A");
    wsprintfA(text, "Physical memory: %lu%%", system_info.memory.dwMemoryLoad);
    SendMessageA(status_bar, SB_SETTEXTA, 2, (LPARAM)text);
    InvalidateRect(graph, NULL, FALSE);
    update_actions();
}
static LRESULT CALLBACK graph_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        RECT r;
        HPEN grid_pen, line_pen;
        HGDIOBJ old_pen;
        HDC dc = BeginPaint(w, &ps);
        unsigned int i;
        int x, y, width, height;
        GetClientRect(w, &r);
        FillRect(dc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
        width = r.right > 1 ? r.right - 1 : 1;
        height = r.bottom > 1 ? r.bottom - 1 : 1;
        grid_pen = CreatePen(PS_SOLID, 1, RGB(0, 80, 40));
        line_pen = CreatePen(PS_SOLID, 1, RGB(0, 255, 0));
        old_pen = SelectObject(dc, grid_pen);
        for (x = 0; x <= width; x += 20) { MoveToEx(dc, x, 0, NULL); LineTo(dc, x, height); }
        for (y = 0; y <= height; y += 20) { MoveToEx(dc, 0, y, NULL); LineTo(dc, width, y); }
        SelectObject(dc, line_pen);
        for (i = 0; i < samples; ++i) {
            x = (int)(HISTORY - samples + i) * width / (HISTORY - 1);
            y = height - (int)history[i] * height / 100;
            if (i == 0) MoveToEx(dc, x, y, NULL); else LineTo(dc, x, y);
        }
        SelectObject(dc, old_pen); DeleteObject(grid_pen); DeleteObject(line_pen);
        SetTextColor(dc, RGB(0,255,0)); SetBkMode(dc, TRANSPARENT);
        TextOutA(dc, 6, 6, "Physical memory load (0-100%)", 28);
        EndPaint(w, &ps);
        return 0;
    }
    return DefWindowProcA(w, msg, wp, lp);
}
static void layout(void)
{
    RECT r, content;
    int width, height, parts[3];
    GetClientRect(main_window, &r);
    width = r.right; height = r.bottom;
    SendMessageA(status_bar, WM_SIZE, 0, 0);
    parts[0] = width / 3; parts[1] = width * 2 / 3; parts[2] = -1;
    SendMessageA(status_bar, SB_SETPARTS, 3, (LPARAM)parts);
    MoveWindow(tabs, 8, 8, width - 16, height - 40, TRUE);
    GetClientRect(tabs, &content);
    TabCtrl_AdjustRect(tabs, FALSE, &content);
    content.left += 16; content.top += 16; content.right -= 0; content.bottom -= 0;
    MoveWindow(apps, content.left, content.top, content.right-content.left-8, content.bottom-content.top-42, TRUE);
    MoveWindow(procs, content.left, content.top, content.right-content.left-8, content.bottom-content.top-42, TRUE);
    MoveWindow(graph, content.left, content.top, content.right-content.left-8, 100, TRUE);
    MoveWindow(memory_text, content.left, content.top+110, content.right-content.left-8, content.bottom-content.top-115, TRUE);
    MoveWindow(end_button, width-368, height-67, 112, 24, TRUE);
    MoveWindow(switch_button, width-248, height-67, 112, 24, TRUE);
    MoveWindow(new_button, width-128, height-67, 112, 24, TRUE);
    ShowWindow(apps, page == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(procs, page == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(graph, page == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(memory_text, page == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(end_button, page < 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(switch_button, page == 0 ? SW_SHOW : SW_HIDE);
    SetWindowTextA(end_button, page == 1 ? "End Process" : "End Task");
    update_actions();
}
static INT_PTR CALLBACK run_dialog(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;
    if (msg == WM_INITDIALOG) { SendDlgItemMessageA(w, IDC_COMMAND, EM_SETLIMITTEXT, 1023, 0); return TRUE; }
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
        /* Closing is asynchronous: a hung application cannot block our UI. */
        if (!PostMessageA((HWND)key, WM_CLOSE, 0, 0)) error_message(main_window, "Close window");
    } else if (page == 1 && selected(procs, &key)) {
        pid = (DWORD)key;
        if (!pid || pid == GetCurrentProcessId()) {
            MessageBoxA(main_window, "This process cannot be ended here.", "Task Manager 95", MB_OK); return;
        }
        process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (!process) { error_message(main_window, "OpenProcess"); return; }
        if (MessageBoxA(main_window, "End the selected process? Unsaved data will be lost.",
            "End Process", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES) {
            if (!TerminateProcess(process, 1)) error_message(main_window, "TerminateProcess");
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
    if (priority != HIGH_PRIORITY_CLASS || MessageBoxA(main_window,
        "High priority can make Windows unresponsive. Continue?", "Set Priority",
        MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) == IDYES)
        if (!SetPriorityClass(process, priority)) error_message(main_window, "SetPriorityClass");
    CloseHandle(process);
}
static HMENU create_menu(void)
{
    HMENU bar = CreateMenu(), file = CreatePopupMenu(), options = CreatePopupMenu();
    HMENU priority = CreatePopupMenu(), help = CreatePopupMenu();
    AppendMenuA(file, MF_STRING, ID_NEW, "&New Task...");
    AppendMenuA(file, MF_SEPARATOR, 0, NULL); AppendMenuA(file, MF_STRING, ID_EXIT, "E&xit");
    AppendMenuA(options, MF_STRING, ID_TOP, "Always on &Top");
    AppendMenuA(options, MF_STRING, ID_PAUSE, "&Pause Updates");
    AppendMenuA(options, MF_STRING, ID_REFRESH, "&Refresh Now");
    AppendMenuA(priority, MF_STRING, ID_IDLE, "&Idle");
    AppendMenuA(priority, MF_STRING, ID_NORMAL, "&Normal");
    AppendMenuA(priority, MF_STRING, ID_HIGH, "&High...");
    AppendMenuA(options, MF_POPUP, (UINT_PTR)priority, "Process &Priority");
    AppendMenuA(help, MF_STRING, ID_ABOUT, "&About");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)file, "&File");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)options, "&Options");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)help, "&Help");
    return bar;
}
static LRESULT CALLBACK window_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    LPARAM key;
    switch (msg) {
    case WM_CREATE: {
        TCITEMA tab;
        main_window = w;
        ui_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        tabs = control(WC_TABCONTROLA, "", WS_TABSTOP, ID_TABS);
        apps = control(WC_LISTVIEWA, "", LVS_REPORT | LVS_SINGLESEL | WS_BORDER | WS_TABSTOP, ID_APPS);
        procs = control(WC_LISTVIEWA, "", LVS_REPORT | LVS_SINGLESEL | WS_BORDER | WS_TABSTOP, ID_PROCS);
        status_bar = control(STATUSCLASSNAMEA, "", SBARS_SIZEGRIP, 0);
        end_button = control("BUTTON", "End Task", WS_TABSTOP, ID_END);
        switch_button = control("BUTTON", "Switch To", WS_TABSTOP, ID_SWITCH);
        new_button = control("BUTTON", "New Task...", WS_TABSTOP, ID_NEW);
        graph = control(GRAPH_CLASS, "", WS_BORDER, 0);
        memory_text = control("STATIC", "", 0, 0);
        if (!tabs || !apps || !procs || !status_bar || !end_button || !switch_button || !new_button || !graph || !memory_text) return -1;
        ZeroMemory(&tab, sizeof(tab)); tab.mask = TCIF_TEXT;
        tab.pszText = "Applications"; SendMessageA(tabs, TCM_INSERTITEMA, 0, (LPARAM)&tab);
        tab.pszText = "Processes"; SendMessageA(tabs, TCM_INSERTITEMA, 1, (LPARAM)&tab);
        tab.pszText = "Performance"; SendMessageA(tabs, TCM_INSERTITEMA, 2, (LPARAM)&tab);
        column(apps, 0, "Task", 420); column(apps, 1, "PID", 90);
        column(procs, 0, "Image Name", 160); column(procs, 1, "PID", 75);
        column(procs, 2, "Threads", 65); column(procs, 3, "Base Priority", 90);
        column(procs, 4, "Executable (as reported)", 280);
        tm_system_init(&system_info);
        refresh();
        if (!SetTimer(w, 1, 1000, NULL)) return -1;
        return 0;
    }
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lp)->ptMinTrackSize.x = 560;
        ((MINMAXINFO *)lp)->ptMinTrackSize.y = 440;
        return 0;
    case WM_SIZE: layout(); return 0;
    case WM_NOTIFY:
        if (((NMHDR *)lp)->idFrom == ID_TABS && ((NMHDR *)lp)->code == TCN_SELCHANGE) {
            page = TabCtrl_GetCurSel(tabs); layout();
        }
        if (((NMHDR *)lp)->code == LVN_ITEMCHANGED) update_actions();
        return 0;
    case WM_TIMER: if (!paused) refresh(); return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_NEW: DialogBoxParamA(instance, MAKEINTRESOURCEA(IDD_RUN), w, run_dialog, 0); refresh(); break;
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
        case ID_ABOUT:
            MessageBoxA(w, "Task Manager 95\r\nNative Win32 prototype\r\nCPU/VxD and networking are not implemented.", "About", MB_OK); break;
        }
        return 0;
    case WM_DESTROY: KillTimer(w, 1); PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(w, msg, wp, lp);
}
void WINAPI WinMainCRTStartup(void)
{
    WNDCLASSA wc;
    MSG msg;
    HWND w;
    BOOL result;
    instance = GetModuleHandleA(NULL);
    InitCommonControls();
    ZeroMemory(&wc, sizeof(wc));
    wc.hInstance = instance; wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpfnWndProc = window_proc; wc.lpszClassName = APP_CLASS;
    if (!RegisterClassA(&wc)) ExitProcess(1);
    wc.lpfnWndProc = graph_proc; wc.lpszClassName = GRAPH_CLASS;
    if (!RegisterClassA(&wc)) ExitProcess(1);
    w = CreateWindowExA(WS_EX_CONTROLPARENT, APP_CLASS, "Task Manager 95",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
        620, 440, NULL, create_menu(), instance, NULL);
    if (!w) ExitProcess(1);
    ShowWindow(w, SW_SHOWDEFAULT); UpdateWindow(w);
    while ((result = GetMessageA(&msg, NULL, 0, 0)) > 0) {
        if (!IsDialogMessageA(w, &msg)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    }
    ExitProcess(result == -1 ? 1 : (UINT)msg.wParam);
}
