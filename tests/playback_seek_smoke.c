#include "playback.h"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>

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

static int create_fixture(const wchar_t *path)
{
    const uint32_t seconds = 6;
    const uint32_t sample_rate = 44100;
    const uint32_t data_size = seconds * sample_rate * 2;
    WaveHeader header = {
        {'R','I','F','F'}, 36 + data_size, {'W','A','V','E'},
        {'f','m','t',' '}, 16, 1, 1, sample_rate, sample_rate * 2, 2, 16,
        {'d','a','t','a'}, data_size
    };
    BYTE zeroes[4096] = {0};
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

int wmain(void)
{
    wchar_t directory[MAX_PATH];
    wchar_t path[MAX_PATH];
    unsigned long duration;
    unsigned long position;
    int result = 0;
    if (GetTempPathW(ARRAYSIZE(directory), directory) == 0 ||
        GetTempFileNameW(directory, L"acs", 0, path) == 0)
        return 2;
    DeleteFileW(path);
    {
        wchar_t *extension = wcsrchr(path, L'.');
        if (extension == NULL) return 2;
        wcscpy_s(extension, ARRAYSIZE(path) - (size_t)(extension - path), L".wav");
    }
    if (!create_fixture(path)) {
        DeleteFileW(path);
        return 3;
    }
    playback_set_volume(0);
    if (!playback_play(path)) {
        result = 4;
        goto cleanup;
    }
    Sleep(150);
    duration = playback_duration_ms();
    if (duration < 5500 || duration > 6500 || !playback_seek_ms(3000)) {
        result = 5;
        goto cleanup;
    }
    Sleep(300);
    position = playback_position_ms();
    if (position < 2900 || position > 4000) {
        fwprintf(stderr, L"seek position outside expected range: %lu\n", position);
        result = 6;
        goto cleanup;
    }
    if (!playback_seek_ms(500)) {
        result = 7;
        goto cleanup;
    }
    Sleep(200);
    position = playback_position_ms();
    if (position < 400 || position > 1500) {
        fwprintf(stderr, L"backward seek outside expected range: %lu\n", position);
        result = 8;
        goto cleanup;
    }
    if (!playback_seek_ms(5200)) {
        result = 9;
        goto cleanup;
    }
    Sleep(200);
    position = playback_position_ms();
    if (position < 5100 || position >= duration) {
        fwprintf(stderr, L"near-end seek outside expected range: %lu\n", position);
        result = 10;
        goto cleanup;
    }
    playback_stop();
    if (playback_is_active() || playback_seek_ms(1000)) {
        result = 11;
        goto cleanup;
    }
    wprintf(L"generated WAV seek passed: duration=%lu position=%lu\n",
            duration, position);
cleanup:
    playback_stop();
    DeleteFileW(path);
    return result;
}
