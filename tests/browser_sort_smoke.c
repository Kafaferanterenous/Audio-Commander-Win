#include "browser.h"

#include <windows.h>
#include <stdio.h>
#include <wchar.h>

int wmain(void)
{
    if (!browser_is_audio_name(L"music.MOD") ||
        !browser_is_audio_name(L"music.s3m") ||
        !browser_is_audio_name(L"music.Xm") ||
        browser_is_audio_name(L"music.txt"))
        return 1;
    wchar_t temp_path[MAX_PATH];
    wchar_t empty_folder[MAX_PATH];
    BrowserEntry entries[] = {
        {L"large.wav", BROWSER_ENTRY_AUDIO_FILE, 900, 9000},
        {L"Folder B", BROWSER_ENTRY_DIRECTORY, 0, 0},
        {L"unknown.wav", BROWSER_ENTRY_AUDIO_FILE, 500, 0},
        {L"small.wav", BROWSER_ENTRY_AUDIO_FILE, 100, 3000},
        {L"Folder A", BROWSER_ENTRY_DIRECTORY, 0, 0}
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

    wprintf(L"empty-folder listing plus name, size, and duration sorting passed\n");
    browser_listing_summary(NULL, &summary);
    if (summary.audio_file_count != 0 || summary.total_size_bytes != 0 ||
        summary.known_duration_ms != 0 || summary.unknown_duration_count != 0)
        return 5;

    return 0;
}
