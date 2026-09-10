#include "browser.h"

#include <windows.h>
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static wchar_t *duplicate_text(const wchar_t *text)
{
    size_t count = wcslen(text) + 1;
    wchar_t *copy = (wchar_t *)malloc(count * sizeof(*copy));
    if (copy != NULL) {
        memcpy(copy, text, count * sizeof(*copy));
    }
    return copy;
}

static void free_entries(_In_reads_(count) BrowserEntry *entries, size_t count)
{
    size_t index;
    if (entries == NULL) return;
    for (index = 0; index < count; ++index) {
        /*
         * count advances only after name allocation and full initialization.
         * MSVC cannot infer that ownership contract through BrowserListing.
         */
#ifdef _MSC_VER
#pragma warning(suppress: 6001)
#endif
        free(entries[index].name);
    }
    free(entries);
}

bool browser_is_audio_name(const wchar_t *name)
{
    const wchar_t *dot = wcsrchr(name, L'.');
    return dot != NULL && (_wcsicmp(dot, L".wav") == 0 || _wcsicmp(dot, L".mp3") == 0 ||
                           _wcsicmp(dot, L".m4a") == 0 || _wcsicmp(dot, L".mp4") == 0 ||
                           _wcsicmp(dot, L".flac") == 0 || _wcsicmp(dot, L".wma") == 0 ||
                           _wcsicmp(dot, L".ogg") == 0 || _wcsicmp(dot, L".opus") == 0 ||
                           _wcsicmp(dot, L".mid") == 0 || _wcsicmp(dot, L".mod") == 0 ||
                           _wcsicmp(dot, L".s3m") == 0 || _wcsicmp(dot, L".xm") == 0);
}

static int compare_entries(const BrowserEntry *left, const BrowserEntry *right,
                           BrowserSortColumn column, bool descending)
{
    int result = 0;
    if (left->kind != right->kind) {
        return left->kind == BROWSER_ENTRY_DIRECTORY ? -1 : 1;
    }
    if (column == BROWSER_SORT_SIZE && left->kind == BROWSER_ENTRY_AUDIO_FILE) {
        result = left->size < right->size ? -1 : left->size > right->size ? 1 : 0;
    } else if (column == BROWSER_SORT_DURATION && left->kind == BROWSER_ENTRY_AUDIO_FILE) {
        if (left->duration_ms == 0 || right->duration_ms == 0) {
            if (left->duration_ms != right->duration_ms)
                return left->duration_ms == 0 ? 1 : -1;
        } else {
            result = left->duration_ms < right->duration_ms ? -1 :
                     left->duration_ms > right->duration_ms ? 1 : 0;
        }
    }
    if (result == 0) result = _wcsicmp(left->name, right->name);
    return descending ? -result : result;
}

void browser_listing_sort(BrowserListing *listing, BrowserSortColumn column, bool descending)
{
    BrowserEntry *scratch;
    BrowserEntry *source;
    BrowserEntry *destination;
    size_t width;
    if (listing == NULL || listing->entries == NULL) return;
    if (listing->count < 2 || listing->count > SIZE_MAX / sizeof(*scratch)) return;
    scratch = (BrowserEntry *)malloc(listing->count * sizeof(*scratch));
    if (scratch == NULL) return;
    source = listing->entries;
    destination = scratch;
    for (width = 1; width < listing->count;) {
        size_t start;
        size_t block = width > SIZE_MAX / 2 ? SIZE_MAX : width * 2;
        for (start = 0; start < listing->count;) {
            size_t left = start;
            size_t middle = start + min(width, listing->count - start);
            size_t right = middle;
            size_t end = middle + min(width, listing->count - middle);
            size_t output = start;
            while (left < middle && right < end) {
                if (compare_entries(&source[left], &source[right], column, descending) <= 0)
                    destination[output++] = source[left++];
                else
                    destination[output++] = source[right++];
            }
            while (left < middle) destination[output++] = source[left++];
            while (right < end) destination[output++] = source[right++];
            if (block >= listing->count - start) break;
            start += block;
        }
        {
            BrowserEntry *swap = source;
            source = destination;
            destination = swap;
        }
        if (width > listing->count / 2) break;
        width *= 2;
    }
    if (source != listing->entries)
        memcpy(listing->entries, source, listing->count * sizeof(*source));
    free(scratch);
}

