#include "i18n.h"
#include "version.h"
static BOOL ru;
static char cache[T_COUNT][512];
static const WCHAR *words[T_COUNT][2] = {
    {L"Task Manager 95 - " TM95_BUILD_LABEL_W, L"Диспетчер задач 95 - " TM95_BUILD_LABEL_W},
    {L"&File", L"&Файл"},
    {L"&Options", L"&Параметры"},
    {L"&View", L"&Вид"},
    {L"&Help", L"&Справка"},
    {L"New Task...", L"Новая задача..."},
    {L"Exit", L"Выход"},
    {L"Always on Top", L"Поверх остальных окон"},
    {L"Pause Updates", L"Приостановить обновление"},
    {L"Refresh Now", L"Обновить сейчас"},
    {L"Process Priority", L"Приоритет процесса"},
    {L"Idle", L"Низкий"},
    {L"Normal", L"Обычный"},
    {L"High...", L"Высокий..."},
    {L"About...", L"О программе..."},
    {L"Language / Язык", L"Язык / Language"},
    {L"Applications", L"Приложения"},
    {L"Processes", L"Процессы"},
    {L"Performance", L"Быстродействие"},
    {L"Task", L"Задача"},
    {L"Image Name", L"Имя образа"},
    {L"Threads", L"Потоки"},
    {L"Base Priority", L"Базовый приоритет"},
    {L"Executable (as reported)", L"Исполняемый файл (данные ОС)"},
    {L"End Task", L"Снять задачу"},
    {L"End Process", L"Завершить процесс"},
    {L"Switch To", L"Переключиться"},
    {L"%s failed (Windows error %lu).", L"Ошибка %s (код Windows: %lu)."},
    {L"Termination is blocked: a critical system component, this task manager, or an unverified process.", L"Завершение запрещено: критичный компонент системы, сам диспетчер или процесс, который не удалось проверить."},
    {L"End the selected process? Unsaved data will be lost.", L"Завершить выбранный процесс? Несохранённые данные будут потеряны."},
    {L"High priority can make Windows unresponsive. Continue?", L"Высокий приоритет может привести к зависанию Windows. Продолжить?"},
    {L"Early alpha. Native Win32 task manager.\nCPU metrics depend on the available provider.\nExperimental driver support; not validated on Windows 95.", L"Ранняя альфа. Диспетчер задач на Win32.\nМетрики ЦП зависят от доступного источника.\nДрайвер экспериментальный; Windows 95 ещё не проверена."},
    {L"Enter a program and any command-line arguments:", L"Введите имя программы и параметры командной строки:"},
    {L"Cancel", L"Отмена"},
    {L"CPU Usage", L"Загрузка ЦП"},
    {L"CPU Usage History", L"Хронология загрузки ЦП"},
    {L"Memory Usage", L"Загрузка памяти"},
    {L"Physical Memory Usage History", L"Хронология загрузки физической памяти"},
    {L"Totals", L"Всего"},
    {L"Physical Memory (K)", L"Физическая память (КБ)"},
    {L"Page-file Capacity (K)", L"Ресурс подкачки (КБ)"},
    {L"Virtual Address Space (K)", L"Виртуальное адресное пространство (КБ)"},
    {L"Total", L"Всего"},
    {L"Available", L"Доступно"},
    {L"Handles", L"Дескрипторов"},
    {L"Processes", L"Процессов"},
    {L"N/A", L"Н/Д"},
    {L"unavailable / stale", L"недоступно / устарело"},
    {L"Processes: %lu", L"Процессов: %lu"},
    {L"Processes: unavailable / stale", L"Процессы: недоступно / устарело"},
    {L"CPU Usage: %lu%%", L"Загрузка ЦП: %lu%%"},
    {L"CPU Usage: N/A", L"Загрузка ЦП: Н/Д"},
    {L"Physical memory: %lu%%", L"Физическая память: %lu%%"},
    {L"CPU data unavailable", L"Нет данных о загрузке ЦП"},
    {L"Russian on Windows 95 requires the Cyrillic system code page (1251).", L"Для русского языка в Windows 95 нужна системная кодовая страница 1251."},
    {L"Experimental VxD",L"Экспериментальный VxD"},
    {L"VMM uptime (s)",L"Время работы VMM (с)"},
    {L"VMM free pages",L"Свободных страниц VMM"},
    {L"CPU Usage",L"ЦП"},
    {L"Memory",L"Память"},
    {L"Memory Usage History",L"Хронология памяти"},
    {L"Physical Memory (K)",L"Физ. память (КБ)"},
    {L"Page File (K)",L"Подкачка (КБ)"},
    {L"Virtual Memory (K)",L"Вирт. память (КБ)"},
    {L"Uptime (s)",L"VMM (с)"},
    {L"VMM pages",L"Страниц VMM"},
    {L"CPU: %lu%%",L"ЦП: %lu%%"},
    {L"CPU: N/A",L"ЦП: Н/Д"},
    {L"Memory: %lu%%",L"Память: %lu%%"},
};
void language_init(BOOL russian)
{
    int i;
    ru = russian;
    for (i = 0; i < T_COUNT; ++i) {
        WideCharToMultiByte(CP_ACP, 0, words[i][ru ? 1 : 0], -1,
            cache[i], sizeof(cache[i]), NULL, NULL);
        cache[i][sizeof(cache[i])-1] = 0;
    }
}
BOOL language_russian(void) { return ru; }
const char *tr(int id) { return cache[id]; }
const WCHAR *trw(int id) { return words[id][ru ? 1 : 0]; }
static BOOL nt;
static BOOL (WINAPI *set_text_w)(HWND,LPCWSTR);
static BOOL (WINAPI *append_menu_w)(HMENU,UINT,UINT_PTR,LPCWSTR);
static int (WINAPI *message_box_w)(HWND,LPCWSTR,LPCWSTR,UINT);
static int (WINAPI *draw_text_w)(HDC,LPCWSTR,int,LPRECT,UINT);
BOOL language_unicode(void)
{
    HMODULE user;
    if(!set_text_w) {
        nt=(GetVersion() & 0x80000000UL)==0;
        user=GetModuleHandleA("USER32.DLL");
        set_text_w=(BOOL (WINAPI *)(HWND,LPCWSTR))GetProcAddress(user,"SetWindowTextW");
        append_menu_w=(BOOL (WINAPI *)(HMENU,UINT,UINT_PTR,LPCWSTR))GetProcAddress(user,"AppendMenuW");
        message_box_w=(int (WINAPI *)(HWND,LPCWSTR,LPCWSTR,UINT))GetProcAddress(user,"MessageBoxW");
        draw_text_w=(int (WINAPI *)(HDC,LPCWSTR,int,LPRECT,UINT))GetProcAddress(user,"DrawTextW");
    }
    return nt && set_text_w && append_menu_w && message_box_w && draw_text_w;
}
void language_text(HWND w,int id)
{
    if(language_unicode()) set_text_w(w,trw(id)); else SetWindowTextA(w,tr(id));
}
void language_menu(HMENU menu,UINT flags,UINT_PTR command,int id)
{
    if(language_unicode()) append_menu_w(menu,flags,command,trw(id));
    else AppendMenuA(menu,flags,command,tr(id));
}
int language_message(HWND w,int text,int caption,UINT flags)
{
    if(language_unicode()) return message_box_w(w,trw(text),trw(caption),flags);
    return MessageBoxA(w,tr(text),tr(caption),flags);
}
void language_draw(HDC dc,RECT *r,int id,UINT flags)
{
    if(language_unicode()) draw_text_w(dc,trw(id),-1,r,flags | DT_SINGLELINE | DT_NOPREFIX);
    else DrawTextA(dc,tr(id),-1,r,flags | DT_SINGLELINE | DT_NOPREFIX);
}
void language_status(HWND bar,int part,int id,DWORD value,BOOL format)
{
    if(language_unicode()) {
        WCHAR text[256];
        int (WINAPIV *print)(LPWSTR,LPCWSTR,...);
        print=(int (WINAPIV *)(LPWSTR,LPCWSTR,...))GetProcAddress(GetModuleHandleA("USER32.DLL"),"wsprintfW");
        if(format && print) {
            print(text,trw(id),value);
            SendMessageA(bar,WM_USER+11,part,(LPARAM)text); /* SB_SETTEXTW */
        } else SendMessageA(bar,WM_USER+11,part,(LPARAM)trw(id));
    } else {
        char text[256];
        if(format) { wsprintfA(text,tr(id),value);SendMessageA(bar,WM_USER+1,part,(LPARAM)text); }
        else SendMessageA(bar,WM_USER+1,part,(LPARAM)tr(id));
    }
}
void language_error(HWND owner,const char *operation,DWORD error)
{
    if(language_unicode()) {
        WCHAR text[256],name[80];
        int (WINAPIV *print)(LPWSTR,LPCWSTR,...);
        MultiByteToWideChar(CP_ACP,0,operation,-1,name,80);
        print=(int (WINAPIV *)(LPWSTR,LPCWSTR,...))GetProcAddress(GetModuleHandleA("USER32.DLL"),"wsprintfW");
        if(print) { print(text,trw(T_ERROR),name,error);message_box_w(owner,text,trw(T_TITLE),MB_OK|MB_ICONEXCLAMATION); }
    } else {
        char text[256];
        wsprintfA(text,tr(T_ERROR),operation,error);
        MessageBoxA(owner,text,tr(T_TITLE),MB_OK|MB_ICONEXCLAMATION);
    }
}
