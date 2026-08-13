#include "copy_progress.h"

#include "file_ops.h"

#include <commctrl.h>
#include <stdlib.h>
#include <wchar.h>

#define COPY_PROGRESS_CLASS L"AudioCommanderCopyProgressV92"
#define WM_COPY_PROGRESS_UPDATE (WM_APP + 77)
#define COPY_PROGRESS_WIDTH 520
#define COPY_PROGRESS_HEIGHT 174
#define ID_COPY_FILE 1
#define ID_COPY_MESSAGE 2
#define ID_COPY_BAR 3
#define ID_COPY_BYTES 4

typedef struct CopyProgressUpdate {
    ULONGLONG total_bytes;
    ULONGLONG transferred_bytes;
} CopyProgressUpdate;

typedef struct CopyProgressState {
    HWND window;
    HWND file_label;
    HWND message_label;
    HWND progress_bar;
    HWND bytes_label;
    const wchar_t *source;
    const wchar_t *destination;
    const wchar_t *display_name;
    const CopyProgressStrings *strings;
    bool overwrite;
    bool success;
    DWORD error_code;
    DWORD last_post_tick;
    int item_number;
    int item_count;
} CopyProgressState;

static ATOM copy_progress_class;

bool copy_progress_should_show(ULONGLONG elapsed_ms, bool completed)
{
    return !completed && elapsed_ms >= COPY_PROGRESS_REVEAL_DELAY_MS;
}

static void update_progress_controls(CopyProgressState *state,
                                     ULONGLONG total_bytes,
                                     ULONGLONG transferred_bytes)
{
    wchar_t text[160];
    unsigned int percent = total_bytes == 0 ? 0 :
        (unsigned int)((double)transferred_bytes * 100.0 / (double)total_bytes);
    int position = total_bytes == 0 ? 0 :
        (int)((double)transferred_bytes * 1000.0 / (double)total_bytes);
    if (percent > 100) percent = 100;
    if (position > 1000) position = 1000;
    SendMessageW(state->progress_bar, PBM_SETPOS, (WPARAM)position, 0);
    swprintf_s(text, ARRAYSIZE(text), state->strings->bytes_format,
               (double)transferred_bytes / 1000000.0,
               (double)total_bytes / 1000000.0, percent);
    SetWindowTextW(state->bytes_label, text);
}

static LRESULT CALLBACK copy_progress_window_proc(HWND window, UINT message,
                                                  WPARAM w_param, LPARAM l_param)
{
    CopyProgressState *state =
        (CopyProgressState *)GetWindowLongPtrW(window, GWLP_USERDATA);
    (void)w_param;
    if (message == WM_NCCREATE) {
        CREATESTRUCTW *create = (CREATESTRUCTW *)l_param;
        state = (CopyProgressState *)create->lpCreateParams;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)state);
        return TRUE;
    }
    if (message == WM_COPY_PROGRESS_UPDATE) {
        CopyProgressUpdate *update = (CopyProgressUpdate *)l_param;
        if (state != NULL && update != NULL)
            update_progress_controls(state, update->total_bytes,
                                     update->transferred_bytes);
        free(update);
        return 0;
    }
    if (message == WM_CLOSE) return 0;
    return DefWindowProcW(window, message, w_param, l_param);
}

static bool register_copy_progress_class(void)
{
    WNDCLASSEXW window_class;
    if (copy_progress_class != 0) return true;
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = copy_progress_window_proc;
    window_class.hInstance = GetModuleHandleW(NULL);
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszClassName = COPY_PROGRESS_CLASS;
    copy_progress_class = RegisterClassExW(&window_class);
    return copy_progress_class != 0;
}

static void position_copy_window(HWND window, HWND owner)
{
    RECT owner_rect;
    RECT window_rect = {0, 0, COPY_PROGRESS_WIDTH, COPY_PROGRESS_HEIGHT};
    int x;
    int y;
    AdjustWindowRectEx(&window_rect, WS_POPUP | WS_CAPTION | WS_BORDER,
                       FALSE, WS_EX_DLGMODALFRAME);
    if (owner == NULL || !GetWindowRect(owner, &owner_rect)) {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &owner_rect, 0);
    }
    x = owner_rect.left +
        ((owner_rect.right - owner_rect.left) - (window_rect.right - window_rect.left)) / 2;
    y = owner_rect.top +
        ((owner_rect.bottom - owner_rect.top) - (window_rect.bottom - window_rect.top)) / 2;
    SetWindowPos(window, HWND_TOP, x, y,
                 window_rect.right - window_rect.left,
                 window_rect.bottom - window_rect.top,
                 SWP_NOACTIVATE);
}

