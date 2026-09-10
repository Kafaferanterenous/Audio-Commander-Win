#include "media_info.h"
#include "playback.h"

#include <stdio.h>
#include <windows.h>

static int create_corrupt_fixture(wchar_t *path, size_t count)
{
    wchar_t temporary[MAX_PATH];
    FILE *file = NULL;
    const char invalid_data[] = "not a tracker module";
    if (GetTempPathW((DWORD)count, path) == 0 ||
        GetTempFileNameW(path, L"act", 0, temporary) == 0) return 0;
    DeleteFileW(temporary);
    if (wcslen(temporary) + 1 > count) return 0;
    wcscpy_s(path, count, temporary);
    wcscpy_s(path + wcslen(path) - 4, 5, L".mod");
    if (_wfopen_s(&file, path, L"wb") != 0 || file == NULL) return 0;
    if (fwrite(invalid_data, 1, sizeof(invalid_data) - 1, file) !=
        sizeof(invalid_data) - 1) {
        fclose(file);
        DeleteFileW(path);
        return 0;
    }
    fclose(file);
    return 1;
}

int wmain(void)
{
    wchar_t path[MAX_PATH];
    MediaInfo info;
    if (!create_corrupt_fixture(path, ARRAYSIZE(path))) return 2;
    media_read_info(path, &info);
    if (info.duration_ms != 0) {
        DeleteFileW(path);
        fwprintf(stderr, L"corrupt module reported duration: %lu\n", info.duration_ms);
        return 3;
    }
    playback_set_volume(0);
    if (playback_play(path)) {
        playback_stop();
        DeleteFileW(path);
        fwprintf(stderr, L"corrupt module unexpectedly started playback\n");
        return 4;
    }
    DeleteFileW(path);
    wprintf(L"corrupt tracker rejected safely\n");
    return 0;
}
