#include "media_info.h"
#include "playback.h"

#include <stdio.h>

int wmain(int argc, wchar_t **argv)
{
    MediaInfo info;
    if (argc != 2) return 2;
    media_read_info(argv[1], &info);
    if (info.duration_ms != 0) {
        fwprintf(stderr, L"corrupt module reported duration: %lu\n", info.duration_ms);
        return 3;
    }
    playback_set_volume(0);
    if (playback_play(argv[1])) {
        playback_stop();
        fwprintf(stderr, L"corrupt module unexpectedly started playback\n");
        return 4;
    }
    wprintf(L"corrupt tracker rejected safely\n");
    return 0;
}
