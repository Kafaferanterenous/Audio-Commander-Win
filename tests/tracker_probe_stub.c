#include <stdbool.h>
#include <wchar.h>

#include "media_info.h"

/*
 * The privacy-safe folder timing probe exercises MP3/WAV/Shell/FFmpeg metadata.
 * Tracker formats are intentionally reported unsupported in this one-off probe;
 * the sanitized large-folder fixture contains only MP3 files.
 */
bool tracker_is_path(const wchar_t *path)
{
    (void)path;
    return false;
}

bool tracker_read_info(const wchar_t *path, MediaInfo *info)
{
    (void)path;
    (void)info;
    return false;
}
