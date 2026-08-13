#include "playback_ffmpeg.h"

#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

typedef struct FfmpegPlayback {
    AVFormatContext *format;
    AVCodecContext *codec;
    SwrContext *resampler;
    AVPacket *packet;
    AVFrame *frame;
    HANDLE thread;
    HWAVEOUT output;
    int stream_index;
    volatile LONG stop_requested;
    volatile LONG active;
    volatile LONG position_ms;
    unsigned long duration_ms;
    wchar_t path[32768];
    struct {
        WAVEHDR header;
        uint8_t *data;
        DWORD samples;
        BOOL prepared;
    } wave_buffers[6];
    int next_wave_buffer;
} FfmpegPlayback;

static FfmpegPlayback playback;
static int playback_volume = 75;
static HMODULE ffmpeg_modules[4];

static BOOL ffmpeg_available(void)
{
    static const wchar_t *names[4] = {
        L"avutil-61.dll", L"swresample-7.dll", L"avcodec-63.dll", L"avformat-63.dll"
    };
    int index;
    if (ffmpeg_modules[3] != NULL) return TRUE;
    for (index = 0; index < 4; ++index) {
        ffmpeg_modules[index] = LoadLibraryExW(names[index], NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
        if (ffmpeg_modules[index] == NULL) return FALSE;
    }
    return TRUE;
}

static void release_decoder(void)
{
    if (playback.packet != NULL) av_packet_free(&playback.packet);
    if (playback.frame != NULL) av_frame_free(&playback.frame);
    if (playback.resampler != NULL) swr_free(&playback.resampler);
    if (playback.codec != NULL) avcodec_free_context(&playback.codec);
    if (playback.format != NULL) avformat_close_input(&playback.format);
}

static void release_wave_buffer(int index, BOOL wait_for_completion)
{
    WAVEHDR *header = &playback.wave_buffers[index].header;
    if (!playback.wave_buffers[index].prepared) return;
    while (wait_for_completion && (header->dwFlags & WHDR_DONE) == 0 &&
           InterlockedCompareExchange(&playback.stop_requested, 0, 0) == 0) Sleep(1);
    if ((header->dwFlags & WHDR_DONE) == 0 &&
        InterlockedCompareExchange(&playback.stop_requested, 0, 0) != 0) {
        waveOutReset(playback.output);
        while ((header->dwFlags & WHDR_DONE) == 0) Sleep(1);
    }
    if ((header->dwFlags & WHDR_DONE) != 0)
        InterlockedExchangeAdd(&playback.position_ms,
            (LONG)((playback.wave_buffers[index].samples * 1000ULL) / 44100ULL));
    waveOutUnprepareHeader(playback.output, header, sizeof(*header));
    av_free(playback.wave_buffers[index].data);
    ZeroMemory(&playback.wave_buffers[index], sizeof(playback.wave_buffers[index]));
}

static BOOL queue_pcm(uint8_t *data, DWORD byte_count, DWORD samples)
{
    int index = playback.next_wave_buffer;
    WAVEHDR *header;
    MMRESULT result;
    release_wave_buffer(index, TRUE);
    if (InterlockedCompareExchange(&playback.stop_requested, 0, 0) != 0) {
        av_free(data);
        return FALSE;
    }
    playback.wave_buffers[index].data = data;
    playback.wave_buffers[index].samples = samples;
    header = &playback.wave_buffers[index].header;
    header->lpData = (LPSTR)data;
    header->dwBufferLength = byte_count;
    result = waveOutPrepareHeader(playback.output, header, sizeof(*header));
    if (result != MMSYSERR_NOERROR) {
        av_free(data);
        ZeroMemory(&playback.wave_buffers[index], sizeof(playback.wave_buffers[index]));
        return FALSE;
    }
    playback.wave_buffers[index].prepared = TRUE;
    result = waveOutWrite(playback.output, header, sizeof(*header));
    if (result != MMSYSERR_NOERROR) {
        release_wave_buffer(index, FALSE);
        return FALSE;
    }
    playback.next_wave_buffer = (index + 1) % ARRAYSIZE(playback.wave_buffers);
    return TRUE;
}

static BOOL drain_frames(void)
{
    for (;;) {
        int result = avcodec_receive_frame(playback.codec, playback.frame);
        int output_samples;
        int converted;
        uint8_t *buffer;
        uint8_t *planes[1];
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) return TRUE;
        if (result < 0) return FALSE;
        output_samples = swr_get_out_samples(playback.resampler, playback.frame->nb_samples);
        buffer = (uint8_t *)av_malloc((size_t)output_samples * 4);
        if (buffer == NULL) return FALSE;
        planes[0] = buffer;
        converted = swr_convert(playback.resampler, planes, output_samples,
                                (const uint8_t **)playback.frame->extended_data,
                                playback.frame->nb_samples);
        av_frame_unref(playback.frame);
        if (converted < 0) {
            av_free(buffer);
            return FALSE;
        }
        if (!queue_pcm(buffer, (DWORD)converted * 4, (DWORD)converted)) return FALSE;
        if (InterlockedCompareExchange(&playback.stop_requested, 0, 0) != 0) return FALSE;
    }
}

