#include "metadata_loader.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

#include "media_info.h"

typedef struct MetadataRequestItem {
    unsigned long long entry_id;
    wchar_t *path;
} MetadataRequestItem;

typedef struct MetadataRequest {
    size_t pane_index;
    LONG generation;
    size_t count;
    MetadataRequestItem *items;
} MetadataRequest;

struct MetadataLoader {
    HWND notify_window;
    UINT notify_message;
    size_t pane_count;
    MetadataDurationReader reader;
    void *reader_context;
    HANDLE stop_event;
    HANDLE wake_event;
    HANDLE thread;
    CRITICAL_SECTION lock;
    MetadataRequest **pending;
    volatile LONG *latest_generation;
    size_t next_pane;
};

static unsigned long default_duration_reader(const wchar_t *path, void *context)
{
    (void)context;
    return media_duration_ms(path);
}

static void request_free(MetadataRequest *request)
{
    size_t index;
    if (request == NULL) return;
    for (index = 0; index < request->count; ++index) free(request->items[index].path);
    free(request->items);
    free(request);
}

void metadata_batch_free(MetadataBatch *batch)
{
    free(batch);
}

static wchar_t *joined_path(const wchar_t *folder, const wchar_t *name)
{
    size_t folder_length;
    size_t name_length;
    size_t total;
    BOOL separator;
    wchar_t *path;
    if (folder == NULL || name == NULL || folder[0] == L'\0' || name[0] == L'\0') return NULL;
    folder_length = wcslen(folder);
    name_length = wcslen(name);
    separator = folder[folder_length - 1] != L'\\' && folder[folder_length - 1] != L'/';
    if (folder_length > 32767 || name_length > 32767 ||
        folder_length + name_length + (separator ? 2 : 1) > 32768) return NULL;
    total = folder_length + name_length + (separator ? 2 : 1);
    path = (wchar_t *)malloc(total * sizeof(*path));
    if (path == NULL) return NULL;
    memcpy(path, folder, folder_length * sizeof(*path));
    if (separator) path[folder_length++] = L'\\';
    memcpy(path + folder_length, name, (name_length + 1) * sizeof(*path));
    return path;
}

static MetadataRequest *request_create(size_t pane_index, LONG generation,
                                       const wchar_t *folder,
                                       const BrowserListing *listing)
{
    MetadataRequest *request;
    size_t audio_count = 0;
    size_t source_index;
    size_t target_index = 0;
    if (folder == NULL || listing == NULL || generation <= 0) return NULL;
    for (source_index = 0; source_index < listing->count; ++source_index) {
        if (listing->entries[source_index].kind == BROWSER_ENTRY_AUDIO_FILE) ++audio_count;
    }
    request = (MetadataRequest *)calloc(1, sizeof(*request));
    if (request == NULL) return NULL;
    request->pane_index = pane_index;
    request->generation = generation;
    request->count = audio_count;
    if (audio_count == 0) return request;
    if (audio_count > SIZE_MAX / sizeof(*request->items)) {
        request_free(request);
        return NULL;
    }
    request->items = (MetadataRequestItem *)calloc(audio_count, sizeof(*request->items));
    if (request->items == NULL) {
        request_free(request);
        return NULL;
    }
    for (source_index = 0; source_index < listing->count; ++source_index) {
        const BrowserEntry *entry = &listing->entries[source_index];
        if (entry->kind != BROWSER_ENTRY_AUDIO_FILE) continue;
        request->items[target_index].entry_id = entry->entry_id;
        request->items[target_index].path = joined_path(folder, entry->name);
        if (request->items[target_index].path == NULL) {
            request->count = target_index;
            request_free(request);
            return NULL;
        }
        ++target_index;
    }
    return request;
}

static BOOL request_is_current(const MetadataLoader *loader, const MetadataRequest *request)
{
    return WaitForSingleObject(loader->stop_event, 0) != WAIT_OBJECT_0 &&
           InterlockedCompareExchange(
               (volatile LONG *)&loader->latest_generation[request->pane_index], 0, 0) ==
               request->generation;
}

static BOOL post_batch(MetadataLoader *loader, MetadataBatch **batch)
{
    MetadataBatch *posted = *batch;
    if (posted == NULL) return FALSE;
    *batch = NULL;
    if (!PostMessageW(loader->notify_window, loader->notify_message, 0, (LPARAM)posted)) {
        metadata_batch_free(posted);
        return FALSE;
    }
    return TRUE;
}

static void process_request(MetadataLoader *loader, MetadataRequest *request)
{
    MetadataBatch *batch = NULL;
    size_t index;
    for (index = 0; index < request->count; ++index) {
        unsigned long duration;
        if (!request_is_current(loader, request)) break;
        duration = loader->reader(request->items[index].path, loader->reader_context);
        if (!request_is_current(loader, request)) break;
        if (batch == NULL) {
            batch = (MetadataBatch *)calloc(1, sizeof(*batch));
            if (batch == NULL) break;
            batch->pane_index = request->pane_index;
            batch->generation = request->generation;
        }
        batch->updates[batch->count].entry_id = request->items[index].entry_id;
        batch->updates[batch->count].duration_ms = duration;
        ++batch->count;
        if (batch->count == METADATA_BATCH_CAPACITY && !post_batch(loader, &batch)) break;
    }
    if (request_is_current(loader, request)) {
        if (batch == NULL) {
            batch = (MetadataBatch *)calloc(1, sizeof(*batch));
            if (batch != NULL) {
                batch->pane_index = request->pane_index;
                batch->generation = request->generation;
            }
        }
        if (batch != NULL) {
            batch->complete = TRUE;
            (void)post_batch(loader, &batch);
        }
    }
    metadata_batch_free(batch);
}

