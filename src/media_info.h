#ifndef AUDIOCOMMANDER_MEDIA_INFO_H
#define AUDIOCOMMANDER_MEDIA_INFO_H

#include <wchar.h>

typedef struct MediaInfo {
    unsigned long duration_ms;
    unsigned long bitrate_kbps;
    unsigned long sample_rate_hz;
    unsigned long channels;
    unsigned long bit_depth;
    wchar_t container[64];
    wchar_t codec[64];
    wchar_t channel_layout[128];
    wchar_t bitrate_mode[32];
    wchar_t title[256];
    wchar_t artist[256];
    wchar_t album[256];
} MediaInfo;

unsigned long media_duration_ms(const wchar_t *path);
void media_read_info(const wchar_t *path, MediaInfo *info);

#endif