static DWORD WINAPI decode_thread(void *unused)
{
    (void)unused;
    while (InterlockedCompareExchange(&playback.stop_requested, 0, 0) == 0 &&
           av_read_frame(playback.format, playback.packet) >= 0) {
        if (playback.packet->stream_index == playback.stream_index &&
            avcodec_send_packet(playback.codec, playback.packet) >= 0 && !drain_frames()) {
            av_packet_unref(playback.packet);
            break;
        }
        av_packet_unref(playback.packet);
    }
    if (InterlockedCompareExchange(&playback.stop_requested, 0, 0) == 0) {
        avcodec_send_packet(playback.codec, NULL);
        drain_frames();
    }
    if (InterlockedCompareExchange(&playback.stop_requested, 0, 0) != 0) waveOutReset(playback.output);
    for (int index = 0; index < ARRAYSIZE(playback.wave_buffers); ++index)
        release_wave_buffer(index, TRUE);
    waveOutClose(playback.output);
    playback.output = NULL;
    release_decoder();
    InterlockedExchange(&playback.active, 0);
    return 0;
}

static bool playback_ffmpeg_play_from(const wchar_t *path, int volume_percent,
                                      unsigned long start_ms)
{
    char *utf8_path = NULL;
    const AVCodec *decoder;
    AVChannelLayout output_layout = AV_CHANNEL_LAYOUT_STEREO;
    WAVEFORMATEX wave_format = {WAVE_FORMAT_PCM, 2, 44100, 44100 * 4, 4, 16, 0};
    int utf8_count;
    int open_result;
    int stream;
    playback_ffmpeg_stop();
    ZeroMemory(&playback, sizeof(playback));
    if (!ffmpeg_available()) return false;
    utf8_count = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
    if (utf8_count <= 0) return false;
    utf8_path = (char *)malloc((size_t)utf8_count);
    if (utf8_path == NULL ||
        WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8_path, utf8_count, NULL, NULL) == 0) {
        free(utf8_path);
        return false;
    }
    open_result = avformat_open_input(&playback.format, utf8_path, NULL, NULL);
    free(utf8_path);
    if (open_result < 0 ||
        avformat_find_stream_info(playback.format, NULL) < 0) goto fail;
    stream = av_find_best_stream(playback.format, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
    if (stream < 0 || decoder == NULL) goto fail;
    playback.stream_index = stream;
    playback.codec = avcodec_alloc_context3(decoder);
    if (playback.codec == NULL ||
        avcodec_parameters_to_context(playback.codec, playback.format->streams[stream]->codecpar) < 0 ||
        avcodec_open2(playback.codec, decoder, NULL) < 0) goto fail;
    if (swr_alloc_set_opts2(&playback.resampler, &output_layout, AV_SAMPLE_FMT_S16, 44100,
                            &playback.codec->ch_layout, playback.codec->sample_fmt,
                            playback.codec->sample_rate, 0, NULL) < 0 ||
        playback.resampler == NULL || swr_init(playback.resampler) < 0) goto fail;
    playback.packet = av_packet_alloc();
    playback.frame = av_frame_alloc();
    if (playback.packet == NULL || playback.frame == NULL) goto fail;
    if (playback.format->duration > 0)
        playback.duration_ms = (unsigned long)(playback.format->duration * 1000ULL / AV_TIME_BASE);
    if (start_ms > 0) {
        AVRational milliseconds = {1, 1000};
        int64_t timestamp;
        if (playback.duration_ms > 0 && start_ms >= playback.duration_ms)
            start_ms = playback.duration_ms - 1;
        timestamp = av_rescale_q((int64_t)start_ms, milliseconds,
                                 playback.format->streams[stream]->time_base);
        if (av_seek_frame(playback.format, stream, timestamp, AVSEEK_FLAG_BACKWARD) < 0)
            goto fail;
        avcodec_flush_buffers(playback.codec);
        InterlockedExchange(&playback.position_ms, (LONG)start_ms);
    }
    wcscpy_s(playback.path, ARRAYSIZE(playback.path), path);
    if (waveOutOpen(&playback.output, WAVE_MAPPER, &wave_format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) goto fail;
    playback_ffmpeg_set_volume(volume_percent);
    InterlockedExchange(&playback.active, 1);
    playback.thread = CreateThread(NULL, 0, decode_thread, NULL, 0, NULL);
    if (playback.thread == NULL) goto fail;
    return true;
fail:
    if (playback.output != NULL) {
        waveOutClose(playback.output);
        playback.output = NULL;
    }
    release_decoder();
    ZeroMemory(&playback, sizeof(playback));
    return false;
}

bool playback_ffmpeg_play(const wchar_t *path, int volume_percent)
{
    return playback_ffmpeg_play_from(path, volume_percent, 0);
}

void playback_ffmpeg_stop(void)
{
    HANDLE thread = playback.thread;
    if (thread == NULL) return;
    InterlockedExchange(&playback.stop_requested, 1);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    playback.thread = NULL;
    InterlockedExchange(&playback.active, 0);
}

bool playback_ffmpeg_is_active(void)
{
    return InterlockedCompareExchange(&playback.active, 0, 0) != 0;
}

unsigned long playback_ffmpeg_position_ms(void)
{
    return (unsigned long)InterlockedCompareExchange(&playback.position_ms, 0, 0);
}

unsigned long playback_ffmpeg_duration_ms(void)
{
    return playback.duration_ms;
}

bool playback_ffmpeg_seek_ms(unsigned long position_ms)
{
    wchar_t *path;
    bool result;
    if (playback.thread == NULL || playback.path[0] == L'\0') return false;
    path = (wchar_t *)malloc(sizeof(playback.path));
    if (path == NULL) return false;
    wcscpy_s(path, ARRAYSIZE(playback.path), playback.path);
    result = playback_ffmpeg_play_from(path, playback_volume, position_ms);
    free(path);
    return result;
}

void playback_ffmpeg_set_volume(int percent)
{
    DWORD value;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    playback_volume = percent;
    value = (DWORD)((percent * 0xffff) / 100);
    if (playback.output != NULL) waveOutSetVolume(playback.output, value | (value << 16));
}
