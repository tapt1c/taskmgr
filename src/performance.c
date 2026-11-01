#include <windows.h>
#include "performance.h"
#include "i18n.h"
#define HISTORY 120
#define MIN_CONTENT_WIDTH 520
#define MIN_CONTENT_HEIGHT 300
static TM_SYSTEM data;
static DWORD memory_history[HISTORY], cpu_history[HISTORY];
static unsigned int samples;
static HFONT font;
static int scroll_x, scroll_y;
static BOOL sizing;

static void scroll_layout(HWND w)
{
    RECT r;
    SCROLLINFO info;
    int pass;
    if(sizing) return;
    sizing=TRUE;
    /* Showing one scrollbar can reduce space enough to require the other. */
    for(pass=0;pass<2;++pass) {
        GetClientRect(w,&r);
        ZeroMemory(&info,sizeof(info));info.cbSize=sizeof(info);
        info.fMask=SIF_RANGE | SIF_PAGE | SIF_POS;
        info.nMax=MIN_CONTENT_WIDTH-1;info.nPage=r.right;info.nPos=scroll_x;
        SetScrollInfo(w,SB_HORZ,&info,TRUE);
        scroll_x=GetScrollPos(w,SB_HORZ);
        GetClientRect(w,&r);
        info.nMax=MIN_CONTENT_HEIGHT-1;info.nPage=r.bottom;info.nPos=scroll_y;
        SetScrollInfo(w,SB_VERT,&info,TRUE);
        scroll_y=GetScrollPos(w,SB_VERT);
    }
    sizing=FALSE;
    InvalidateRect(w,NULL,FALSE);
}

static void scroll_content(HWND w,int bar,WPARAM command)
{
    SCROLLINFO info;
    int position;
    ZeroMemory(&info,sizeof(info));info.cbSize=sizeof(info);
    info.fMask=SIF_ALL;GetScrollInfo(w,bar,&info);
    position=info.nPos;
    switch(LOWORD(command)) {
    case SB_LINEUP: position-=16;break;
    case SB_LINEDOWN: position+=16;break;
    case SB_PAGEUP: position-=(int)info.nPage;break;
    case SB_PAGEDOWN: position+=(int)info.nPage;break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK: position=info.nTrackPos;break;
    case SB_TOP: position=info.nMin;break;
    case SB_BOTTOM: position=info.nMax;break;
    default: return;
    }
    info.fMask=SIF_POS;info.nPos=position;
    SetScrollInfo(w,bar,&info,TRUE);
    if(bar==SB_HORZ) scroll_x=GetScrollPos(w,bar);
    else scroll_y=GetScrollPos(w,bar);
    InvalidateRect(w,NULL,FALSE);
}

