#include "playback_openmpt.h"

#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stdlib.h>

#include "tracker_support.h"

#define TRACKER_SAMPLE_RATE 48000
#define TRACKER_BUFFER_FRAMES 4096
#define TRACKER_BUFFER_COUNT 6

typedef struct OpenMptPlayback {
    TrackerModule tracker;
    HANDLE thread;
    HWAVEOUT output;
    volatile LONG stop_requested;
    volatile LONG active;
    volatile LONG position_ms;
    unsigned long duration_ms;
    unsigned long start_position_ms;
    unsigned long long completed_frames;
    wchar_t path[32768];
    struct {
        WAVEHDR header;
        int16_t *data;
        DWORD frames;
        BOOL prepared;
    } buffers[TRACKER_BUFFER_COUNT];
    int next_buffer;
} OpenMptPlayback;

static OpenMptPlayback playback;
static int playback_volume = 75;

static void release_buffer(int index, BOOL wait_for_completion)
{
    WAVEHDR *header = &playback.buffers[index].header;
    if (!playback.buffers[index].prepared) return;
    while (wait_for_completion && (header->dwFlags & WHDR_DONE) == 0 &&
           InterlockedCompareExchange(&playback.stop_requested, 0, 0) == 0)
        Sleep(1);
    if ((header->dwFlags & WHDR_DONE) == 0 &&
        InterlockedCompareExchange(&playback.stop_requested, 0, 0) != 0) {
        waveOutReset(playback.output);
        while ((header->dwFlags & WHDR_DONE) == 0) Sleep(1);
    }
    if ((header->dwFlags & WHDR_DONE) != 0) {
        playback.completed_frames += playback.buffers[index].frames;
        InterlockedExchange(&playback.position_ms,
            (LONG)(playback.start_position_ms +
            (playback.completed_frames * 1000ULL) / TRACKER_SAMPLE_RATE));
    }
    waveOutUnprepareHeader(playback.output, header, sizeof(*header));
    free(playback.buffers[index].data);
    ZeroMemory(&playback.buffers[index], sizeof(playback.buffers[index]));
}

static BOOL queue_frames(int16_t *samples, DWORD frames)
{
    int index = playback.next_buffer;
    WAVEHDR *header;
    MMRESULT result;
    release_buffer(index, TRUE);
    if (InterlockedCompareExchange(&playback.stop_requested, 0, 0) != 0) {
        free(samples);
        return FALSE;
    }
    playback.buffers[index].data = samples;
    playback.buffers[index].frames = frames;
    header = &playback.buffers[index].header;
    header->lpData = (LPSTR)samples;
    header->dwBufferLength = frames * 2 * sizeof(*samples);
    result = waveOutPrepareHeader(playback.output, header, sizeof(*header));
    if (result != MMSYSERR_NOERROR) {
        free(samples);
        ZeroMemory(&playback.buffers[index], sizeof(playback.buffers[index]));
        return FALSE;
    }
    playback.buffers[index].prepared = TRUE;
    result = waveOutWrite(playback.output, header, sizeof(*header));
    if (result != MMSYSERR_NOERROR) {
        release_buffer(index, FALSE);
        return FALSE;
    }
    playback.next_buffer = (index + 1) % TRACKER_BUFFER_COUNT;
    return TRUE;
}

static DWORD WINAPI decode_thread(void *unused)
{
    (void)unused;
    while (InterlockedCompareExchange(&playback.stop_requested, 0, 0) == 0) {
        int16_t *samples = (int16_t *)malloc(
            TRACKER_BUFFER_FRAMES * 2 * sizeof(*samples));
        size_t frames;
        if (samples == NULL) break;
        frames = openmpt_module_read_interleaved_stereo(
            playback.tracker.module, TRACKER_SAMPLE_RATE,
            TRACKER_BUFFER_FRAMES, samples);
        if (frames == 0) {
            free(samples);
            break;
        }
        if (!queue_frames(samples, (DWORD)frames)) break;
    }
    if (InterlockedCompareExchange(&playback.stop_requested, 0, 0) != 0)
        waveOutReset(playback.output);
    for (int index = 0; index < TRACKER_BUFFER_COUNT; ++index)
        release_buffer(index, TRUE);
    waveOutClose(playback.output);
    playback.output = NULL;
    tracker_module_close(&playback.tracker);
    InterlockedExchange(&playback.active, 0);
    return 0;
}

