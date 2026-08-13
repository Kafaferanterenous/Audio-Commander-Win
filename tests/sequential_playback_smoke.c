#include "playback.h"
#include "playlist.h"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#pragma pack(push, 1)
typedef struct WaveHeader {
    char riff[4];
    uint32_t file_size_minus_8;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_size;
} WaveHeader;
#pragma pack(pop)

static int make_temp_wav_path(wchar_t *path, size_t count)
{
    wchar_t directory[MAX_PATH];
    wchar_t temporary[MAX_PATH];
    wchar_t *extension;
    if (GetTempPathW(ARRAYSIZE(directory), directory) == 0 ||
        GetTempFileNameW(directory, L"acq", 0, temporary) == 0)
        return 0;
    if (wcscpy_s(path, count, temporary) != 0) {
        DeleteFileW(temporary);
        return 0;
    }
    extension = wcsrchr(path, L'.');
    if (extension == NULL ||
        wcscpy_s(extension, count - (size_t)(extension - path), L".wav") != 0 ||
        !MoveFileW(temporary, path)) {
        DeleteFileW(temporary);
        return 0;
    }
    return 1;
}

static int create_silent_wave(const wchar_t *path)
{
    const uint32_t sample_rate = 8000;
    const uint32_t data_size = sample_rate * 2;
    WaveHeader header = {
        {'R','I','F','F'}, 36 + data_size, {'W','A','V','E'},
        {'f','m','t',' '}, 16, 1, 1, sample_rate, sample_rate * 2, 2, 16,
        {'d','a','t','a'}, data_size
    };
    BYTE zeroes[1024] = {0};
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY, NULL);
    DWORD written;
    uint32_t remaining = data_size;
    if (file == INVALID_HANDLE_VALUE) return 0;
    if (!WriteFile(file, &header, sizeof(header), &written, NULL) ||
        written != sizeof(header)) {
        CloseHandle(file);
        return 0;
    }
    while (remaining > 0) {
        DWORD chunk = remaining < sizeof(zeroes) ? remaining : sizeof(zeroes);
        if (!WriteFile(file, zeroes, chunk, &written, NULL) || written != chunk) {
            CloseHandle(file);
            return 0;
        }
        remaining -= chunk;
    }
    CloseHandle(file);
    return 1;
}

static int create_invalid_wave(const wchar_t *path)
{
    static const char invalid[] = "not audio";
    DWORD written;
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    if (!WriteFile(file, invalid, sizeof(invalid), &written, NULL) ||
        written != sizeof(invalid)) {
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    return 1;
}

static int wait_until_stopped(unsigned long timeout_ms)
{
    DWORD start = GetTickCount();
    while (playback_is_active() && GetTickCount() - start < timeout_ms) Sleep(10);
    return !playback_is_active();
}

static const wchar_t *base_name(const wchar_t *path)
{
    const wchar_t *slash = wcsrchr(path, (wchar_t)0x005c);
    return slash == NULL ? path : slash + 1;
}

static int finish(int result, Playlist *queue, const wchar_t *first,
                  const wchar_t *invalid, const wchar_t *last)
{
    playback_stop();
    playlist_clear(queue);
    DeleteFileW(first);
    DeleteFileW(invalid);
    DeleteFileW(last);
    return result;
}

int wmain(void)
{
    wchar_t first[MAX_PATH] = L"";
    wchar_t invalid[MAX_PATH] = L"";
    wchar_t last[MAX_PATH] = L"";
    wchar_t folder[MAX_PATH];
    wchar_t *slash;
    BrowserEntry entries[3] = {0};
    BrowserListing listing = {0};
    Playlist queue = {0};
    const wchar_t *path;
    unsigned long duration;

    if (!make_temp_wav_path(first, ARRAYSIZE(first)) ||
        !make_temp_wav_path(invalid, ARRAYSIZE(invalid)) ||
        !make_temp_wav_path(last, ARRAYSIZE(last)) ||
        !create_silent_wave(first) || !create_invalid_wave(invalid) ||
        !create_silent_wave(last))
        return finish(2, &queue, first, invalid, last);

    wcscpy_s(folder, ARRAYSIZE(folder), first);
    slash = wcsrchr(folder, (wchar_t)0x005c);
    if (slash == NULL) return finish(3, &queue, first, invalid, last);
    *slash = (wchar_t)0x005c;
    slash[1] = L'\0';

    entries[0].name = (wchar_t *)base_name(first);
    entries[1].name = (wchar_t *)base_name(invalid);
    entries[2].name = (wchar_t *)base_name(last);
    entries[0].kind = entries[1].kind = entries[2].kind = BROWSER_ENTRY_AUDIO_FILE;
    listing.entries = entries;
    listing.count = ARRAYSIZE(entries);

    if (!playlist_build_after(&queue, folder, &listing, 0) || queue.count != 2)
        return finish(4, &queue, first, invalid, last);

    playback_set_volume(0);
    if (!playback_play(first))
        return finish(5, &queue, first, invalid, last);
    duration = playback_duration_ms();
    if (duration == 0 || !wait_until_stopped(duration + 2000))
        return finish(6, &queue, first, invalid, last);

    path = playlist_next(&queue);
    if (path == NULL || _wcsicmp(path, invalid) != 0)
        return finish(7, &queue, first, invalid, last);
    if (playback_play(path)) {
        playback_stop();
        return finish(8, &queue, first, invalid, last);
    }

    path = playlist_next(&queue);
    if (path == NULL || _wcsicmp(path, last) != 0 || !playback_play(path))
        return finish(9, &queue, first, invalid, last);
    duration = playback_duration_ms();
    if (duration == 0 || !wait_until_stopped(duration + 2000) ||
        playlist_next(&queue) != NULL)
        return finish(10, &queue, first, invalid, last);

    wprintf(L"natural completion, corrupt-item skip, and final queue exhaustion passed\n");
    return finish(0, &queue, first, invalid, last);
}
