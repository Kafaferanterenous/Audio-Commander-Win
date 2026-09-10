#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "metadata_loader.h"

#define TEST_MESSAGE (WM_APP + 40)

typedef struct ReaderState {
    volatile LONG calls;
    DWORD delay_ms;
} ReaderState;

static unsigned long fake_duration(const wchar_t *path, void *context)
{
    ReaderState *state = (ReaderState *)context;
    (void)path;
    Sleep(state->delay_ms);
    InterlockedIncrement(&state->calls);
    return 1234;
}

int wmain(void)
{
    enum { ITEM_COUNT = 100 };
    HWND window;
    MetadataLoader *loader;
    BrowserEntry *entries;
    BrowserListing listing;
    ReaderState reader = {0, 10};
    ULONGLONG submitted;
    ULONGLONG deadline;
    size_t completed_updates = 0;
    BOOL complete = FALSE;
    MSG message;
    size_t index;

    window = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0,
                             HWND_MESSAGE, NULL, GetModuleHandleW(NULL), NULL);
    if (window == NULL) return 1;
    loader = metadata_loader_create(window, TEST_MESSAGE, 2, fake_duration, &reader);
    if (loader == NULL) return 2;
    entries = (BrowserEntry *)calloc(ITEM_COUNT, sizeof(*entries));
    if (entries == NULL) return 3;
    for (index = 0; index < ITEM_COUNT; ++index) {
        entries[index].name = L"synthetic.mp3";
        entries[index].kind = BROWSER_ENTRY_AUDIO_FILE;
        entries[index].entry_id = index + 1;
    }
    listing.entries = entries;
    listing.count = ITEM_COUNT;
    submitted = GetTickCount64();
    if (!metadata_loader_submit(loader, 0, 1, L"C:\\synthetic", &listing)) return 4;
    if (GetTickCount64() - submitted > 100) return 5;

    deadline = GetTickCount64() + 5000;
    while (!complete && GetTickCount64() < deadline) {
        while (PeekMessageW(&message, window, TEST_MESSAGE, TEST_MESSAGE, PM_REMOVE)) {
            MetadataBatch *batch = (MetadataBatch *)message.lParam;
            if (batch == NULL || batch->pane_index != 0 || batch->generation != 1) return 6;
            completed_updates += batch->count;
            complete = batch->complete;
            metadata_batch_free(batch);
        }
        Sleep(1);
    }
    if (!complete || completed_updates != ITEM_COUNT || reader.calls != ITEM_COUNT) return 7;

    reader.delay_ms = 25;
    reader.calls = 0;
    if (!metadata_loader_submit(loader, 0, 2, L"C:\\synthetic", &listing)) return 8;
    Sleep(40);
    if (!metadata_loader_submit(loader, 0, 3, L"C:\\synthetic", &listing)) return 9;
    complete = FALSE;
    deadline = GetTickCount64() + 5000;
    while (!complete && GetTickCount64() < deadline) {
        while (PeekMessageW(&message, window, TEST_MESSAGE, TEST_MESSAGE, PM_REMOVE)) {
            MetadataBatch *batch = (MetadataBatch *)message.lParam;
            if (batch != NULL && batch->generation == 3 && batch->complete) complete = TRUE;
            metadata_batch_free(batch);
        }
        Sleep(1);
    }
    if (!complete) return 10;
    if (!metadata_loader_shutdown(loader, 2000)) return 11;
    DestroyWindow(window);
    free(entries);
    wprintf(L"asynchronous metadata loading and stale-request cancellation passed\n");
    return 0;
}

unsigned long media_duration_ms(const wchar_t *path)
{
    (void)path;
    return 0;
}