static void label(HDC dc, RECT r, const char *text, UINT flags)
{
    DrawTextA(dc, text, -1, &r, flags | DT_SINGLELINE | DT_NOPREFIX);
}
static RECT group(HDC dc, int x, int y, int width, int height, int title)
{
    RECT r, caption;

    r.left=x; r.top=y+6; r.right=x+width; r.bottom=y+height;
    DrawEdge(dc, &r, EDGE_ETCHED, BF_RECT);
    caption.left=0;caption.top=0;caption.right=0;caption.bottom=0;
    language_draw(dc,&caption,title,DT_CALCRECT);
    caption.right=caption.right+8+x+8;caption.left=x+8;caption.top=y;caption.bottom=y+16;
    if(caption.right>x+width-8) caption.right=x+width-8;
    FillRect(dc, &caption, GetSysColorBrush(COLOR_BTNFACE));
    caption.left+=4;
    language_draw(dc,&caption,title,DT_LEFT);
    r.left+=12; r.top=y+23; r.right-=12; r.bottom-=10;
    return r;
}
static void number(HDC dc, RECT r, int title, DWORD value, BOOL valid)
{
    char text[40];
    language_draw(dc,&r,title,DT_LEFT);
    if (valid) wsprintfA(text, "%lu", value); else lstrcpyA(text, tr(T_NA));
    if(valid) label(dc,r,text,DT_RIGHT);else language_draw(dc,&r,T_NA,DT_RIGHT);
}
static void plot(HDC dc, RECT r, const DWORD *values, BOOL meter, BOOL valid, DWORD current)
{
    HPEN grid, line;
    HGDIOBJ old;
    char text[32];
    int x,y,w,h,bars;
    unsigned int i;
    DrawEdge(dc, &r, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
    FillRect(dc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
    w=r.right-r.left-1; h=r.bottom-r.top-1;
    if (w<1 || h<1) return;
    grid=CreatePen(PS_SOLID,1,RGB(0,80,40));
    line=CreatePen(PS_SOLID,1,RGB(0,255,0));
    old=SelectObject(dc,grid);
    if (!meter) {
        for(x=r.left; x<r.right; x+=20) { MoveToEx(dc,x,r.top,NULL); LineTo(dc,x,r.bottom); }
        for(y=r.top; y<r.bottom; y+=20) { MoveToEx(dc,r.left,y,NULL); LineTo(dc,r.right,y); }
        SelectObject(dc,line);
        for(i=0;i<samples;++i) {
            if(values[i]>100) continue;
            x=r.left+(int)(HISTORY-samples+i)*w/(HISTORY-1);
            y=r.bottom-1-(int)values[i]*h/100;
            if(i==0 || values[i-1]>100) MoveToEx(dc,x,y,NULL); else LineTo(dc,x,y);
        }
    } else {
        HBRUSH lit=CreateSolidBrush(RGB(0,255,0)), dark=CreateSolidBrush(RGB(0,60,0));
        RECT bar;
        bars=(h-24)/4;
        if(bars<1) bars=1;
        if(bars>20) bars=20;
        for(y=0;y<bars;++y) {
            bar.left=r.left+w/4; bar.right=r.right-w/4;
            bar.top=r.top+4+y*4; bar.bottom=bar.top+2;
            FillRect(dc,&bar,valid && (bars-y)*100/bars<=(int)current ? lit : dark);
        }
        DeleteObject(lit); DeleteObject(dark);
    }
    SelectObject(dc,old); DeleteObject(grid); DeleteObject(line);
    if(meter || !valid) {
        SetTextColor(dc,RGB(0,255,0));
        if(valid) wsprintfA(text,"%lu%%",current); else lstrcpyA(text,tr(T_NA));
        if(meter) r.top=r.bottom-21;
        if(valid) label(dc,r,text,DT_CENTER | DT_VCENTER);
        else language_draw(dc,&r,T_NA,DT_CENTER | DT_VCENTER);
        SetTextColor(dc,GetSysColor(COLOR_BTNTEXT));
    }
}
static void paint(HDC dc, RECT client)
{
    RECT r,line;
    HGDIOBJ old_font;
    int width=client.right, height=client.bottom;
    int top_height=(height-180)/2, meter_width=128, half=(width-28)/2;
    if(top_height<60) top_height=60;
    FillRect(dc,&client,GetSysColorBrush(COLOR_BTNFACE));
    old_font=SelectObject(dc,font);
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,GetSysColor(COLOR_BTNTEXT));
    r=group(dc,8,4,meter_width,top_height,T_CPU);
    plot(dc,r,cpu_history,TRUE,data.cpu_valid,data.cpu_percent);
    r=group(dc,meter_width+16,4,width-meter_width-24,top_height,T_CPUHISTORY);
    plot(dc,r,cpu_history,FALSE,data.cpu_valid,data.cpu_percent);
    r=group(dc,8,top_height+12,meter_width,top_height,T_MEMLOAD);
    plot(dc,r,memory_history,TRUE,TRUE,data.memory.dwMemoryLoad);
    r=group(dc,meter_width+16,top_height+12,width-meter_width-24,top_height,T_MEMHISTORY);
    plot(dc,r,memory_history,FALSE,TRUE,data.memory.dwMemoryLoad);
    r=group(dc,8,2*top_height+20,half,76,T_TOTALS);
    line=r; line.bottom=line.top+16;
    number(dc,line,T_PROCESSES,data.processes,data.processes_valid); line.top+=16;line.bottom+=16;
    number(dc,line,T_THREADS,data.threads,data.processes_valid); line.top+=16;line.bottom+=16;
    number(dc,line,T_UPTIME,data.vxd.uptime_ms/1000,data.vxd_valid);
    r=group(dc,half+20,2*top_height+20,half,76,T_PHYSICAL);
    line=r;line.bottom=line.top+16;
    number(dc,line,T_TOTAL,data.memory.dwTotalPhys/1024,TRUE);line.top+=16;line.bottom+=16;
    number(dc,line,T_AVAILABLE,data.memory.dwAvailPhys/1024,TRUE);line.top+=16;line.bottom+=16;
    number(dc,line,T_VMMPAGES,data.vxd.free_pages,data.vxd_valid);
    r=group(dc,8,2*top_height+104,half,66,T_PAGING);
    line=r;line.bottom=line.top+16;
    number(dc,line,T_TOTAL,data.memory.dwTotalPageFile/1024,TRUE);line.top+=16;line.bottom+=16;
    number(dc,line,T_AVAILABLE,data.memory.dwAvailPageFile/1024,TRUE);
    r=group(dc,half+20,2*top_height+104,half,66,T_VIRTUAL);
    line=r;line.bottom=line.top+16;
    number(dc,line,T_TOTAL,data.memory.dwTotalVirtual/1024,TRUE);line.top+=16;line.bottom+=16;
    number(dc,line,T_AVAILABLE,data.memory.dwAvailVirtual/1024,TRUE);
    SelectObject(dc,old_font);
}
static LRESULT CALLBACK performance_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    if(msg==WM_SIZE) { scroll_layout(w);return 0; }
    if(msg==WM_HSCROLL || msg==WM_VSCROLL) {
        scroll_content(w,msg==WM_HSCROLL?SB_HORZ:SB_VERT,wp);return 0;
    }
    if(msg==WM_ERASEBKGND) return 1;
    if(msg==WM_PAINT) {
        PAINTSTRUCT ps;
        RECT r,canvas;
        POINT origin;
        HDC dc=BeginPaint(w,&ps), buffer;
        HBITMAP bitmap;
        HGDIOBJ old;
        GetClientRect(w,&r);
        canvas=r;
        if(canvas.right<MIN_CONTENT_WIDTH) canvas.right=MIN_CONTENT_WIDTH;
        if(canvas.bottom<MIN_CONTENT_HEIGHT) canvas.bottom=MIN_CONTENT_HEIGHT;
        buffer=CreateCompatibleDC(dc);
        bitmap=CreateCompatibleBitmap(dc,r.right>0?r.right:1,r.bottom>0?r.bottom:1);
        if(buffer && bitmap) {
            old=SelectObject(buffer,bitmap);
            SetViewportOrgEx(buffer,-scroll_x,-scroll_y,NULL);
            paint(buffer,canvas);
            SetViewportOrgEx(buffer,0,0,NULL);
            BitBlt(dc,0,0,r.right,r.bottom,buffer,0,0,SRCCOPY);
            SelectObject(buffer,old);
        } else {
            SetViewportOrgEx(dc,-scroll_x,-scroll_y,&origin);
            paint(dc,canvas);
            SetViewportOrgEx(dc,origin.x,origin.y,NULL);
        }
        if(bitmap) DeleteObject(bitmap);
        if(buffer) DeleteDC(buffer);
        EndPaint(w,&ps); return 0;
    }
    return DefWindowProcA(w,msg,wp,lp);
}
BOOL performance_register(HINSTANCE instance)
{
    WNDCLASSA wc;
    ZeroMemory(&wc,sizeof(wc));
    wc.hInstance=instance; wc.lpfnWndProc=performance_proc;
    wc.lpszClassName=TM95_PERF_CLASS; wc.hCursor=LoadCursorA(NULL,IDC_ARROW);
    font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
    return RegisterClassA(&wc)!=0;
}
void performance_update(HWND window,const TM_SYSTEM *sys,BOOL sample)
{
    unsigned int i;
    data=*sys;
    if(sample) {
        if(samples==HISTORY) {
            for(i=1;i<HISTORY;++i) { memory_history[i-1]=memory_history[i];cpu_history[i-1]=cpu_history[i]; }
            --samples;
        }
        memory_history[samples]=sys->memory.dwMemoryLoad;
        cpu_history[samples]=sys->cpu_valid?sys->cpu_percent:0xffffffffUL;
        ++samples;
    }
    InvalidateRect(window,NULL,FALSE);
}
