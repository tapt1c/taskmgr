#include <windows.h>
#include "columns.h"
#include "resource.h"
#include "i18n.h"
static TM_COLUMNS draft,*active;
static int dialog_selected,auto_title;
static LRESULT (WINAPI *send_w)(HWND,UINT,WPARAM,LPARAM);
static int draft_id(void) { return draft.order[dialog_selected]; }
static void dialog_controls(HWND dialog)
{
    char width[24];
    int id=draft_id();
    CheckDlgButton(dialog,IDC_COLUMN_VISIBLE,draft.visible[id]?BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(dialog,IDC_COLUMN_AUTOFIT,draft.autofit?BST_CHECKED:BST_UNCHECKED);
    EnableWindow(GetDlgItem(dialog,IDC_COLUMN_VISIBLE),id!=0);
    EnableWindow(GetDlgItem(dialog,IDC_COLUMN_UP),dialog_selected>1);
    EnableWindow(GetDlgItem(dialog,IDC_COLUMN_DOWN),dialog_selected>0 && dialog_selected<draft.count-1);
    wsprintfA(width,"%d",draft.width[id]);SetDlgItemTextA(dialog,IDC_COLUMN_WIDTH,width);
    EnableWindow(GetDlgItem(dialog,IDC_COLUMN_WIDTH),id!=0 || !draft.autofit);
}
static void dialog_list(HWND dialog)
{
    int i;
    HWND box=GetDlgItem(dialog,IDC_COLUMNS_LIST);
    SendMessageA(box,LB_RESETCONTENT,0,0);
    for(i=0;i<draft.count;++i) {
        if(send_w && language_unicode()) send_w(box,LB_ADDSTRING,0,(LPARAM)trw(draft.titles[draft.order[i]]));
        else SendMessageA(box,LB_ADDSTRING,0,(LPARAM)tr(draft.titles[draft.order[i]]));
    }
    SendMessageA(box,LB_SETCURSEL,dialog_selected,0);dialog_controls(dialog);
}
static BOOL dialog_width(HWND dialog)
{
    BOOL valid;
    UINT value;
    int id=draft_id();
    if(id==0 && draft.autofit) return TRUE;
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
        draft=*active;dialog_selected=0;
        language_text(dialog,T_APP_COLUMNS);
        language_text(GetDlgItem(dialog,IDC_COLUMN_PROMPT),T_APP_COLUMN_PROMPT);
        language_text(GetDlgItem(dialog,IDC_COLUMN_UP),T_APP_UP);
        language_text(GetDlgItem(dialog,IDC_COLUMN_DOWN),T_APP_DOWN);
        language_text(GetDlgItem(dialog,IDC_COLUMN_VISIBLE),T_APP_VISIBLE);
        language_text(GetDlgItem(dialog,IDC_COLUMN_WIDTH_LABEL),T_APP_WIDTH);
        language_text(GetDlgItem(dialog,IDC_COLUMN_AUTOFIT),auto_title);
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
        id=draft_id();if(id!=0) draft.visible[id]=IsDlgButtonChecked(dialog,command)==BST_CHECKED;
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
        if(dialog_selected==0 || target<1 || target>=draft.count) return TRUE;
        temp=draft.order[target];draft.order[target]=draft.order[dialog_selected];draft.order[dialog_selected]=temp;
        dialog_selected=target;dialog_list(dialog);return TRUE;
    }
    if(command==IDOK) {
        if(!dialog_width(dialog)) return TRUE;
        *active=draft;active->visible[0]=TRUE;
        EndDialog(dialog,1);return TRUE;
    }
    return FALSE;
}
BOOL columns_edit(TM_COLUMNS *columns,HINSTANCE instance,HWND owner,int auto_label)
{
    INT_PTR result;
    INT_PTR (WINAPI *dialog_w)(HINSTANCE,LPCWSTR,HWND,DLGPROC,LPARAM);
    HMODULE user=GetModuleHandleA("USER32.DLL");
    active=columns;auto_title=auto_label;
    send_w=(LRESULT (WINAPI *)(HWND,UINT,WPARAM,LPARAM))GetProcAddress(user,"SendMessageW");
    dialog_w=(INT_PTR (WINAPI *)(HINSTANCE,LPCWSTR,HWND,DLGPROC,LPARAM))GetProcAddress(user,"DialogBoxParamW");
    if(dialog_w && language_unicode()) result=dialog_w(instance,MAKEINTRESOURCEW(IDD_COLUMNS),owner,columns_dialog,0);
    else result=DialogBoxParamA(instance,MAKEINTRESOURCEA(IDD_COLUMNS),owner,columns_dialog,0);
    return result==1;
}
