#include "playback.h"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
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
        GetTempFileNameW(directory, L"acs", 0, temporary) == 0)
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

static int create_wave(const wchar_t *path, uint32_t seconds)
{
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

static int create_invalid(const wchar_t *path)
{
    static const char invalid[] = "not audio";
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY, NULL);
    DWORD written;
    if (file == INVALID_HANDLE_VALUE) return 0;
    if (!WriteFile(file, invalid, sizeof(invalid), &written, NULL) ||
        written != sizeof(invalid)) {
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    return 1;
}

static int finish(int result, int owns_fixtures, const wchar_t *good_a,
                  const wchar_t *good_b, const wchar_t *invalid)
{
    playback_stop();
    if (owns_fixtures) {
        DeleteFileW(good_a);
        DeleteFileW(good_b);
        DeleteFileW(invalid);
    }
    return result;
}

static int wait_for_progress(unsigned long timeout_ms)
{
    DWORD start = GetTickCount();
    while (GetTickCount() - start < timeout_ms) {
        if (playback_position_ms() > 0) return 1;
        Sleep(20);
    }
    return 0;
}

int wmain(int argc, wchar_t **argv)
{
    wchar_t generated_a[MAX_PATH] = L"";
    wchar_t generated_b[MAX_PATH] = L"";
    wchar_t generated_invalid[MAX_PATH] = L"";
    const wchar_t *good_a;
    const wchar_t *good_b;
    const wchar_t *invalid;
    int owns_fixtures = argc == 1;
    int iteration;
    if (argc != 1 && argc != 4) {
        fwprintf(stderr, L"usage: playback_stress [<good-a> <good-b> <invalid>]\n");
        return 2;
    }
    if (owns_fixtures) {
        if (!make_temp_wav_path(generated_a, ARRAYSIZE(generated_a)) ||
            !make_temp_wav_path(generated_b, ARRAYSIZE(generated_b)) ||
            !make_temp_wav_path(generated_invalid, ARRAYSIZE(generated_invalid)) ||
            !create_wave(generated_a, 2) || !create_wave(generated_b, 3) ||
            !create_invalid(generated_invalid))
            return finish(8, TRUE, generated_a, generated_b, generated_invalid);
        good_a = generated_a;
        good_b = generated_b;
        invalid = generated_invalid;
    } else {
        good_a = argv[1];
        good_b = argv[2];
        invalid = argv[3];
    }
    playback_set_volume(5);
    for (iteration = 0; iteration < 20; ++iteration) {
        const wchar_t *path = (iteration & 1) ? good_a : good_b;
        if (!playback_play(path) || !wait_for_progress(2000)) {
            fwprintf(stderr, L"iteration %d failed for %ls\n", iteration, path);
            return finish(3, owns_fixtures, good_a, good_b, invalid);
        }
        playback_stop();
        if (playback_is_active())
            return finish(4, owns_fixtures, good_a, good_b, invalid);
    }
    {
        DWORD start;
        DWORD elapsed;
        unsigned long duration;
        if (!playback_play(good_a))
            return finish(6, owns_fixtures, good_a, good_b, invalid);
        duration = playback_duration_ms();
        start = GetTickCount();
        while (playback_is_active() && GetTickCount() - start < duration + 3000) Sleep(10);
        elapsed = GetTickCount() - start;
        playback_stop();
        if (duration == 0 || elapsed + 300 < duration || elapsed > duration + 750) {
            fwprintf(stderr, L"continuous timing failed: duration=%lu elapsed=%lu\n", duration, elapsed);
            return finish(7, owns_fixtures, good_a, good_b, invalid);
        }
        wprintf(L"continuous playback timing duration=%lu elapsed=%lu\n", duration, elapsed);
    }
    if (playback_play(invalid)) {
        Sleep(200);
        if (playback_is_active()) {
            fwprintf(stderr, L"invalid input remained active\n");
            return finish(5, owns_fixtures, good_a, good_b, invalid);
        }
        playback_stop();
    }
    wprintf(L"20 rapid play/stop and switch iterations passed; invalid input rejected\n");
    return finish(0, owns_fixtures, good_a, good_b, invalid);
}
