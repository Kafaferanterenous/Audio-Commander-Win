#include "tracker_support.h"

#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TRACKER_MAX_FILE_BYTES (512ULL * 1024ULL * 1024ULL)

bool tracker_is_path(const wchar_t *path)
{
    const wchar_t *extension;
    if (path == NULL) return false;
    extension = wcsrchr(path, L'.');
    return extension != NULL && (_wcsicmp(extension, L".mod") == 0 ||
                                 _wcsicmp(extension, L".s3m") == 0 ||
                                 _wcsicmp(extension, L".xm") == 0);
}

bool tracker_module_open(const wchar_t *path, TrackerModule *tracker)
{
    HANDLE file;
    LARGE_INTEGER size;
    unsigned char *data = NULL;
    size_t offset = 0;
    int error = 0;
    const char *error_message = NULL;
    if (tracker == NULL || !tracker_is_path(path)) return false;
    ZeroMemory(tracker, sizeof(*tracker));
    file = CreateFileW(path, GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 ||
        (unsigned long long)size.QuadPart > TRACKER_MAX_FILE_BYTES ||
        (unsigned long long)size.QuadPart > SIZE_MAX) {
        CloseHandle(file);
        return false;
    }
    data = (unsigned char *)malloc((size_t)size.QuadPart);
    if (data == NULL) {
        CloseHandle(file);
        return false;
    }
    while (offset < (size_t)size.QuadPart) {
        DWORD request = (DWORD)(((size_t)size.QuadPart - offset) > 1024 * 1024 ?
                                1024 * 1024 : ((size_t)size.QuadPart - offset));
        DWORD read = 0;
        if (!ReadFile(file, data + offset, request, &read, NULL) || read != request) {
            free(data);
            CloseHandle(file);
            return false;
        }
        offset += read;
    }
    CloseHandle(file);
    tracker->module = openmpt_module_create_from_memory2(
        data, (size_t)size.QuadPart, openmpt_log_func_silent, NULL,
        NULL, NULL, &error, &error_message, NULL);
    if (error_message != NULL) openmpt_free_string(error_message);
    if (tracker->module == NULL) {
        free(data);
        return false;
    }
    tracker->file_data = data;
    tracker->file_size = (size_t)size.QuadPart;
    openmpt_module_set_repeat_count(tracker->module, 0);
    return true;
}

void tracker_module_close(TrackerModule *tracker)
{
    if (tracker == NULL) return;
    if (tracker->module != NULL) openmpt_module_destroy(tracker->module);
    free(tracker->file_data);
    ZeroMemory(tracker, sizeof(*tracker));
}

static void copy_metadata(openmpt_module *module, const char *key,
                          wchar_t *destination, size_t count)
{
    const char *value;
    if (destination == NULL || count == 0) return;
    destination[0] = L'\0';
    value = openmpt_module_get_metadata(module, key);
    if (value != NULL) {
        MultiByteToWideChar(CP_UTF8, 0, value, -1, destination, (int)count);
        destination[count - 1] = L'\0';
        openmpt_free_string(value);
    }
}

bool tracker_read_info(const wchar_t *path, MediaInfo *info)
{
    TrackerModule tracker;
    const wchar_t *extension;
    double seconds;
    if (info == NULL || !tracker_module_open(path, &tracker)) return false;
    seconds = openmpt_module_get_duration_seconds(tracker.module);
    if (seconds > 0.0 && seconds < 4294967.0)
        info->duration_ms = (unsigned long)(seconds * 1000.0 + 0.5);
    info->sample_rate_hz = 48000;
    info->channels = 2;
    info->bit_depth = 16;
    wcscpy_s(info->channel_layout, _countof(info->channel_layout), L"stereo");
    wcscpy_s(info->codec, _countof(info->codec), L"libopenmpt tracker");
    extension = wcsrchr(path, L'.');
    wcscpy_s(info->container, _countof(info->container),
             extension != NULL && _wcsicmp(extension, L".mod") == 0 ? L"ProTracker MOD" :
             extension != NULL && _wcsicmp(extension, L".s3m") == 0 ? L"Scream Tracker 3" :
             L"FastTracker II XM");
    copy_metadata(tracker.module, "title", info->title, _countof(info->title));
    copy_metadata(tracker.module, "artist", info->artist, _countof(info->artist));
    tracker_module_close(&tracker);
    return info->duration_ms != 0;
}
