#include <windows.h>
#include <stdio.h>

#include "media_info.h"

int wmain(int argc, wchar_t **argv)
{
    MediaInfo info;
    unsigned long expected = 5000;
    if (argc < 2 || argc > 3) {
        fwprintf(stderr, L"usage: media_info_smoke <audio-file> [expected-ms]\n");
        return 2;
    }
    if (argc == 3) expected = wcstoul(argv[2], NULL, 10);
    media_read_info(argv[1], &info);
    wprintf(L"duration_ms=%lu bitrate_kbps=%lu mode=%ls sample_rate_hz=%lu "
            L"bit_depth=%lu channels=%lu layout=%ls container=%ls codec=%ls "
            L"title=%ls artist=%ls album=%ls\n",
            info.duration_ms, info.bitrate_kbps, info.bitrate_mode,
            info.sample_rate_hz, info.bit_depth, info.channels, info.channel_layout,
            info.container, info.codec, info.title, info.artist, info.album);
    return info.duration_ms + 150 >= expected && info.duration_ms <= expected + 150 &&
           info.codec[0] != L'\0' && info.channel_layout[0] != L'\0' ? 0 : 3;
}
