#include "copy_progress.h"

#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <wchar.h>

typedef struct ObserverState {
    HWND owner;
    volatile LONG stop;
    volatile LONG saw_window;
    volatile LONG saw_patient_message;
    volatile LONG owner_responded;
} ObserverState;

static DWORD WINAPI observe_copy_window(void *context)
{
    ObserverState *state = (ObserverState *)context;
    while (InterlockedCompareExchange(&state->stop, 0, 0) == 0) {
        HWND progress = FindWindowW(L"AudioCommanderCopyProgressV92", NULL);
        if (progress != NULL) {
            HWND message = GetDlgItem(progress, 2);
            wchar_t text[256];
            DWORD_PTR ignored;
            InterlockedExchange(&state->saw_window, 1);
            if (message != NULL && GetWindowTextW(message, text, ARRAYSIZE(text)) > 0 &&
                wcsstr(text, L"Please be patient") != NULL)
                InterlockedExchange(&state->saw_patient_message, 1);
            if (SendMessageTimeoutW(state->owner, WM_NULL, 0, 0,
                                    SMTO_ABORTIFHUNG, 500, &ignored) != 0)
                InterlockedExchange(&state->owner_responded, 1);
        }
        Sleep(10);
    }
    return 0;
}

int wmain(int argc, wchar_t **argv)
{
    static const CopyProgressStrings strings = {
        L"Copy progress",
        L"Copying to this drive is taking longer than usual. Please be patient; the computer is still working.",
        L"Copying file %d of %d: %ls",
        L"%.1f MB of %.1f MB (%u%%)"
    };
    ObserverState observer = {0};
    HWND owner;
    HANDLE observer_thread;
    ULONGLONG started;
    ULONGLONG elapsed;
    bool success;
    DWORD copy_error;
    if (argc != 3) {
        fwprintf(stderr, L"usage: usb_copy_live_smoke SOURCE DESTINATION\n");
        return 2;
    }
    if (GetFileAttributesW(argv[2]) != INVALID_FILE_ATTRIBUTES) {
        fwprintf(stderr, L"refusing to overwrite existing destination: %ls\n", argv[2]);
        return 3;
    }
    owner = CreateWindowExW(0, L"STATIC", L"AudioCommander v9.2 USB copy test",
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                            CW_USEDEFAULT, CW_USEDEFAULT, 560, 230,
                            NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (owner == NULL) return 4;
    ShowWindow(owner, SW_SHOWNORMAL);
    UpdateWindow(owner);
    observer.owner = owner;
    observer_thread = CreateThread(NULL, 0, observe_copy_window, &observer, 0, NULL);
    if (observer_thread == NULL) {
        DestroyWindow(owner);
        return 5;
    }
    started = GetTickCount64();
    success = copy_progress_copy(owner, argv[1], argv[2], false,
                                 L"AudioCommander_v92_copy_test_20260810_130452.bin",
                                 1, 1, &strings);
    elapsed = GetTickCount64() - started;
    copy_error = GetLastError();
    InterlockedExchange(&observer.stop, 1);
    WaitForSingleObject(observer_thread, 2000);
    CloseHandle(observer_thread);
    DestroyWindow(owner);
    wprintf(L"success=%d elapsed_ms=%llu progress_window=%ld patient_message=%ld owner_responsive=%ld error=%lu\n",
            success ? 1 : 0, elapsed, observer.saw_window,
            observer.saw_patient_message, observer.owner_responded, copy_error);
    if (!success) return 6;
    if (observer.saw_window == 0) return 7;
    if (observer.saw_patient_message == 0) return 8;
    if (observer.owner_responded == 0) return 9;
    return 0;
}
