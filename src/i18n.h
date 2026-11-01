#ifndef TM95_I18N_H
#define TM95_I18N_H
#include <windows.h>
enum {
    T_TITLE,
    T_FILE,
    T_OPTIONS,
    T_VIEW,
    T_HELP,
    T_NEW,
    T_EXIT,
    T_TOP,
    T_PAUSE,
    T_REFRESH,
    T_PRIORITY,
    T_IDLE,
    T_NORMAL,
    T_HIGH,
    T_ABOUT,
    T_LANGUAGE,
    T_APPS,
    T_PROCS,
    T_PERF,
    T_TASK,
    T_IMAGE,
    T_THREADS,
    T_BASEPRI,
    T_PATH,
    T_ENDTASK,
    T_ENDPROCESS,
    T_SWITCH,
    T_ERROR,
    T_PROTECTED,
    T_CONFIRMEND,
    T_CONFIRMHIGH,
    T_ABOUTTEXT,
    T_RUNPROMPT,
    T_CANCEL,
    T_CPU,
    T_CPUHISTORY,
    T_MEMLOAD,
    T_MEMHISTORY,
    T_TOTALS,
    T_PHYSICAL,
    T_PAGING,
    T_VIRTUAL,
    T_TOTAL,
    T_AVAILABLE,
    T_HANDLES,
    T_PROCESSES,
    T_NA,
    T_STALE,
    T_STATPROC,
    T_STATSTALE,
    T_STATCPU,
    T_STATCPUNA,
    T_STATMEM,
    T_CPUNOTE,
    T_RUSSIANACP,
    T_DRIVER,
    T_UPTIME,
    T_VMMPAGES,
    T_COUNT
};
void language_init(BOOL russian);
BOOL language_russian(void);
const char *tr(int id);
const WCHAR *trw(int id);
/* ANSI on Win95; Unicode rendering on NT/Wine avoids ACP-dependent Russian. */
BOOL language_unicode(void);
void language_text(HWND w, int id);
void language_menu(HMENU menu, UINT flags, UINT_PTR command, int id);
int language_message(HWND w, int text, int caption, UINT flags);
void language_draw(HDC dc, RECT *r, int id, UINT flags);

void language_status(HWND bar,int part,int id,DWORD value,BOOL format);
void language_error(HWND owner,const char *operation,DWORD error);
#endif