static bool create_copy_window(CopyProgressState *state, HWND owner)
{
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    wchar_t file_text[512];
    if (!register_copy_progress_class()) return false;
    state->window = CreateWindowExW(
        WS_EX_DLGMODALFRAME, COPY_PROGRESS_CLASS, state->strings->title,
        WS_POPUP | WS_CAPTION | WS_BORDER, 0, 0, COPY_PROGRESS_WIDTH,
        COPY_PROGRESS_HEIGHT, owner, NULL, GetModuleHandleW(NULL), state);
    if (state->window == NULL) return false;
    swprintf_s(file_text, ARRAYSIZE(file_text), state->strings->file_format,
               state->item_number, state->item_count, state->display_name);
    state->file_label = CreateWindowExW(
        0, L"STATIC", file_text, WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
        18, 16, 484, 22, state->window, (HMENU)(INT_PTR)ID_COPY_FILE, NULL, NULL);
    state->message_label = CreateWindowExW(
        0, L"STATIC", state->strings->patient_message, WS_CHILD | WS_VISIBLE,
        18, 43, 484, 40, state->window, (HMENU)(INT_PTR)ID_COPY_MESSAGE, NULL, NULL);
    state->progress_bar = CreateWindowExW(
        0, PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        18, 91, 484, 22, state->window, (HMENU)(INT_PTR)ID_COPY_BAR, NULL, NULL);
    state->bytes_label = CreateWindowExW(
        0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
        18, 123, 484, 24, state->window, (HMENU)(INT_PTR)ID_COPY_BYTES, NULL, NULL);
    if (state->file_label == NULL || state->message_label == NULL ||
        state->progress_bar == NULL || state->bytes_label == NULL) {
        DestroyWindow(state->window);
        state->window = NULL;
        return false;
    }
    SendMessageW(state->file_label, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(state->message_label, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(state->bytes_label, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(state->progress_bar, PBM_SETRANGE32, 0, 1000);
    update_progress_controls(state, 0, 0);
    position_copy_window(state->window, owner);
    return true;
}

static void post_copy_progress(ULONGLONG total_bytes,
                               ULONGLONG transferred_bytes, void *context)
{
    CopyProgressState *state = (CopyProgressState *)context;
    DWORD now = GetTickCount();
    CopyProgressUpdate *update;
    if (transferred_bytes != total_bytes && now - state->last_post_tick < 100)
        return;
    state->last_post_tick = now;
    update = (CopyProgressUpdate *)malloc(sizeof(*update));
    if (update == NULL) return;
    update->total_bytes = total_bytes;
    update->transferred_bytes = transferred_bytes;
    if (!PostMessageW(state->window, WM_COPY_PROGRESS_UPDATE, 0, (LPARAM)update))
        free(update);
}

static DWORD WINAPI copy_worker(void *context)
{
    CopyProgressState *state = (CopyProgressState *)context;
    state->success = file_ops_copy_with_progress(
        state->source, state->destination, state->overwrite,
        post_copy_progress, state);
    state->error_code = state->success ? ERROR_SUCCESS : GetLastError();
    return state->success ? 0 : 1;
}

bool copy_progress_copy(HWND owner, const wchar_t *source,
                        const wchar_t *destination, bool overwrite,
                        const wchar_t *display_name, int item_number,
                        int item_count, const CopyProgressStrings *strings)
{
    CopyProgressState state;
    HANDLE thread;
    ULONGLONG started;
    bool shown = false;
    bool owner_was_enabled;
    bool saw_quit = false;
    int quit_code = 0;
    MSG message;
    DWORD wait_result;
    WIN32_FILE_ATTRIBUTE_DATA source_data;
    ULONGLONG source_size = 0;
    if (owner == NULL || source == NULL || destination == NULL ||
        display_name == NULL || strings == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    ZeroMemory(&state, sizeof(state));
    state.source = source;
    state.destination = destination;
    state.display_name = display_name;
    state.strings = strings;
    state.overwrite = overwrite;
    state.item_number = item_number;
    state.item_count = item_count;
    if (!create_copy_window(&state, owner)) return false;
    if (GetFileAttributesExW(source, GetFileExInfoStandard, &source_data)) {
        source_size = ((ULONGLONG)source_data.nFileSizeHigh << 32) |
                      source_data.nFileSizeLow;
        update_progress_controls(&state, source_size, 0);
    }

    owner_was_enabled = IsWindowEnabled(owner) != FALSE;
    if (owner_was_enabled) EnableWindow(owner, FALSE);
    thread = CreateThread(NULL, 0, copy_worker, &state, 0, NULL);
    if (thread == NULL) {
        DWORD error_code = GetLastError();
        if (owner_was_enabled) EnableWindow(owner, TRUE);
        DestroyWindow(state.window);
        SetLastError(error_code);
        return false;
    }

    started = GetTickCount64();
    for (;;) {
        wait_result = MsgWaitForMultipleObjects(1, &thread, FALSE, 50, QS_ALLINPUT);
        if (wait_result == WAIT_OBJECT_0) break;
        if (wait_result == WAIT_FAILED) {
            state.error_code = GetLastError();
            state.success = false;
            break;
        }
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                saw_quit = true;
                quit_code = (int)message.wParam;
                continue;
            }
            if (!IsDialogMessageW(state.window, &message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        if (!shown && copy_progress_should_show(GetTickCount64() - started, false)) {
            ShowWindow(state.window, SW_SHOWNORMAL);
            UpdateWindow(state.window);
            shown = true;
        }
    }
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    while (PeekMessageW(&message, state.window, WM_COPY_PROGRESS_UPDATE,
                        WM_COPY_PROGRESS_UPDATE, PM_REMOVE)) {
        DispatchMessageW(&message);
    }
    DestroyWindow(state.window);
    if (owner_was_enabled) {
        EnableWindow(owner, TRUE);
        SetActiveWindow(owner);
    }
    if (saw_quit) PostQuitMessage(quit_code);
    SetLastError(state.success ? ERROR_SUCCESS : state.error_code);
    return state.success;
}
