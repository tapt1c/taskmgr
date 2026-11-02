#include "processes.h"
#include "columns.h"
#include "owner.h"
#include "process_memory.h"
#include "i18n.h"
#include "resource.h"
#define COLUMN_COUNT 7
#define NAME 0
#define PID 1
#define THREADS 2
#define PRIORITY 3
#define PATH 4
#define USER 5
#define MEMORY 6
#define INI_SECTION "Processes"
typedef struct {
    DWORD pid,threads;
    LONG priority;
    SIZE_T memory_kb;
    BOOL memory_valid;
    int base;
    WCHAR path[MAX_PATH],user[256];
} PROCESS_ROW;
static const int titles[COLUMN_COUNT]={T_IMAGE,T_APP_PID,T_THREADS,T_BASEPRI,T_PATH,T_PROC_USER,T_PROC_MEMORY};
static TM_COLUMNS columns;
static PROCESS_ROW *rows;
static int row_count,sort_column,descending;
static int visible_columns[COLUMN_COUNT],column_count,viewport_width,last_auto_width;
static HWND list_window,owner_window;
static HINSTANCE module_instance;
static HMENU view_menu,sort_menu;
static BOOL initialized,configuring;
static char ini_path[MAX_PATH];
static void save_settings(void)
{
    char key[32],value[32];
    int i;
#define SAVE_INT(key_name,number) wsprintfA(value,"%d",(number));WritePrivateProfileStringA(INI_SECTION,(key_name),value,ini_path)
    SAVE_INT("Sort",sort_column);SAVE_INT("Descending",descending);
    SAVE_INT("AutoNameWidth",columns.autofit);
    for(i=0;i<COLUMN_COUNT;++i) {
        wsprintfA(key,"Order%d",i);SAVE_INT(key,columns.order[i]);
        wsprintfA(key,"Visible%d",i);SAVE_INT(key,columns.visible[i]);
        wsprintfA(key,"Width%d",i);SAVE_INT(key,columns.width[i]);
    }
#undef SAVE_INT
}
static void load_settings(void)
{
    static const int order[COLUMN_COUNT]={NAME,USER,PID,MEMORY,THREADS,PRIORITY,PATH};
    static const int width[COLUMN_COUNT]={150,85,70,115,280,140,100};
    char key[32];
    int i,value,mask=1,count=1;
    columns.count=COLUMN_COUNT;columns.titles=titles;
    sort_column=(int)GetPrivateProfileIntA(INI_SECTION,"Sort",NAME,ini_path);
    if(sort_column<0 || sort_column>=COLUMN_COUNT) sort_column=NAME;
    descending=GetPrivateProfileIntA(INI_SECTION,"Descending",0,ini_path)!=0;
    columns.autofit=GetPrivateProfileIntA(INI_SECTION,"AutoNameWidth",1,ini_path)!=0;
    columns.order[0]=NAME;
    for(i=0;i<COLUMN_COUNT;++i) {
        wsprintfA(key,"Order%d",i);value=(int)GetPrivateProfileIntA(INI_SECTION,key,-1,ini_path);
        if(value>NAME && value<COLUMN_COUNT && !(mask&(1<<value))) {
            columns.order[count++]=value;mask|=1<<value;
        }
        wsprintfA(key,"Visible%d",i);columns.visible[i]=GetPrivateProfileIntA(INI_SECTION,key,1,ini_path)!=0;
        wsprintfA(key,"Width%d",i);value=(int)GetPrivateProfileIntA(INI_SECTION,key,width[i],ini_path);
        columns.width[i]=value>=32 && value<=1200?value:width[i];
    }
    /* Preserve the saved relative order when upgrading from fewer columns.
       Append missing/new columns; malformed or duplicate IDs cannot escape bounds. */
    for(i=0;i<COLUMN_COUNT;++i) if(!(mask&(1<<order[i]))) {
        columns.order[count++]=order[i];mask|=1<<order[i];
    }
    columns.visible[NAME]=TRUE;
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
static const WCHAR *field(PROCESS_ROW *row,int id,WCHAR *scratch)
{
    char value[32];
    if(id==NAME) return row->path+row->base;
    if(id==PATH) return row->path;
    if(id==USER) return row->user[0]?row->user:trw(T_NA);
    if(id==MEMORY) {
        SIZE_T number=row->memory_kb;
        WCHAR reversed[32];
        int count=0,i;
        if(!row->memory_valid) return trw(T_NA);
        do { reversed[count++]=(WCHAR)('0'+number%10);number/=10; } while(number);
        for(i=0;i<count;++i) scratch[i]=reversed[count-i-1];
        scratch[count]=0;return scratch;
    }
    if(id==PID) wsprintfA(value,"%lu",row->pid);
    else if(id==THREADS) wsprintfA(value,"%lu",row->threads);
    else wsprintfA(value,"%ld",row->priority);
    MultiByteToWideChar(CP_ACP,0,value,-1,scratch,32);return scratch;
}
static PROCESS_ROW *find_row(DWORD pid)
{
    int i;
    for(i=0;i<row_count;++i) if(rows[i].pid==pid) return &rows[i];
    return NULL;
}
static int CALLBACK compare_items(LPARAM left,LPARAM right,LPARAM unused)
{
    PROCESS_ROW *a=find_row((DWORD)left),*b=find_row((DWORD)right);
    WCHAR sa[32],sb[32];
    int result;
    (void)unused;
    if(!a || !b) return 0;
    /* Unknown readings sort last in either direction, separately from true zero. */
    if(sort_column==MEMORY && a->memory_valid!=b->memory_valid) return a->memory_valid?-1:1;
    if(sort_column==MEMORY) result=a->memory_kb<b->memory_kb?-1:a->memory_kb>b->memory_kb?1:0;
    else if(sort_column==PID) result=a->pid<b->pid?-1:a->pid>b->pid?1:0;
    else if(sort_column==THREADS) result=a->threads<b->threads?-1:a->threads>b->threads?1:0;
    else if(sort_column==PRIORITY) result=a->priority<b->priority?-1:a->priority>b->priority?1:0;
    else result=compare_w(field(a,sort_column,sa),field(b,sort_column,sb));
    if(!result) result=a->pid<b->pid?-1:a->pid>b->pid?1:0;
    return descending?-result:result;
}
static void sort_items(void) { ListView_SortItems(list_window,compare_items,0); }
static void update_cells(void)
{
    int i,j;
    LVITEMA item;
    WCHAR scratch[32];
    char ansi[MAX_PATH];
    const WCHAR *text;
    for(i=0;i<ListView_GetItemCount(list_window);++i) {
        PROCESS_ROW *row;
        ZeroMemory(&item,sizeof(item));item.mask=LVIF_PARAM;item.iItem=i;
        if(!SendMessageA(list_window,LVM_GETITEMA,0,(LPARAM)&item)) continue;
        row=find_row((DWORD)item.lParam);if(!row) continue;
        for(j=0;j<column_count;++j) {
            text=field(row,visible_columns[j],scratch);
            ZeroMemory(&item,sizeof(item));item.iSubItem=j;
            if(language_unicode()) item.pszText=(LPSTR)text;
            else {
                ansi[0]=0;WideCharToMultiByte(CP_ACP,0,text,-1,ansi,MAX_PATH,NULL,NULL);
                ansi[MAX_PATH-1]=0;item.pszText=ansi;
            }
            SendMessageA(list_window,language_unicode()?LVM_SETITEMTEXTW:LVM_SETITEMTEXTA,i,(LPARAM)&item);
        }
    }
}
static void capture_widths(void)
{
    int i,id,width;
    if(!initialized || configuring) return;
    for(i=0;i<column_count;++i) {
        id=visible_columns[i];width=ListView_GetColumnWidth(list_window,i);
        if(id==NAME && columns.autofit) {
            if(last_auto_width<=0 || width==last_auto_width) continue;
            columns.autofit=FALSE;
        }
        columns.width[id]=width<32?32:width>1200?1200:width;
    }
}
void processes_resize(int width)
{
    int i,name_width=width-20;
    capture_widths();viewport_width=width;
    if(!initialized) return;
    if(columns.autofit) {
        for(i=1;i<column_count;++i) name_width-=columns.width[visible_columns[i]];
        if(name_width<140) name_width=140;
        ListView_SetColumnWidth(list_window,0,name_width);last_auto_width=name_width;
    }
}
static void rebuild_columns(void)
{
    LVCOLUMNA column;
    int i,id,existing;
    BOOL draw=IsWindowVisible(list_window);
    configuring=TRUE;last_auto_width=0;
    if(draw) SendMessageA(list_window,WM_SETREDRAW,FALSE,0);
    existing=Header_GetItemCount((HWND)SendMessageA(list_window,LVM_GETHEADER,0,0));
    /* Preserve column zero for the original Win95 common controls. */
    for(i=existing-1;i>0;--i) ListView_DeleteColumn(list_window,i);
    column_count=0;
    for(i=0;i<COLUMN_COUNT;++i) {
        id=columns.order[i];if(!columns.visible[id]) continue;
        visible_columns[column_count]=id;
        ZeroMemory(&column,sizeof(column));column.mask=LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.cx=columns.width[id];column.iSubItem=column_count;
        column.pszText=(LPSTR)(language_unicode()?(const void *)trw(titles[id]):(const void *)tr(titles[id]));
        if(column_count==0 && existing>0)
            SendMessageA(list_window,language_unicode()?LVM_SETCOLUMNW:LVM_SETCOLUMNA,0,(LPARAM)&column);
        else SendMessageA(list_window,language_unicode()?LVM_INSERTCOLUMNW:LVM_INSERTCOLUMNA,column_count,(LPARAM)&column);
        ++column_count;
    }
    update_cells();processes_resize(viewport_width);
    if(draw) SendMessageA(list_window,WM_SETREDRAW,TRUE,0);
    configuring=FALSE;InvalidateRect(list_window,NULL,TRUE);
}
void processes_refresh(TM_SYSTEM *sys)
{
    HANDLE snapshot;
    PROCESSENTRY32 entry;
    PROCESS_ROW *fresh=NULL,*grown;
    int count=0,capacity=0,i;
    DWORD threads=0,error;
    BOOL ok;
    LVITEMA item;
    if(!initialized) return;
    sys->processes_valid=FALSE;
    snapshot=tm_process_snapshot(sys);
    if(snapshot==INVALID_HANDLE_VALUE) return;
    ZeroMemory(&entry,sizeof(entry));entry.dwSize=sizeof(entry);
    ok=sys->first(snapshot,&entry);
    while(ok) {
        PROCESS_ROW *row;
        if(count==capacity) {
            if(capacity>=65536) { SetLastError(ERROR_NOT_ENOUGH_MEMORY);break; }
            if(fresh) grown=(PROCESS_ROW *)HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,fresh,capacity*2*sizeof(*fresh));
            else grown=(PROCESS_ROW *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,32*sizeof(*fresh));
            if(!grown) { SetLastError(ERROR_NOT_ENOUGH_MEMORY);break; }
            fresh=grown;capacity=capacity?capacity*2:32;
        }
        row=&fresh[count];row->pid=entry.th32ProcessID;row->threads=entry.cntThreads;row->priority=entry.pcPriClassBase;
        if(!MultiByteToWideChar(CP_ACP,0,entry.szExeFile,-1,row->path,MAX_PATH)) row->path[0]=0;
        row->path[MAX_PATH-1]=0;
        for(i=0;row->path[i];++i) if(row->path[i]=='\\' || row->path[i]=='/') row->base=i+1;
        /* Resolve only when displayed or used for sorting. NT tokens are opened fresh. */
        if(columns.visible[USER] || sort_column==USER) owner_name(row->pid,row->user,256);
        if(columns.visible[MEMORY] || sort_column==MEMORY)
            row->memory_valid=process_memory_kb(row->pid,&row->memory_kb);
        threads+=row->threads;++count;entry.dwSize=sizeof(entry);
        ok=sys->next(snapshot,&entry);
    }
    error=GetLastError();CloseHandle(snapshot);
    /* Preserve the last complete table if enumeration or allocation failed. */
    if(ok || error!=ERROR_NO_MORE_FILES) { if(fresh) HeapFree(GetProcessHeap(),0,fresh);return; }
    ListView_DeleteAllItems(list_window);
    if(rows) HeapFree(GetProcessHeap(),0,rows);
    rows=fresh;row_count=count;
    sys->processes=count;sys->threads=threads;sys->processes_valid=TRUE;
    for(i=0;i<row_count;++i) {
        ZeroMemory(&item,sizeof(item));item.mask=LVIF_TEXT | LVIF_PARAM;
        item.iItem=i;item.pszText="";item.lParam=(LPARAM)rows[i].pid;
        SendMessageA(list_window,LVM_INSERTITEMA,0,(LPARAM)&item);
    }
    update_cells();sort_items();
}
void processes_localize(void)
{
    if(initialized) { capture_widths();rebuild_columns();sort_items(); }
}
void processes_menu(HMENU menu)
{
    int i;
    view_menu=menu;sort_menu=CreatePopupMenu();
    language_menu(menu,MF_STRING,ID_VIEW_COLUMNS,T_APP_COLUMNS);
    language_menu(menu,MF_STRING,ID_VIEW_AUTOFIT,T_PROC_AUTOFIT);
    for(i=0;i<COLUMN_COUNT;++i) language_menu(sort_menu,MF_STRING,ID_PROC_SORT_NAME+i,titles[i]);
    AppendMenuA(sort_menu,MF_SEPARATOR,0,NULL);
    language_menu(sort_menu,MF_STRING,ID_PROC_SORT_DESCENDING,T_APP_DESCENDING);
    language_menu(menu,MF_POPUP,(UINT_PTR)sort_menu,T_APP_SORT);
    processes_menu_state();
}
void processes_menu_state(void)
{
    int i;
    if(!view_menu) return;
    CheckMenuItem(view_menu,ID_VIEW_AUTOFIT,MF_BYCOMMAND | (columns.autofit?MF_CHECKED:MF_UNCHECKED));
    for(i=0;i<COLUMN_COUNT;++i) CheckMenuItem(sort_menu,ID_PROC_SORT_NAME+i,MF_BYCOMMAND | (sort_column==i?MF_CHECKED:MF_UNCHECKED));
    CheckMenuItem(sort_menu,ID_PROC_SORT_DESCENDING,MF_BYCOMMAND | (descending?MF_CHECKED:MF_UNCHECKED));
}
BOOL processes_command(UINT command)
{
    if(!initialized || !(command==ID_VIEW_COLUMNS || command==ID_VIEW_AUTOFIT ||
        (command>=ID_PROC_SORT_NAME && command<=ID_PROC_SORT_DESCENDING))) return FALSE;
    capture_widths();
    if(command==ID_VIEW_COLUMNS) {
        if(columns_edit(&columns,module_instance,owner_window,T_PROC_AUTOFIT)) { rebuild_columns();save_settings(); }
        processes_menu_state();return TRUE;
    }
    if(command==ID_VIEW_AUTOFIT) { columns.autofit=!columns.autofit;processes_resize(viewport_width); }
    else if(command==ID_PROC_SORT_DESCENDING) descending=!descending;
    else sort_column=(int)(command-ID_PROC_SORT_NAME);
    sort_items();save_settings();processes_menu_state();return TRUE;
}
void processes_notify(NMHDR *header)
{
    int id;
    if(!initialized || configuring) return;
    if(header->hwndFrom==list_window && header->code==LVN_COLUMNCLICK) {
        NMLISTVIEW *click=(NMLISTVIEW *)header;
        if(click->iSubItem<0 || click->iSubItem>=column_count) return;
        id=visible_columns[click->iSubItem];
        if(sort_column==id) descending=!descending;
        else { sort_column=id;descending=FALSE; }
        sort_items();save_settings();processes_menu_state();
    } else if(header->hwndFrom==(HWND)SendMessageA(list_window,LVM_GETHEADER,0,0) &&
        (header->code==HDN_ENDTRACKA || header->code==HDN_ENDTRACKW)) {
        NMHEADERA *track=(NMHEADERA *)header;
        if(track->iItem<0 || track->iItem>=column_count || !track->pitem || !(track->pitem->mask&HDI_WIDTH)) return;
        id=visible_columns[track->iItem];columns.width[id]=track->pitem->cxy;
        if(columns.width[id]<32) columns.width[id]=32;
        if(columns.width[id]>1200) columns.width[id]=1200;
        if(id==NAME) columns.autofit=FALSE;
        save_settings();processes_menu_state();
    }
}
BOOL processes_init(HWND list,HWND owner,HINSTANCE instance,const char *ini)
{
    DWORD length;
    list_window=list;owner_window=owner;module_instance=instance;
    length=GetFullPathNameA(ini,MAX_PATH,ini_path,NULL);
    if(!length || length>=MAX_PATH) return FALSE;
    owner_init();process_memory_init();load_settings();initialized=TRUE;rebuild_columns();return TRUE;
}
void processes_close(void)
{
    if(initialized) { capture_widths();save_settings(); }
    if(rows) HeapFree(GetProcessHeap(),0,rows);
    rows=NULL;row_count=0;initialized=FALSE;process_memory_close();
}
