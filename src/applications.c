#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include "applications.h"
#include "i18n.h"
#include "resource.h"
#define COLUMN_COUNT 5
#define TASK 0
#define PID 1
#define STATE 2
#define CLASS 3
#define PROGRAM 4
#define INI_SECTION "Applications"
#define TEXT_LENGTH 256

typedef struct {
    int mode,icons,sort,descending,autofit;
    int order[COLUMN_COUNT],visible[COLUMN_COUNT],width[COLUMN_COUNT];
} APP_SETTINGS;
typedef struct {
    HWND window;
    DWORD pid,icon_tick;
    BOOL used,seen,fallback_cached;
    int state;
    WCHAR title[TEXT_LENGTH],cls[TEXT_LENGTH],program[MAX_PATH];
} APP_ROW;
static HWND list_window,owner_window;
static HINSTANCE module_instance;
static HMENU view_menu,sort_menu;
static APP_SETTINGS settings,draft;
static APP_ROW *rows;
static int capacity,visible_columns[COLUMN_COUNT],column_count,viewport_width,last_auto_width;
static int dialog_selected;
static BOOL configuring,initialized;
static char ini_path[MAX_PATH];
static HIMAGELIST small_images,large_images;
static DWORD icon_spent;
static int (WINAPI *window_text_w)(HWND,LPWSTR,int);
static int (WINAPI *class_name_w)(HWND,LPWSTR,int);
static LRESULT (WINAPI *send_w)(HWND,UINT,WPARAM,LPARAM);
static TM_SNAPSHOT snapshot_modules;
static BOOL (WINAPI *module_first)(HANDLE,LPMODULEENTRY32);
static INT_PTR (WINAPI *dialog_w)(HINSTANCE,LPCWSTR,HWND,DLGPROC,LPARAM);
static const int titles[COLUMN_COUNT]={T_TASK,T_APP_PID,T_APP_STATE,T_APP_CLASS,T_APP_PROGRAM};