static MetadataRequest *take_next_request(MetadataLoader *loader)
{
    MetadataRequest *request = NULL;
    size_t offset;
    EnterCriticalSection(&loader->lock);
    for (offset = 0; offset < loader->pane_count; ++offset) {
        size_t pane = (loader->next_pane + offset) % loader->pane_count;
        if (loader->pending[pane] != NULL) {
            request = loader->pending[pane];
            loader->pending[pane] = NULL;
            loader->next_pane = (pane + 1) % loader->pane_count;
            break;
        }
    }
    LeaveCriticalSection(&loader->lock);
    return request;
}

static DWORD WINAPI worker_main(void *parameter)
{
    MetadataLoader *loader = (MetadataLoader *)parameter;
    HANDLE events[2] = {loader->stop_event, loader->wake_event};
    for (;;) {
        DWORD wait = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0) return 0;
        if (wait != WAIT_OBJECT_0 + 1) return 1;
        for (;;) {
            MetadataRequest *request = take_next_request(loader);
            if (request == NULL) break;
            process_request(loader, request);
            request_free(request);
            if (WaitForSingleObject(loader->stop_event, 0) == WAIT_OBJECT_0) return 0;
        }
    }
}

MetadataLoader *metadata_loader_create(HWND notify_window, UINT notify_message,
                                       size_t pane_count,
                                       MetadataDurationReader reader,
                                       void *reader_context)
{
    MetadataLoader *loader;
    if (notify_window == NULL || notify_message < WM_APP || pane_count == 0 || pane_count > 16)
        return NULL;
    loader = (MetadataLoader *)calloc(1, sizeof(*loader));
    if (loader == NULL) return NULL;
    loader->notify_window = notify_window;
    loader->notify_message = notify_message;
    loader->pane_count = pane_count;
    loader->reader = reader != NULL ? reader : default_duration_reader;
    loader->reader_context = reader_context;
    loader->pending = (MetadataRequest **)calloc(pane_count, sizeof(*loader->pending));
    loader->latest_generation = (volatile LONG *)calloc(pane_count, sizeof(*loader->latest_generation));
    loader->stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    loader->wake_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (loader->pending == NULL || loader->latest_generation == NULL ||
        loader->stop_event == NULL || loader->wake_event == NULL) goto fail;
    InitializeCriticalSection(&loader->lock);
    loader->thread = CreateThread(NULL, 0, worker_main, loader, 0, NULL);
    if (loader->thread == NULL) {
        DeleteCriticalSection(&loader->lock);
        goto fail;
    }
    return loader;

fail:
    if (loader->stop_event != NULL) CloseHandle(loader->stop_event);
    if (loader->wake_event != NULL) CloseHandle(loader->wake_event);
    free((void *)loader->latest_generation);
    free(loader->pending);
    free(loader);
    return NULL;
}

bool metadata_loader_submit(MetadataLoader *loader, size_t pane_index, LONG generation,
                            const wchar_t *folder, const BrowserListing *listing)
{
    MetadataRequest *request;
    MetadataRequest *replaced;
    if (loader == NULL || pane_index >= loader->pane_count) return false;
    request = request_create(pane_index, generation, folder, listing);
    if (request == NULL) return false;
    InterlockedExchange((volatile LONG *)&loader->latest_generation[pane_index], generation);
    EnterCriticalSection(&loader->lock);
    replaced = loader->pending[pane_index];
    loader->pending[pane_index] = request;
    LeaveCriticalSection(&loader->lock);
    request_free(replaced);
    if (!SetEvent(loader->wake_event)) {
        BOOL removed = FALSE;
        EnterCriticalSection(&loader->lock);
        if (loader->pending[pane_index] == request) {
            loader->pending[pane_index] = NULL;
            removed = TRUE;
        }
        LeaveCriticalSection(&loader->lock);
        if (removed) {
            request_free(request);
            return false;
        }
        /* The worker took ownership while the event call was in progress. */
        return true;
    }
    return true;
}

void metadata_loader_cancel(MetadataLoader *loader, size_t pane_index, LONG generation)
{
    MetadataRequest *request = NULL;
    if (loader == NULL || pane_index >= loader->pane_count) return;
    InterlockedExchange((volatile LONG *)&loader->latest_generation[pane_index], generation);
    EnterCriticalSection(&loader->lock);
    request = loader->pending[pane_index];
    loader->pending[pane_index] = NULL;
    LeaveCriticalSection(&loader->lock);
    request_free(request);
}

bool metadata_loader_shutdown(MetadataLoader *loader, DWORD maximum_wait_ms)
{
    size_t index;
    DWORD wait;
    if (loader == NULL) return true;
    SetEvent(loader->stop_event);
    SetEvent(loader->wake_event);
    wait = WaitForSingleObject(loader->thread, maximum_wait_ms);
    if (wait != WAIT_OBJECT_0) return false;
    for (index = 0; index < loader->pane_count; ++index) request_free(loader->pending[index]);
    CloseHandle(loader->thread);
    CloseHandle(loader->stop_event);
    CloseHandle(loader->wake_event);
    DeleteCriticalSection(&loader->lock);
    free((void *)loader->latest_generation);
    free(loader->pending);
    free(loader);
    return true;
}
