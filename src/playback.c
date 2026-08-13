#include "playback.h"

#include <windows.h>
#include <mmsystem.h>
#include <wchar.h>
#include <stdlib.h>
#include "playback_mf.h"
#include "playback_ffmpeg.h"
#include "playback_openmpt.h"
#include "tracker_support.h"

typedef enum PlaybackBackend {
    PLAYBACK_BACKEND_NONE,
    PLAYBACK_BACKEND_MCI,
    PLAYBACK_BACKEND_MEDIA_FOUNDATION,
    PLAYBACK_BACKEND_FFMPEG,
    PLAYBACK_BACKEND_OPENMPT
} PlaybackBackend;

static PlaybackBackend active_backend;
static int current_volume = 75;

static unsigned long query_value(const wchar_t *property)
{
    wchar_t command[64];
    wchar_t result[64];
    swprintf_s(command, ARRAYSIZE(command), L"status acplay %ls", property);
    if (active_backend != PLAYBACK_BACKEND_MCI ||
        mciSendStringW(command, result, ARRAYSIZE(result), NULL) != 0) return 0;
    return wcstoul(result, NULL, 10);
}

bool playback_play(const wchar_t *path)
{
    wchar_t *command;
    size_t command_count;
    const wchar_t *extension;
    int written;
    if (path == NULL || path[0] == L'\0') return false;
    extension = wcsrchr(path, L'.');
    bool ffmpeg_only = extension != NULL && (_wcsicmp(extension, L".ogg") == 0 ||
                                             _wcsicmp(extension, L".opus") == 0);
    bool audio_extension = extension != NULL && (_wcsicmp(extension, L".wav") == 0 ||
                                                  _wcsicmp(extension, L".mp3") == 0 ||
                                                  _wcsicmp(extension, L".m4a") == 0 ||
                                                  _wcsicmp(extension, L".mp4") == 0 ||
                                                  _wcsicmp(extension, L".flac") == 0 ||
                                                  _wcsicmp(extension, L".wma") == 0 || ffmpeg_only);
    playback_stop();
    if (tracker_is_path(path)) {
        if (!playback_openmpt_play(path, current_volume)) return false;
        active_backend = PLAYBACK_BACKEND_OPENMPT;
        return true;
    }
    if (audio_extension && playback_ffmpeg_play(path, current_volume)) {
        active_backend = PLAYBACK_BACKEND_FFMPEG;
        return true;
    }
    if (ffmpeg_only) return false;
    if (extension != NULL && (_wcsicmp(extension, L".m4a") == 0 ||
                              _wcsicmp(extension, L".mp4") == 0 ||
                              _wcsicmp(extension, L".flac") == 0 ||
                              _wcsicmp(extension, L".wma") == 0)) {
        if (!playback_mf_play(path, current_volume)) return false;
        active_backend = PLAYBACK_BACKEND_MEDIA_FOUNDATION;
        return true;
    }
    command_count = wcslen(path) + 32;
    command = (wchar_t *)malloc(command_count * sizeof(*command));
    if (command == NULL) return false;
    written = swprintf_s(command, command_count, L"open \"%ls\" alias acplay", path);
    if (written < 0 || mciSendStringW(command, NULL, 0, NULL) != 0) {
        free(command);
        return false;
    }
    free(command);
    active_backend = PLAYBACK_BACKEND_MCI;
    mciSendStringW(L"set acplay time format milliseconds", NULL, 0, NULL);
    playback_set_volume(current_volume);
    if (mciSendStringW(L"play acplay", NULL, 0, NULL) != 0) {
        playback_stop();
        return false;
    }
    return true;
}

void playback_stop(void)
{
    if (active_backend == PLAYBACK_BACKEND_MCI) {
        mciSendStringW(L"stop acplay", NULL, 0, NULL);
        mciSendStringW(L"close acplay", NULL, 0, NULL);
    } else if (active_backend == PLAYBACK_BACKEND_MEDIA_FOUNDATION) {
        playback_mf_stop();
    } else if (active_backend == PLAYBACK_BACKEND_FFMPEG) {
        playback_ffmpeg_stop();
    } else if (active_backend == PLAYBACK_BACKEND_OPENMPT) {
        playback_openmpt_stop();
    }
    active_backend = PLAYBACK_BACKEND_NONE;
}

bool playback_is_active(void)
{
    wchar_t mode[32];
    if (active_backend == PLAYBACK_BACKEND_MEDIA_FOUNDATION) return playback_mf_is_active();
    if (active_backend == PLAYBACK_BACKEND_FFMPEG) return playback_ffmpeg_is_active();
    if (active_backend == PLAYBACK_BACKEND_OPENMPT) return playback_openmpt_is_active();
    if (active_backend != PLAYBACK_BACKEND_MCI) return false;
    if (mciSendStringW(L"status acplay mode", mode, ARRAYSIZE(mode), NULL) != 0) return false;
    return _wcsicmp(mode, L"playing") == 0 || _wcsicmp(mode, L"paused") == 0;
}

unsigned long playback_position_ms(void)
{
    if (active_backend == PLAYBACK_BACKEND_MEDIA_FOUNDATION) return playback_mf_position_ms();
    if (active_backend == PLAYBACK_BACKEND_FFMPEG) return playback_ffmpeg_position_ms();
    if (active_backend == PLAYBACK_BACKEND_OPENMPT) return playback_openmpt_position_ms();
    return query_value(L"position");
}

unsigned long playback_duration_ms(void)
{
    if (active_backend == PLAYBACK_BACKEND_MEDIA_FOUNDATION) return playback_mf_duration_ms();
    if (active_backend == PLAYBACK_BACKEND_FFMPEG) return playback_ffmpeg_duration_ms();
    if (active_backend == PLAYBACK_BACKEND_OPENMPT) return playback_openmpt_duration_ms();
    return query_value(L"length");
}

bool playback_seek_ms(unsigned long position_ms)
{
    unsigned long duration = playback_duration_ms();
    wchar_t command[64];
    if (active_backend == PLAYBACK_BACKEND_NONE || duration == 0) return false;
    if (position_ms >= duration) position_ms = duration > 0 ? duration - 1 : 0;
    if (active_backend == PLAYBACK_BACKEND_MEDIA_FOUNDATION)
        return playback_mf_seek_ms(position_ms);
    if (active_backend == PLAYBACK_BACKEND_FFMPEG)
        return playback_ffmpeg_seek_ms(position_ms);
    if (active_backend == PLAYBACK_BACKEND_OPENMPT)
        return playback_openmpt_seek_ms(position_ms);
    swprintf_s(command, ARRAYSIZE(command), L"seek acplay to %lu", position_ms);
    if (mciSendStringW(command, NULL, 0, NULL) != 0) return false;
    return mciSendStringW(L"play acplay", NULL, 0, NULL) == 0;
}

void playback_set_volume(int percent)
{
    wchar_t command[64];
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    current_volume = percent;
    if (active_backend == PLAYBACK_BACKEND_MEDIA_FOUNDATION) {
        playback_mf_set_volume(percent);
        return;
    }
    if (active_backend == PLAYBACK_BACKEND_FFMPEG) {
        playback_ffmpeg_set_volume(percent);
        return;
    }
    if (active_backend == PLAYBACK_BACKEND_OPENMPT) {
        playback_openmpt_set_volume(percent);
        return;
    }
    if (active_backend != PLAYBACK_BACKEND_MCI) return;
    swprintf_s(command, ARRAYSIZE(command), L"setaudio acplay volume to %d", percent * 10);
    mciSendStringW(command, NULL, 0, NULL);
}
