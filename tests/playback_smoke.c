#include "playback.h"

#include <windows.h>
#include <stdio.h>

int wmain(int argument_count, wchar_t **arguments)
{
    unsigned long duration;
    unsigned long position;
    unsigned long seek_target = 0;
    if (argument_count != 2 && argument_count != 3) {
        fwprintf(stderr, L"usage: playback_smoke <audio-file> [seek-ms]\n");
        return 2;
    }
    if (argument_count == 3) {
        wchar_t *end = NULL;
        seek_target = wcstoul(arguments[2], &end, 10);
        if (end == arguments[2] || *end != L'\0') {
            fwprintf(stderr, L"invalid seek position: %ls\n", arguments[2]);
            return 2;
        }
    }
    playback_set_volume(0);
    if (!playback_play(arguments[1])) {
        fwprintf(stderr, L"playback_play failed: %ls\n", arguments[1]);
        return 3;
    }
    duration = 0;
    position = 0;
    for (int attempt = 0; attempt < 20 && (duration == 0 || position == 0); ++attempt) {
        MSG message;
        MsgWaitForMultipleObjects(0, NULL, FALSE, 250, QS_ALLINPUT);
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        duration = playback_duration_ms();
        position = playback_position_ms();
    }
    wprintf(L"active=%d duration_ms=%lu position_ms=%lu\n",
            playback_is_active() ? 1 : 0, duration, position);
    if (argument_count == 3) {
        unsigned long minimum;
        if (seek_target >= duration) seek_target = duration - 1;
        if (!playback_seek_ms(seek_target)) {
            fwprintf(stderr, L"seek failed: %lu\n", seek_target);
            playback_stop();
            return 5;
        }
        Sleep(350);
        position = playback_position_ms();
        minimum = seek_target > 250 ? seek_target - 250 : 0;
        if (position < minimum || position > seek_target + 1500) {
            fwprintf(stderr, L"seek position outside expected range: target=%lu actual=%lu\n",
                     seek_target, position);
            playback_stop();
            return 6;
        }
        wprintf(L"seek passed: target_ms=%lu position_ms=%lu\n",
                seek_target, position);
    }
    playback_stop();
    if (duration == 0 || position == 0) return 4;
    return 0;
}
