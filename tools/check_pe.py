#!/usr/bin/env python3
"""Reject accidental CRT/new-OS imports and incorrect PE target metadata.

This is a build guard, not a substitute for testing on Windows 95.
Tool Help exports are resolved dynamically and cannot be validated here.
"""
import argparse
import re
import struct
import subprocess
from pathlib import Path

# Deliberately narrow set of baseline APIs used by this program.
ALLOWED = {
    'KERNEL32.dll': '''CloseHandle CreateProcessA ExitProcess GetCurrentProcessId
        GetFullPathNameA GetShortPathNameA GetPrivateProfileIntA GetProcessHeap HeapAlloc HeapReAlloc HeapFree GetTickCount
        GetLastError GetModuleHandleA GetModuleFileNameA GetCommandLineA GetProcAddress GlobalMemoryStatus
        OpenProcess SetPriorityClass TerminateProcess lstrcpyA lstrcatA lstrlenA
        GetVersion GetACP WideCharToMultiByte MultiByteToWideChar GetPrivateProfileStringA
        WritePrivateProfileStringA LoadLibraryA FreeLibrary SetLastError
        FindResourceA SizeofResource LoadResource LockResource GetTempPathA
        GetTempFileNameA CreateFileA WriteFile DeleteFileA MoveFileA DeviceIoControl'''.split(),
    'USER32.dll': '''AppendMenuA BeginPaint CheckMenuItem CreateMenu CreatePopupMenu
        CreateWindowExA DefWindowProcA DestroyWindow DialogBoxParamA DispatchMessageA
        EnableWindow EndDialog EndPaint EnumWindows FillRect GetClientRect
        GetDlgItemTextA GetMessageA GetMenu GetWindow GetWindowTextA FindWindowA
        GetWindowThreadProcessId InvalidateRect IsDialogMessageA IsIconic
        IsWindow IsWindowVisible KillTimer LoadCursorA LoadIconA MessageBoxA
        MoveWindow PostMessageA PostQuitMessage RegisterClassA SendDlgItemMessageA
        SendMessageA SetForegroundWindow SetTimer SetWindowPos SetWindowTextA
        ShowWindow ShowWindowAsync TranslateMessage UpdateWindow wsprintfA
        DestroyIcon CheckDlgButton GetDlgItemInt IsDlgButtonChecked SetFocus GetClassNameA
        GetClassLongA GetWindowLongA SetWindowLongA GetSystemMetrics IsZoomed SendMessageTimeoutA EnableMenuItem
        DrawTextA DrawEdge SetScrollInfo GetScrollInfo GetScrollPos GetSysColor GetSysColorBrush MapWindowPoints InflateRect
        GetWindowRect SetMenu DestroyMenu DrawMenuBar GetDlgItem SetDlgItemTextA'''.split(),
    'GDI32.dll': '''CreatePen DeleteObject GetStockObject LineTo MoveToEx SelectObject
        SetBkMode SetTextColor TextOutA BitBlt CreateCompatibleDC
        CreateCompatibleBitmap DeleteDC CreateSolidBrush SetViewportOrgEx'''.split(),
    'COMCTL32.DLL': ['InitCommonControls', 'ImageList_Create', 'ImageList_Destroy', 'ImageList_GetImageCount', 'ImageList_ReplaceIcon'],
    'SHELL32.dll': ['ShellAboutA', 'ExtractIconExA'],
    'ADVAPI32.dll': ['RegOpenKeyExA', 'RegQueryValueExA', 'RegCloseKey', 'RegCreateKeyExA', 'RegSetValueExA', 'RegDeleteValueA'],
}

def check(path, objdump):
    data = Path(path).read_bytes()
    assert data[:2] == b'MZ', 'Missing DOS header'
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[pe:pe+4] == b'PE\0\0', 'Not a PE file'
    assert struct.unpack_from('<H', data, pe+4)[0] == 0x14c, 'Not x86'
    opt = pe+24
    assert struct.unpack_from('<H', data, opt)[0] == 0x10b, 'Not PE32'
    assert struct.unpack_from('<HH', data, opt+40) == (4, 0), 'OS version is not 4.0'
    assert struct.unpack_from('<HH', data, opt+48) == (4, 0), 'Subsystem version is not 4.0'
    assert struct.unpack_from('<H', data, opt+68)[0] == 2, 'Not Windows GUI'
    output = subprocess.check_output([objdump, '-p', str(path)], text=True)
    allow = {dll.lower(): set(names) for dll, names in ALLOWED.items()}
    imports = {}
    current = None
    for line in output.splitlines():
        if 'DLL Name:' in line:
            current = line.split('DLL Name:', 1)[1].strip().lower()
            assert current in allow, f'Unexpected DLL: {current}'
            imports[current] = []
        elif current:
            match = re.match(r'\s+[0-9a-f]+\s+(?:<none>\s+)?[0-9a-f]+\s+(\S+)\s*$', line)
            if match:
                name = match[1]
                assert name in allow[current], f'Unreviewed import: {current}!{name}'
                imports[current].append(name)
    assert imports and all(imports.values()), 'Could not read named imports'
    assert set(imports) == set(allow), 'Expected system DLLs missing'
    print(f'PASS: x86 PE32, OS/subsystem 4.0, {len(data)} bytes')
    print(f'PASS: {sum(map(len, imports.values()))} reviewed imports; no CRT DLL')
    print('Windows 95 execution and dynamic Tool Help behavior: NOT TESTED here')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--objdump', default='i686-w64-mingw32-objdump')
    parser.add_argument('exe')
    args = parser.parse_args()
    check(args.exe, args.objdump)