static bool playback_openmpt_play_from(const wchar_t *path, int volume_percent,
                                       unsigned long start_ms)
{
    WAVEFORMATEX format = {
        WAVE_FORMAT_PCM, 2, TRACKER_SAMPLE_RATE, TRACKER_SAMPLE_RATE * 4,
        4, 16, 0
    };
    double duration_seconds;
    playback_openmpt_stop();
    ZeroMemory(&playback, sizeof(playback));
    if (!tracker_module_open(path, &playback.tracker)) return false;
    duration_seconds = openmpt_module_get_duration_seconds(playback.tracker.module);
    if (duration_seconds <= 0.0 || duration_seconds >= 4294967.0) goto fail;
    playback.duration_ms = (unsigned long)(duration_seconds * 1000.0 + 0.5);
    if (start_ms >= playback.duration_ms)
        start_ms = playback.duration_ms > 0 ? playback.duration_ms - 1 : 0;
    if (start_ms > 0) {
        double actual = openmpt_module_set_position_seconds(
            playback.tracker.module, start_ms / 1000.0);
        if (actual < 0.0) goto fail;
        playback.start_position_ms = (unsigned long)(actual * 1000.0);
        InterlockedExchange(&playback.position_ms,
                            (LONG)playback.start_position_ms);
    }
    wcscpy_s(playback.path, _countof(playback.path), path);
    if (waveOutOpen(&playback.output, WAVE_MAPPER, &format, 0, 0,
                    CALLBACK_NULL) != MMSYSERR_NOERROR)
        goto fail;
    playback_openmpt_set_volume(volume_percent);
    InterlockedExchange(&playback.active, 1);
    playback.thread = CreateThread(NULL, 0, decode_thread, NULL, 0, NULL);
    if (playback.thread == NULL) goto fail;
    return true;
fail:
    if (playback.output != NULL) {
        waveOutClose(playback.output);
        playback.output = NULL;
    }
    tracker_module_close(&playback.tracker);
    ZeroMemory(&playback, sizeof(playback));
    return false;
}

bool playback_openmpt_play(const wchar_t *path, int volume_percent)
{
    return playback_openmpt_play_from(path, volume_percent, 0);
}

void playback_openmpt_stop(void)
{
    HANDLE thread = playback.thread;
    if (thread == NULL) return;
    InterlockedExchange(&playback.stop_requested, 1);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    playback.thread = NULL;
    InterlockedExchange(&playback.active, 0);
}

bool playback_openmpt_is_active(void)
{
    return InterlockedCompareExchange(&playback.active, 0, 0) != 0;
}

unsigned long playback_openmpt_position_ms(void)
{
    return (unsigned long)InterlockedCompareExchange(&playback.position_ms, 0, 0);
}

unsigned long playback_openmpt_duration_ms(void)
{
    return playback.duration_ms;
}

bool playback_openmpt_seek_ms(unsigned long position_ms)
{
    wchar_t *path;
    bool result;
    if (playback.thread == NULL || playback.path[0] == L'\0') return false;
    path = (wchar_t *)malloc(sizeof(playback.path));
    if (path == NULL) return false;
    wcscpy_s(path, _countof(playback.path), playback.path);
    result = playback_openmpt_play_from(path, playback_volume, position_ms);
    free(path);
    return result;
}

void playback_openmpt_set_volume(int percent)
{
    DWORD value;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    playback_volume = percent;
    value = (DWORD)((percent * 0xffff) / 100);
    if (playback.output != NULL)
        waveOutSetVolume(playback.output, value | (value << 16));
}
