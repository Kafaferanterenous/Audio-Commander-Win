#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int wmain(int argc, wchar_t **argv)
{
    HWND window;
    UINT command;
    if (argc != 3) return 2;
    window = (HWND)(UINT_PTR)_wcstoui64(argv[1], NULL, 10);
    command = (UINT)wcstoul(argv[2], NULL, 10);
    if (!IsWindow(window)) return 3;
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(command, BN_CLICKED), 0);
    return 0;
}
