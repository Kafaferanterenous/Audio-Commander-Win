#ifndef AUDIOCOMMANDER_BROWSER_H
#define AUDIOCOMMANDER_BROWSER_H

#include <stdbool.h>
#include <stddef.h>
#include <wchar.h>

typedef enum BrowserEntryKind {
    BROWSER_ENTRY_DIRECTORY,
    BROWSER_ENTRY_AUDIO_FILE
} BrowserEntryKind;

typedef struct BrowserEntry {
    wchar_t *name;
    BrowserEntryKind kind;
    unsigned long long size;
    unsigned long duration_ms;
} BrowserEntry;

typedef struct BrowserListing {
    BrowserEntry *entries;
    size_t count;
} BrowserListing;

typedef struct BrowserListingSummary {
    unsigned long long audio_file_count;
    unsigned long long total_size_bytes;
    unsigned long long known_duration_ms;
    unsigned long long unknown_duration_count;
} BrowserListingSummary;

typedef enum BrowserSortColumn {
    BROWSER_SORT_NAME,
    BROWSER_SORT_SIZE,
    BROWSER_SORT_DURATION
} BrowserSortColumn;

bool browser_is_audio_name(const wchar_t *name);
bool browser_normalize_folder(const wchar_t *input, wchar_t *output, size_t output_count);
bool browser_parent_folder(const wchar_t *folder, wchar_t *output, size_t output_count);
bool browser_join_path(const wchar_t *folder, const wchar_t *name, wchar_t *output, size_t output_count);
bool browser_list_folder(const wchar_t *folder, BrowserListing *listing);
void browser_listing_sort(BrowserListing *listing, BrowserSortColumn column, bool descending);
void browser_listing_summary(const BrowserListing *listing, BrowserListingSummary *summary);
void browser_listing_free(BrowserListing *listing);

#endif
