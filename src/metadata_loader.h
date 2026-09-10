#ifndef AUDIOCOMMANDER_METADATA_LOADER_H
#define AUDIOCOMMANDER_METADATA_LOADER_H

#include <windows.h>
#include <stdbool.h>
#include <stddef.h>

#include "browser.h"

#define METADATA_BATCH_CAPACITY 16

typedef unsigned long (*MetadataDurationReader)(const wchar_t *path, void *context);

typedef struct MetadataDurationUpdate {
    unsigned long long entry_id;
    unsigned long duration_ms;
} MetadataDurationUpdate;

typedef struct MetadataBatch {
    size_t pane_index;
    LONG generation;
    size_t count;
    BOOL complete;
    MetadataDurationUpdate updates[METADATA_BATCH_CAPACITY];
} MetadataBatch;

typedef struct MetadataLoader MetadataLoader;

MetadataLoader *metadata_loader_create(HWND notify_window, UINT notify_message,
                                       size_t pane_count,
                                       MetadataDurationReader reader,
                                       void *reader_context);
bool metadata_loader_submit(MetadataLoader *loader, size_t pane_index, LONG generation,
                            const wchar_t *folder, const BrowserListing *listing);
void metadata_loader_cancel(MetadataLoader *loader, size_t pane_index, LONG generation);
bool metadata_loader_shutdown(MetadataLoader *loader, DWORD maximum_wait_ms);
void metadata_batch_free(MetadataBatch *batch);

#endif