static void copy_w(WCHAR *dest,const WCHAR *source,int count)
{
    int i=0;
    while(i<count-1 && source[i]) { dest[i]=source[i];++i; }
    dest[i]=0;
}
static BOOL equal_w(const WCHAR *a,const WCHAR *b)
{
    while(*a && *a==*b) { ++a;++b; }
    return *a==*b;
}
static int compare_w(const WCHAR *a,const WCHAR *b)
{
    WCHAR left,right;
    do {
        left=*a++;right=*b++;
        if((left>='A' && left<='Z') || (left>=0x410 && left<=0x42f)) left+=32;
        if((right>='A' && right<='Z') || (right>=0x410 && right<=0x42f)) right+=32;
        if(left==0x401) left=0x451;
        if(right==0x401) right=0x451;
        if(left!=right) return left<right?-1:1;
    } while(left);
    return 0;
}
static void from_ansi(WCHAR *dest,int count,const char *source)
{
    if(!MultiByteToWideChar(CP_ACP,0,source,-1,dest,count)) dest[0]=0;
    dest[count-1]=0;
}
static void to_ansi(char *dest,int count,const WCHAR *source)
{
    if(!WideCharToMultiByte(CP_ACP,0,source,-1,dest,count,NULL,NULL)) dest[0]=0;
    dest[count-1]=0;
}
static void save_settings(void)
{
    char key[32],value[32];
    int i;
#define SAVE_INT(key_name,number) wsprintfA(value,"%d",(number));WritePrivateProfileStringA(INI_SECTION,(key_name),value,ini_path)
    SAVE_INT("Mode",settings.mode);
    SAVE_INT("Icons",settings.icons);
    SAVE_INT("Sort",settings.sort);
    SAVE_INT("Descending",settings.descending);
    SAVE_INT("AutoTaskWidth",settings.autofit);
    for(i=0;i<COLUMN_COUNT;++i) {
        wsprintfA(key,"Order%d",i);SAVE_INT(key,settings.order[i]);
        wsprintfA(key,"Visible%d",i);SAVE_INT(key,settings.visible[i]);
        wsprintfA(key,"Width%d",i);SAVE_INT(key,settings.width[i]);
    }
#undef SAVE_INT
}
static void load_settings(void)
{
    static const int default_order[COLUMN_COUNT]={TASK,STATE,PID,CLASS,PROGRAM};
    static const int default_width[COLUMN_COUNT]={240,90,96,160,240};
    char key[32];
    int i,value,mask=0;
    settings.mode=(int)GetPrivateProfileIntA(INI_SECTION,"Mode",0,ini_path);
    if(settings.mode<0 || settings.mode>3) settings.mode=0;
    settings.icons=GetPrivateProfileIntA(INI_SECTION,"Icons",1,ini_path)!=0;
    settings.sort=(int)GetPrivateProfileIntA(INI_SECTION,"Sort",TASK,ini_path);
    if(settings.sort<0 || settings.sort>=COLUMN_COUNT) settings.sort=TASK;
    settings.descending=GetPrivateProfileIntA(INI_SECTION,"Descending",0,ini_path)!=0;
    settings.autofit=GetPrivateProfileIntA(INI_SECTION,"AutoTaskWidth",1,ini_path)!=0;
    for(i=0;i<COLUMN_COUNT;++i) {
        wsprintfA(key,"Order%d",i);
        value=(int)GetPrivateProfileIntA(INI_SECTION,key,default_order[i],ini_path);
        if(value<0 || value>=COLUMN_COUNT || (mask&(1<<value))) value=-1;
        settings.order[i]=value;
        if(value>=0) mask|=1<<value;
        wsprintfA(key,"Visible%d",i);
        settings.visible[i]=GetPrivateProfileIntA(INI_SECTION,key,i==TASK || i==STATE,ini_path)!=0;
        wsprintfA(key,"Width%d",i);
        value=(int)GetPrivateProfileIntA(INI_SECTION,key,default_width[i],ini_path);
        settings.width[i]=value>=32 && value<=1200?value:default_width[i];
    }
    if(mask!=31 || settings.order[0]!=TASK)
        for(i=0;i<COLUMN_COUNT;++i) settings.order[i]=default_order[i];
    settings.visible[TASK]=TRUE;
}
static APP_ROW *find_row(HWND window)
{
    int i;
    for(i=0;i<capacity;++i) if(rows[i].used && rows[i].window==window) return &rows[i];
    return NULL;
}
static int find_item(HWND window)
{
    LVFINDINFOA info;
    ZeroMemory(&info,sizeof(info));info.flags=LVFI_PARAM;info.lParam=(LPARAM)window;
    return (int)SendMessageA(list_window,LVM_FINDITEMA,(WPARAM)-1,(LPARAM)&info);
}
static const WCHAR *field(APP_ROW *row,int column,WCHAR *scratch)
{
    char number[24];
    switch(column) {
    case TASK: return row->title;
    case PID: wsprintfA(number,"%lu",row->pid);from_ansi(scratch,32,number);return scratch;
    case STATE: return trw(row->state==1?T_APP_MINIMIZED:row->state==2?T_APP_MAXIMIZED:T_APP_NORMAL);
    case CLASS: return row->cls;
    case PROGRAM: return row->program[0]?row->program:trw(T_NA);
    }
    return L"";
}
static void set_cell(int item,int sub,const WCHAR *text)
{
    LVITEMA cell;
    WCHAR old[TEXT_LENGTH];
    char ansi[TEXT_LENGTH];
    ZeroMemory(&cell,sizeof(cell));cell.iSubItem=sub;cell.cchTextMax=TEXT_LENGTH;
    old[0]=0;ansi[0]=0;
    if(language_unicode()) {
        cell.pszText=(LPSTR)old;
        SendMessageA(list_window,LVM_GETITEMTEXTW,item,(LPARAM)&cell);
        if(equal_w(old,text)) return;
        cell.pszText=(LPSTR)text;
        SendMessageA(list_window,LVM_SETITEMTEXTW,item,(LPARAM)&cell);
    } else {
        cell.pszText=ansi;
        SendMessageA(list_window,LVM_GETITEMTEXTA,item,(LPARAM)&cell);
        from_ansi(old,TEXT_LENGTH,ansi);
        if(equal_w(old,text)) return;
        to_ansi(ansi,sizeof(ansi),text);cell.pszText=ansi;
        SendMessageA(list_window,LVM_SETITEMTEXTA,item,(LPARAM)&cell);
    }
}
static void update_cells(void)
{
    WCHAR scratch[32];
    int i,item,column;
    for(i=0;i<capacity;++i) if(rows[i].used) {
        item=find_item(rows[i].window);
        if(item<0) continue;
        for(column=0;column<column_count;++column)
            set_cell(item,column,field(&rows[i],visible_columns[column],scratch));
    }
}
static int CALLBACK compare_items(LPARAM left,LPARAM right,LPARAM unused)
{
    APP_ROW *a=find_row((HWND)left),*b=find_row((HWND)right);
    WCHAR text_a[32],text_b[32];
    int result;
    (void)unused;
    if(!a || !b) return 0;
    if(settings.sort==PID) result=a->pid<b->pid?-1:a->pid>b->pid?1:0;
    else result=compare_w(field(a,settings.sort,text_a),field(b,settings.sort,text_b));
    if(!result) result=compare_w(a->title,b->title);
    if(!result) result=(UINT_PTR)a->window<(UINT_PTR)b->window?-1:(UINT_PTR)a->window>(UINT_PTR)b->window?1:0;
    return settings.descending?-result:result;
}
static void sort_items(void)
{
    ListView_SortItems(list_window,compare_items,0);
    if(settings.mode==1 || settings.mode==2) ListView_Arrange(list_window,LVA_DEFAULT);
}
static void capture_widths(void)
{
    int i,id,width;
    if(!initialized || configuring || settings.mode!=0) return;
    for(i=0;i<column_count;++i) {
        id=visible_columns[i];width=ListView_GetColumnWidth(list_window,i);
        if(id==TASK && settings.autofit) {
            if(last_auto_width<=0 || width==last_auto_width) continue;
            settings.autofit=FALSE;
        }
        settings.width[id]=width<32?32:width>1200?1200:width;
    }
}
void applications_resize(int width)
{
    int i,task_width=width-20;
    capture_widths();viewport_width=width;
    if(!initialized) return;
    if(settings.autofit) {
        for(i=1;i<column_count;++i) task_width-=settings.width[visible_columns[i]];
        if(task_width<140) task_width=140;
        ListView_SetColumnWidth(list_window,0,task_width);last_auto_width=task_width;
    }
    if(settings.mode==1 || settings.mode==2) ListView_Arrange(list_window,LVA_DEFAULT);
}
static void rebuild_columns(void)
{
    LVCOLUMNA column;
    int i,id,existing;
    DWORD style=(DWORD)GetWindowLongA(list_window,GWL_STYLE);
    BOOL draw=IsWindowVisible(list_window);
    configuring=TRUE;last_auto_width=0;
    if(draw) SendMessageA(list_window,WM_SETREDRAW,FALSE,0);
    if((style&LVS_TYPEMASK)!=LVS_REPORT)
        SetWindowLongA(list_window,GWL_STYLE,(LONG)((style&~LVS_TYPEMASK)|LVS_REPORT));
    existing=Header_GetItemCount((HWND)SendMessageA(list_window,LVM_GETHEADER,0,0));
    /* Original COMCTL32 cannot reliably delete column zero. Keep Task first. */
    for(i=existing-1;i>0;--i) ListView_DeleteColumn(list_window,i);
    column_count=0;
    for(i=0;i<COLUMN_COUNT;++i) {
        id=settings.order[i];
        if(!settings.visible[id]) continue;
        visible_columns[column_count]=id;
        ZeroMemory(&column,sizeof(column));column.mask=LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.cx=settings.width[id];column.iSubItem=column_count;
        column.pszText=(LPSTR)(language_unicode()?(const void *)trw(titles[id]):(const void *)tr(titles[id]));
        if(column_count==0 && existing>0)
            SendMessageA(list_window,language_unicode()?LVM_SETCOLUMNW:LVM_SETCOLUMNA,0,(LPARAM)&column);
        else SendMessageA(list_window,language_unicode()?LVM_INSERTCOLUMNW:LVM_INSERTCOLUMNA,column_count,(LPARAM)&column);
        ++column_count;
    }
    update_cells();applications_resize(viewport_width);
    if((style&LVS_TYPEMASK)!=LVS_REPORT) SetWindowLongA(list_window,GWL_STYLE,(LONG)style);
    if(draw) SendMessageA(list_window,WM_SETREDRAW,TRUE,0);
    configuring=FALSE;InvalidateRect(list_window,NULL,TRUE);
}
static void apply_view(void)
{
    static const DWORD modes[4]={LVS_REPORT,LVS_ICON,LVS_SMALLICON,LVS_LIST};
    DWORD style=(DWORD)GetWindowLongA(list_window,GWL_STYLE);
    SetWindowLongA(list_window,GWL_STYLE,(LONG)((style&~LVS_TYPEMASK)|modes[settings.mode]));
    rebuild_columns();
    ListView_SetImageList(list_window,settings.icons?small_images:NULL,LVSIL_SMALL);
    ListView_SetImageList(list_window,settings.icons?large_images:NULL,LVSIL_NORMAL);
    SetWindowPos(list_window,NULL,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    sort_items();InvalidateRect(list_window,NULL,TRUE);
}
static void copy_images(int slot,HICON small,HICON large)
{
    HICON fallback=LoadIconA(NULL,IDI_APPLICATION);
    int target=slot<ImageList_GetImageCount(small_images)?slot:-1;
    if(!small) small=large?large:fallback;
    if(!large) large=small;
    if(ImageList_ReplaceIcon(small_images,target,small)<0) ImageList_ReplaceIcon(small_images,target,fallback);
    target=slot<ImageList_GetImageCount(large_images)?slot:-1;
    if(ImageList_ReplaceIcon(large_images,target,large)<0) ImageList_ReplaceIcon(large_images,target,fallback);
    /* These are borrowed window/class icons. Image lists copy them. */
}
static void executable_icons(APP_ROW *row,HICON *small,HICON *large)
{
    MODULEENTRY32 module;
    HANDLE snapshot;
    char path[MAX_PATH];
    if(snapshot_modules && module_first) {
        snapshot=snapshot_modules(TH32CS_SNAPMODULE,row->pid);
        if(snapshot!=INVALID_HANDLE_VALUE) {
            ZeroMemory(&module,sizeof(module));module.dwSize=sizeof(module);
            if(module_first(snapshot,&module)) from_ansi(row->program,MAX_PATH,module.szExePath);
            CloseHandle(snapshot);
        }
    }
    if(row->program[0]) {
        to_ansi(path,sizeof(path),row->program);
        ExtractIconExA(path,0,large,small,1);
    }
}
static void window_icons(APP_ROW *row,int slot,BOOL force)
{
    DWORD now=GetTickCount(),started=0;
    BOOL queried=FALSE;
    DWORD_PTR answer=0;
    HICON small=NULL,large=NULL,owned_small=NULL,owned_large=NULL;
    BOOL responsive=FALSE;
    if(!force && (DWORD)(now-row->icon_tick)<5000) return;
    if(icon_spent<100) {
        queried=TRUE;started=GetTickCount();SetLastError(0);
        responsive=SendMessageTimeoutA(row->window,WM_GETICON,ICON_SMALL,0,SMTO_ABORTIFHUNG | SMTO_BLOCK,100,&answer)!=0;
        if(responsive) small=(HICON)answer;
        if(responsive && icon_spent+(DWORD)(GetTickCount()-started)<100) {
            answer=0;
            if(SendMessageTimeoutA(row->window,WM_GETICON,ICON_BIG,0,SMTO_ABORTIFHUNG | SMTO_BLOCK,100,&answer)) large=(HICON)answer;
        }
    }
    if(queried) icon_spent+=(DWORD)(GetTickCount()-started);
    if(!small && large) small=large;
    if(!small) small=(HICON)GetClassLongPtrA(row->window,GCLP_HICONSM);
    if(!large) large=(HICON)GetClassLongPtrA(row->window,GCLP_HICON);
    if(!small && !large) {
        if(!force && row->fallback_cached) { row->icon_tick=queried?now:now-5000;return; }
        executable_icons(row,&owned_small,&owned_large);
        small=owned_small;large=owned_large;row->fallback_cached=TRUE;
    } else row->fallback_cached=FALSE;
    copy_images(slot,small,large);
    if(owned_small) DestroyIcon(owned_small);
    if(owned_large && owned_large!=owned_small) DestroyIcon(owned_large);
    row->icon_tick=queried?now:now-5000;
}
static APP_ROW *new_row(int *slot)
{
    APP_ROW *grown;
    int i,old;
    for(i=0;i<capacity;++i) if(!rows[i].used) { *slot=i;return &rows[i]; }
    old=capacity;
    if(capacity>16384) return NULL;
    if(rows) grown=(APP_ROW *)HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,rows,(capacity*2)*sizeof(*rows));
    else grown=(APP_ROW *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,32*sizeof(*rows));
    if(!grown) return NULL;
    rows=grown;capacity=old?old*2:32;*slot=old;return &rows[old];
}
static BOOL CALLBACK enumerate(HWND window,LPARAM unused)
{
    WCHAR title[TEXT_LENGTH];
    char ansi[TEXT_LENGTH];
    APP_ROW *row;
    LVITEMA item;
    DWORD pid=0;
    int slot,index;
    BOOL fresh;
    (void)unused;
    if(window==owner_window || !IsWindowVisible(window) || GetWindow(window,GW_OWNER)) return TRUE;
    GetWindowThreadProcessId(window,&pid);
    if(!pid) return TRUE;
    if(window_text_w && language_unicode()) {
        if(!window_text_w(window,title,TEXT_LENGTH)) return TRUE;
    } else {
        if(!GetWindowTextA(window,ansi,sizeof(ansi))) return TRUE;
        from_ansi(title,TEXT_LENGTH,ansi);
    }
    row=find_row(window);fresh=!row || row->pid!=pid;
    if(!row) row=new_row(&slot);else slot=(int)(row-rows);
    if(!row) return TRUE;
    if(fresh) ZeroMemory(row,sizeof(*row));
    row->used=TRUE;row->seen=TRUE;row->window=window;row->pid=pid;
    copy_w(row->title,title,TEXT_LENGTH);
    if(class_name_w && language_unicode()) class_name_w(window,row->cls,TEXT_LENGTH);
    else { ansi[0]=0;GetClassNameA(window,ansi,sizeof(ansi));from_ansi(row->cls,TEXT_LENGTH,ansi); }
    row->state=IsIconic(window)?1:IsZoomed(window)?2:0;
    window_icons(row,slot,fresh);
    index=find_item(window);
    if(index<0) {
        ZeroMemory(&item,sizeof(item));item.mask=LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
        item.iItem=ListView_GetItemCount(list_window);item.pszText="";
        item.lParam=(LPARAM)window;item.iImage=slot;
        SendMessageA(list_window,LVM_INSERTITEMA,0,(LPARAM)&item);
    }
    return TRUE;
}
void applications_refresh(TM_SYSTEM *sys)
{
    HANDLE snapshot;
    PROCESSENTRY32 entry;
    BOOL draw=IsWindowVisible(list_window);
    int i,item;
    if(!initialized) return;
    if(draw) SendMessageA(list_window,WM_SETREDRAW,FALSE,0);
    for(i=0;i<capacity;++i) rows[i].seen=FALSE;
    icon_spent=0;EnumWindows(enumerate,0);
    for(i=0;i<capacity;++i) if(rows[i].used && !rows[i].seen) {
        item=find_item(rows[i].window);
        if(item>=0) ListView_DeleteItem(list_window,item);
        rows[i].used=FALSE;
    }
    if(settings.visible[PROGRAM] || settings.sort==PROGRAM) {
        snapshot=tm_process_snapshot(sys);
        if(snapshot!=INVALID_HANDLE_VALUE) {
            ZeroMemory(&entry,sizeof(entry));entry.dwSize=sizeof(entry);
            if(sys->first(snapshot,&entry)) do {
                for(i=0;i<capacity;++i) if(rows[i].used && rows[i].pid==entry.th32ProcessID && !rows[i].program[0])
                    from_ansi(rows[i].program,MAX_PATH,entry.szExeFile);
                entry.dwSize=sizeof(entry);
            } while(sys->next(snapshot,&entry));
            CloseHandle(snapshot);
        }
    }
    update_cells();sort_items();
    if(draw) SendMessageA(list_window,WM_SETREDRAW,TRUE,0);
    InvalidateRect(list_window,NULL,FALSE);
}
void applications_localize(void)
{
    if(initialized) { capture_widths();rebuild_columns();sort_items(); }
}
void applications_menu(HMENU menu)
{
    HMENU sorting=CreatePopupMenu();
    int i;
    view_menu=menu;sort_menu=sorting;
    language_menu(menu,MF_STRING,ID_VIEW_DETAILS,T_APP_DETAILS);
    language_menu(menu,MF_STRING,ID_VIEW_LARGE,T_APP_LARGE);
    language_menu(menu,MF_STRING,ID_VIEW_SMALL,T_APP_SMALL);
    language_menu(menu,MF_STRING,ID_VIEW_LIST,T_APP_LIST);
    AppendMenuA(menu,MF_SEPARATOR,0,NULL);
    language_menu(menu,MF_STRING,ID_VIEW_ICONS,T_APP_ICONS);
    language_menu(menu,MF_STRING,ID_VIEW_AUTOFIT,T_APP_AUTOFIT);
    language_menu(menu,MF_STRING,ID_VIEW_COLUMNS,T_APP_COLUMNS);
    for(i=0;i<COLUMN_COUNT;++i) language_menu(sorting,MF_STRING,ID_SORT_TASK+i,titles[i]);
    AppendMenuA(sorting,MF_SEPARATOR,0,NULL);
    language_menu(sorting,MF_STRING,ID_SORT_DESCENDING,T_APP_DESCENDING);
    language_menu(menu,MF_POPUP,(UINT_PTR)sorting,T_APP_SORT);
    applications_menu_state(TRUE);
}
void applications_menu_state(BOOL active)
{
    int i;
    HMENU sorting;
    if(!view_menu) return;
    for(i=0;i<4;++i) CheckMenuItem(view_menu,ID_VIEW_DETAILS+i,MF_BYCOMMAND | (settings.mode==i?MF_CHECKED:MF_UNCHECKED));
    CheckMenuItem(view_menu,ID_VIEW_ICONS,MF_BYCOMMAND | (settings.icons?MF_CHECKED:MF_UNCHECKED));
    CheckMenuItem(view_menu,ID_VIEW_AUTOFIT,MF_BYCOMMAND | (settings.autofit?MF_CHECKED:MF_UNCHECKED));
    sorting=sort_menu;
    if(sorting) {
        for(i=0;i<COLUMN_COUNT;++i) CheckMenuItem(sorting,ID_SORT_TASK+i,MF_BYCOMMAND | (settings.sort==i?MF_CHECKED:MF_UNCHECKED));
        CheckMenuItem(sorting,ID_SORT_DESCENDING,MF_BYCOMMAND | (settings.descending?MF_CHECKED:MF_UNCHECKED));
    }
    if(owner_window && GetMenu(owner_window)) {
        EnableMenuItem(GetMenu(owner_window),2,MF_BYPOSITION | (active?MF_ENABLED:MF_GRAYED));
        DrawMenuBar(owner_window);
    }
}
static int draft_id(void) { return draft.order[dialog_selected]; }
static void dialog_controls(HWND dialog)
{
    char width[24];
    int id=draft_id();
    CheckDlgButton(dialog,IDC_COLUMN_VISIBLE,draft.visible[id]?BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(dialog,IDC_COLUMN_AUTOFIT,draft.autofit?BST_CHECKED:BST_UNCHECKED);
    EnableWindow(GetDlgItem(dialog,IDC_COLUMN_VISIBLE),id!=TASK);
    EnableWindow(GetDlgItem(dialog,IDC_COLUMN_UP),dialog_selected>1);
    EnableWindow(GetDlgItem(dialog,IDC_COLUMN_DOWN),dialog_selected>0 && dialog_selected<COLUMN_COUNT-1);
    wsprintfA(width,"%d",draft.width[id]);SetDlgItemTextA(dialog,IDC_COLUMN_WIDTH,width);
    EnableWindow(GetDlgItem(dialog,IDC_COLUMN_WIDTH),id!=TASK || !draft.autofit);
}
static void dialog_list(HWND dialog)
{
    int i;
    HWND box=GetDlgItem(dialog,IDC_COLUMNS_LIST);
    SendMessageA(box,LB_RESETCONTENT,0,0);
    for(i=0;i<COLUMN_COUNT;++i) {
        if(send_w && language_unicode()) send_w(box,LB_ADDSTRING,0,(LPARAM)trw(titles[draft.order[i]]));
        else SendMessageA(box,LB_ADDSTRING,0,(LPARAM)tr(titles[draft.order[i]]));
    }
    SendMessageA(box,LB_SETCURSEL,dialog_selected,0);dialog_controls(dialog);
}
static BOOL dialog_width(HWND dialog)
{
    BOOL valid;
    UINT value;
    int id=draft_id();
    if(id==TASK && draft.autofit) return TRUE;
    value=GetDlgItemInt(dialog,IDC_COLUMN_WIDTH,&valid,FALSE);
    if(!valid || value<32 || value>1200) {
        language_message(dialog,T_APP_WIDTH_ERROR,T_APP_COLUMNS,MB_OK | MB_ICONEXCLAMATION);
        SetFocus(GetDlgItem(dialog,IDC_COLUMN_WIDTH));return FALSE;
    }
    draft.width[id]=(int)value;return TRUE;
}
static INT_PTR CALLBACK columns_dialog(HWND dialog,UINT message,WPARAM wp,LPARAM lp)
{
    int command,id,selected,target,temp;
    (void)lp;
    if(message==WM_INITDIALOG) {
        draft=settings;dialog_selected=0;
        language_text(dialog,T_APP_COLUMNS);
        language_text(GetDlgItem(dialog,IDC_COLUMN_PROMPT),T_APP_COLUMN_PROMPT);
        language_text(GetDlgItem(dialog,IDC_COLUMN_UP),T_APP_UP);
        language_text(GetDlgItem(dialog,IDC_COLUMN_DOWN),T_APP_DOWN);
        language_text(GetDlgItem(dialog,IDC_COLUMN_VISIBLE),T_APP_VISIBLE);
        language_text(GetDlgItem(dialog,IDC_COLUMN_WIDTH_LABEL),T_APP_WIDTH);
        language_text(GetDlgItem(dialog,IDC_COLUMN_AUTOFIT),T_APP_AUTOFIT);
        language_text(GetDlgItem(dialog,IDCANCEL),T_CANCEL);
        SendDlgItemMessageA(dialog,IDC_COLUMN_WIDTH,EM_SETLIMITTEXT,4,0);
        dialog_list(dialog);return TRUE;
    }
    if(message!=WM_COMMAND) return FALSE;
    command=LOWORD(wp);
    if(command==IDCANCEL) { EndDialog(dialog,0);return TRUE; }
    if(command==IDC_COLUMNS_LIST && HIWORD(wp)==LBN_SELCHANGE) {
        selected=(int)SendDlgItemMessageA(dialog,IDC_COLUMNS_LIST,LB_GETCURSEL,0,0);
        if(selected<0) return TRUE;
        if(!dialog_width(dialog)) { SendDlgItemMessageA(dialog,IDC_COLUMNS_LIST,LB_SETCURSEL,dialog_selected,0);return TRUE; }
        dialog_selected=selected;dialog_controls(dialog);return TRUE;
    }
    if(command==IDC_COLUMN_VISIBLE) {
        id=draft_id();if(id!=TASK) draft.visible[id]=IsDlgButtonChecked(dialog,command)==BST_CHECKED;
        return TRUE;
    }
    if(command==IDC_COLUMN_AUTOFIT) {
        if(!dialog_width(dialog)) {
            CheckDlgButton(dialog,command,draft.autofit?BST_CHECKED:BST_UNCHECKED);return TRUE;
        }
        draft.autofit=IsDlgButtonChecked(dialog,command)==BST_CHECKED;
        dialog_controls(dialog);return TRUE;
    }
    if(command==IDC_COLUMN_UP || command==IDC_COLUMN_DOWN) {
        if(!dialog_width(dialog)) return TRUE;
        target=dialog_selected+(command==IDC_COLUMN_UP?-1:1);
        if(dialog_selected==0 || target<1 || target>=COLUMN_COUNT) return TRUE;
        temp=draft.order[target];draft.order[target]=draft.order[dialog_selected];draft.order[dialog_selected]=temp;
        dialog_selected=target;dialog_list(dialog);return TRUE;
    }
    if(command==IDOK) {
        if(!dialog_width(dialog)) return TRUE;
        settings=draft;settings.visible[TASK]=TRUE;
        save_settings();rebuild_columns();sort_items();applications_menu_state(TRUE);
        EndDialog(dialog,1);return TRUE;
    }
    return FALSE;
}
BOOL applications_command(UINT command)
{
    if(!initialized) return FALSE;
    if(!((command>=ID_VIEW_DETAILS && command<=ID_VIEW_AUTOFIT) || (command>=ID_SORT_TASK && command<=ID_SORT_DESCENDING))) return FALSE;
    capture_widths();
    if(command>=ID_VIEW_DETAILS && command<=ID_VIEW_LIST) {
        settings.mode=(int)(command-ID_VIEW_DETAILS);apply_view();
    } else if(command==ID_VIEW_ICONS) { settings.icons=!settings.icons;apply_view(); }
    else if(command==ID_VIEW_AUTOFIT) { settings.autofit=!settings.autofit;applications_resize(viewport_width); }
    else if(command==ID_VIEW_COLUMNS) {
        if(dialog_w && language_unicode()) dialog_w(module_instance,MAKEINTRESOURCEW(IDD_COLUMNS),owner_window,columns_dialog,0);
        else DialogBoxParamA(module_instance,MAKEINTRESOURCEA(IDD_COLUMNS),owner_window,columns_dialog,0);
        return TRUE;
    } else if(command>=ID_SORT_TASK && command<=ID_SORT_PROGRAM) {
        settings.sort=(int)(command-ID_SORT_TASK);sort_items();
    } else if(command==ID_SORT_DESCENDING) { settings.descending=!settings.descending;sort_items(); }
    else return FALSE;
    save_settings();applications_menu_state(TRUE);return TRUE;
}
void applications_notify(NMHDR *header)
{
    NMLISTVIEW *click;
    NMHEADERA *track;
    int id;
    if(!initialized || configuring) return;
    if(header->hwndFrom==list_window && header->code==LVN_COLUMNCLICK) {
        click=(NMLISTVIEW *)header;
        if(click->iSubItem<0 || click->iSubItem>=column_count) return;
        id=visible_columns[click->iSubItem];
        if(settings.sort==id) settings.descending=!settings.descending;
        else { settings.sort=id;settings.descending=FALSE; }
        sort_items();save_settings();applications_menu_state(TRUE);
    } else if(header->hwndFrom==(HWND)SendMessageA(list_window,LVM_GETHEADER,0,0) &&
        (header->code==HDN_ENDTRACKA || header->code==HDN_ENDTRACKW)) {
        track=(NMHEADERA *)header;
        if(track->iItem<0 || track->iItem>=column_count || !track->pitem || !(track->pitem->mask&HDI_WIDTH)) return;
        id=visible_columns[track->iItem];
        settings.width[id]=track->pitem->cxy;
        if(settings.width[id]<32) settings.width[id]=32;
        if(settings.width[id]>1200) settings.width[id]=1200;
        if(id==TASK) settings.autofit=FALSE;
        save_settings();applications_menu_state(TRUE);
    }
}
BOOL applications_init(HWND list,HWND owner,HINSTANCE instance,const char *ini)
{
    HMODULE user=GetModuleHandleA("USER32.DLL");
    DWORD path_length;
    HMODULE kernel=GetModuleHandleA("KERNEL32.DLL");
    list_window=list;owner_window=owner;module_instance=instance;
    if(lstrlenA(ini)>=MAX_PATH) return FALSE;
    path_length=GetFullPathNameA(ini,MAX_PATH,ini_path,NULL);
    if(!path_length || path_length>=MAX_PATH) return FALSE;
    load_settings();
    snapshot_modules=(TM_SNAPSHOT)GetProcAddress(kernel,"CreateToolhelp32Snapshot");
    module_first=(BOOL (WINAPI *)(HANDLE,LPMODULEENTRY32))GetProcAddress(kernel,"Module32First");
    if(!module_first) module_first=(BOOL (WINAPI *)(HANDLE,LPMODULEENTRY32))GetProcAddress(kernel,"Module32FirstA");
    window_text_w=(int (WINAPI *)(HWND,LPWSTR,int))GetProcAddress(user,"GetWindowTextW");
    class_name_w=(int (WINAPI *)(HWND,LPWSTR,int))GetProcAddress(user,"GetClassNameW");
    send_w=(LRESULT (WINAPI *)(HWND,UINT,WPARAM,LPARAM))GetProcAddress(user,"SendMessageW");
    dialog_w=(INT_PTR (WINAPI *)(HINSTANCE,LPCWSTR,HWND,DLGPROC,LPARAM))GetProcAddress(user,"DialogBoxParamW");
    small_images=ImageList_Create(GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),ILC_COLOR | ILC_MASK,8,8);
    large_images=ImageList_Create(GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),ILC_COLOR | ILC_MASK,8,8);
    if(!small_images || !large_images) { applications_close();return FALSE; }
    initialized=TRUE;rebuild_columns();apply_view();return TRUE;
}
void applications_close(void)
{
    if(initialized) {
        capture_widths();
        save_settings();
        ListView_SetImageList(list_window,NULL,LVSIL_SMALL);ListView_SetImageList(list_window,NULL,LVSIL_NORMAL);
    }
    if(small_images) ImageList_Destroy(small_images);
    if(large_images) ImageList_Destroy(large_images);
    small_images=large_images=NULL;
    if(rows) HeapFree(GetProcessHeap(),0,rows);
    rows=NULL;capacity=0;initialized=FALSE;
}
