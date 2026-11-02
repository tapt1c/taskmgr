#ifndef TM95_COLUMNS_H
#define TM95_COLUMNS_H
#include <windows.h>
#define TM95_MAX_COLUMNS 8
typedef struct {
    int count;
    const int *titles;
    int autofit;
    int order[TM95_MAX_COLUMNS],visible[TM95_MAX_COLUMNS],width[TM95_MAX_COLUMNS];
} TM_COLUMNS;
BOOL columns_edit(TM_COLUMNS *columns,HINSTANCE instance,HWND owner,int auto_label);
#endif
