#include "browser.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static int verify_large_sort(void)
{
    enum { ITEM_COUNT = 10000, NAME_LENGTH = 24 };
    BrowserEntry *entries = (BrowserEntry *)calloc(ITEM_COUNT, sizeof(*entries));
    wchar_t *names = (wchar_t *)calloc(ITEM_COUNT * NAME_LENGTH, sizeof(*names));
    BrowserListing listing;
    size_t index;
    if (entries == NULL || names == NULL) {
        free(entries);
        free(names);
        return 20;
    }
    for (index = 0; index < ITEM_COUNT; ++index) {
        entries[index].name = names + index * NAME_LENGTH;
        swprintf_s(entries[index].name, NAME_LENGTH, L"track-%05llu.mp3",
                   (unsigned long long)(ITEM_COUNT - index));
        entries[index].kind = BROWSER_ENTRY_AUDIO_FILE;
        entries[index].entry_id = index + 1;
    }
    listing.entries = entries;
    listing.count = ITEM_COUNT;
    browser_listing_sort(&listing, BROWSER_SORT_NAME, false);
    for (index = 1; index < ITEM_COUNT; ++index) {
        if (_wcsicmp(entries[index - 1].name, entries[index].name) > 0) {
            free(entries);
            free(names);
            return 21;
        }
    }
    free(entries);
    free(names);
    return 0;
}

int wmain(int argc, wchar_t **argv)
{
    if (argc == 3 && wcscmp(argv[1], L"--probe-folder") == 0) {
        BrowserListing listing = {0};
        BrowserListingSummary summary;
        ULONGLONG started = GetTickCount64();
        ULONGLONG elapsed;
        if (!browser_list_folder(argv[2], &listing)) return 11;
        elapsed = GetTickCount64() - started;
        browser_listing_summary(&listing, &summary);
        wprintf(L"entries=%llu audio_files=%llu known_duration=%llu unknown_duration=%llu "
                L"elapsed_ms=%llu\n",
                (unsigned long long)listing.count,
                summary.audio_file_count,
                summary.audio_file_count - summary.unknown_duration_count,
                summary.unknown_duration_count,
                (unsigned long long)elapsed);
        browser_listing_free(&listing);
        return 0;
    }
    if (argc != 1) return 12;
    if (!browser_is_audio_name(L"music.MOD") ||
        !browser_is_audio_name(L"music.s3m") ||
        !browser_is_audio_name(L"music.Xm") ||
        browser_is_audio_name(L"music.txt"))
        return 1;
    wchar_t temp_path[MAX_PATH];
    wchar_t empty_folder[MAX_PATH];
    BrowserEntry entries[] = {
        {L"large.wav", BROWSER_ENTRY_AUDIO_FILE, 900, 9000, 1},
        {L"Folder B", BROWSER_ENTRY_DIRECTORY, 0, 0, 2},
        {L"unknown.wav", BROWSER_ENTRY_AUDIO_FILE, 500, 0, 3},
        {L"small.wav", BROWSER_ENTRY_AUDIO_FILE, 100, 3000, 4},
        {L"Folder A", BROWSER_ENTRY_DIRECTORY, 0, 0, 5}
    };
    BrowserListing listing = {entries, ARRAYSIZE(entries)};
    BrowserListingSummary summary;

    if (GetTempPathW(ARRAYSIZE(temp_path), temp_path) == 0 ||
        GetTempFileNameW(temp_path, L"acb", 0, empty_folder) == 0)
        return 7;
    DeleteFileW(empty_folder);
    if (!CreateDirectoryW(empty_folder, NULL)) return 8;
    {
        BrowserListing empty = {0};
        if (!browser_list_folder(empty_folder, &empty) || empty.count != 0) {
            RemoveDirectoryW(empty_folder);
            return 9;
        }
        browser_listing_free(&empty);
    }
    if (!RemoveDirectoryW(empty_folder)) return 10;

    if (!browser_is_audio_name(L"song.mid") || !browser_is_audio_name(L"SONG.MID") ||
        !browser_is_audio_name(L"song.mod") || browser_is_audio_name(L"no_extension"))
        return 6;
    browser_listing_summary(&listing, &summary);
    if (summary.audio_file_count != 3 || summary.total_size_bytes != 1500 ||
        summary.known_duration_ms != 12000 || summary.unknown_duration_count != 1)
        return 1;

    browser_listing_sort(&listing, BROWSER_SORT_NAME, false);
    if (wcscmp(entries[0].name, L"Folder A") != 0 ||
        wcscmp(entries[1].name, L"Folder B") != 0 ||
        wcscmp(entries[2].name, L"large.wav") != 0 ||
        wcscmp(entries[3].name, L"small.wav") != 0 ||
        wcscmp(entries[4].name, L"unknown.wav") != 0)
        return 2;

    browser_listing_sort(&listing, BROWSER_SORT_SIZE, false);
    if (wcscmp(entries[0].name, L"Folder A") != 0 ||
        wcscmp(entries[1].name, L"Folder B") != 0 ||
        wcscmp(entries[2].name, L"small.wav") != 0 ||
        wcscmp(entries[3].name, L"unknown.wav") != 0 ||
        wcscmp(entries[4].name, L"large.wav") != 0)
        return 3;

    browser_listing_sort(&listing, BROWSER_SORT_DURATION, true);
    if (wcscmp(entries[0].name, L"Folder B") != 0 ||
        wcscmp(entries[1].name, L"Folder A") != 0 ||
        wcscmp(entries[2].name, L"large.wav") != 0 ||
        wcscmp(entries[3].name, L"small.wav") != 0 ||
        wcscmp(entries[4].name, L"unknown.wav") != 0)
        return 4;

    {
        int large_sort_result = verify_large_sort();
        if (large_sort_result != 0) return large_sort_result;
    }

    wprintf(L"empty-folder listing plus 10,000-entry name, size, and duration sorting passed\n");
    browser_listing_summary(NULL, &summary);
    if (summary.audio_file_count != 0 || summary.total_size_bytes != 0 ||
        summary.known_duration_ms != 0 || summary.unknown_duration_count != 0)
        return 5;

    return 0;
}