void browser_listing_summary(const BrowserListing *listing, BrowserListingSummary *summary)
{
    size_t index;
    if (summary == NULL) return;
    memset(summary, 0, sizeof(*summary));
    if (listing == NULL || listing->entries == NULL) return;
    for (index = 0; index < listing->count; ++index) {
        const BrowserEntry *entry = &listing->entries[index];
        if (entry->kind != BROWSER_ENTRY_AUDIO_FILE) continue;
        ++summary->audio_file_count;
        summary->total_size_bytes += entry->size;
        if (entry->duration_ms == 0)
            ++summary->unknown_duration_count;
        else
            summary->known_duration_ms += entry->duration_ms;
    }
}

bool browser_normalize_folder(const wchar_t *input, wchar_t *output, size_t output_count)
{
    DWORD result;
    if (input == NULL || input[0] == L'\0' || output == NULL || output_count == 0) {
        return false;
    }
    result = GetFullPathNameW(input, (DWORD)output_count, output, NULL);
    if (result == 0 || result >= output_count) {
        return false;
    }
    while (result > 3 && (output[result - 1] == L'\\' || output[result - 1] == L'/')) {
        output[--result] = L'\0';
    }
    return GetFileAttributesW(output) != INVALID_FILE_ATTRIBUTES &&
           (GetFileAttributesW(output) & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool browser_parent_folder(const wchar_t *folder, wchar_t *output, size_t output_count)
{
    wchar_t *slash;
    if (!browser_normalize_folder(folder, output, output_count)) {
        return false;
    }
    if (wcslen(output) <= 3) {
        return true;
    }
    slash = wcsrchr(output, L'\\');
    if (slash == NULL) {
        return false;
    }
    if (slash == output + 2) {
        slash[1] = L'\0';
    } else {
        *slash = L'\0';
    }
    return true;
}

bool browser_join_path(const wchar_t *folder, const wchar_t *name, wchar_t *output, size_t output_count)
{
    int written = swprintf_s(output, output_count, L"%ls%ls%ls", folder,
                             folder[wcslen(folder) - 1] == L'\\' ? L"" : L"\\", name);
    return written > 0 && (size_t)written < output_count;
}

bool browser_list_folder(const wchar_t *folder, BrowserListing *listing)
{
    wchar_t pattern[MAX_PATH];
    WIN32_FIND_DATAW data;
    HANDLE search;
    BrowserEntry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (listing == NULL || !browser_join_path(folder, L"*", pattern, _countof(pattern))) {
        return false;
    }
    listing->entries = NULL;
    listing->count = 0;
    search = FindFirstFileW(pattern, &data);
    if (search == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        DWORD attributes = GetFileAttributesW(folder);
        if (error == ERROR_FILE_NOT_FOUND &&
            attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            return true;
        return false;
    }
    do {
        BrowserEntry entry = {0};
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0 ||
            (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0) {
            continue;
        }
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            entry.kind = BROWSER_ENTRY_DIRECTORY;
        } else if (browser_is_audio_name(data.cFileName)) {
            entry.kind = BROWSER_ENTRY_AUDIO_FILE;
        } else {
            continue;
        }
        entry.name = duplicate_text(data.cFileName);
        if (entry.name == NULL) {
            free_entries(entries, count);
            FindClose(search);
            return false;
        }
        entry.size = ((unsigned long long)data.nFileSizeHigh << 32) | data.nFileSizeLow;
        entry.duration_ms = 0;
        entry.entry_id = count + 1;
        if (count == capacity) {
            size_t new_capacity = capacity == 0 ? 32 : capacity * 2;
            BrowserEntry *grown = (BrowserEntry *)realloc(entries, new_capacity * sizeof(*grown));
            if (grown == NULL) {
                free(entry.name);
                free_entries(entries, count);
                FindClose(search);
                return false;
            }
            entries = grown;
            capacity = new_capacity;
        }
        entries[count++] = entry;
    } while (FindNextFileW(search, &data));
    FindClose(search);
    listing->entries = entries;
    listing->count = count;
    return true;
}

void browser_listing_free(BrowserListing *listing)
{
    if (listing == NULL) {
        return;
    }
    free_entries(listing->entries, listing->count);
    listing->entries = NULL;
    listing->count = 0;
}
