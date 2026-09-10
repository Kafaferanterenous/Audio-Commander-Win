#define COBJMACROS
#include "media_info.h"

#include <windows.h>
#include <shobjidl.h>
#include <propkey.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <stdint.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include "tracker_support.h"

static uint32_t read_u32le(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static unsigned long wav_duration(HANDLE file)
{
    unsigned char header[12];
    DWORD read;
    uint32_t bytes_per_second = 0;
    uint32_t data_size = 0;
    if (!ReadFile(file, header, sizeof(header), &read, NULL) || read != sizeof(header) ||
        memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) return 0;
    for (;;) {
        unsigned char chunk[8];
        uint32_t size;
        if (!ReadFile(file, chunk, sizeof(chunk), &read, NULL) || read != sizeof(chunk)) break;
        size = read_u32le(chunk + 4);
        if (memcmp(chunk, "fmt ", 4) == 0 && size >= 12) {
            unsigned char format[12];
            if (!ReadFile(file, format, sizeof(format), &read, NULL) || read != sizeof(format)) break;
            bytes_per_second = read_u32le(format + 8);
            SetFilePointer(file, (LONG)(size - 12 + (size & 1)), NULL, FILE_CURRENT);
        } else if (memcmp(chunk, "data", 4) == 0) {
            data_size = size;
            break;
        } else {
            SetFilePointer(file, (LONG)(size + (size & 1)), NULL, FILE_CURRENT);
        }
    }
    return bytes_per_second != 0 ? (unsigned long)(((uint64_t)data_size * 1000) / bytes_per_second) : 0;
}

static unsigned long mp3_duration(HANDLE file, unsigned long long file_size)
{
    unsigned char *data = (unsigned char *)malloc(65536);
    DWORD read;
    size_t offset = 0;
    unsigned long duration = 0;
    static const int mpeg1_rates[16] = {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0};
    static const int mpeg2_rates[16] = {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0};
    if (data == NULL) return 0;
    SetFilePointer(file, 0, NULL, FILE_BEGIN);
    if (!ReadFile(file, data, 65536, &read, NULL) || read < 4) goto done;
    if (read >= 10 && memcmp(data, "ID3", 3) == 0) {
        offset = 10 + (((size_t)data[6] & 0x7f) << 21) + (((size_t)data[7] & 0x7f) << 14) +
                 (((size_t)data[8] & 0x7f) << 7) + ((size_t)data[9] & 0x7f);
        if (offset >= read) offset = 10;
    }
    for (; offset + 4 <= read; ++offset) {
        unsigned char b1 = data[offset + 1], b2 = data[offset + 2];
        int version, layer, rate_index, rate;
        if (data[offset] != 0xff || (b1 & 0xe0) != 0xe0) continue;
        version = (b1 >> 3) & 3;
        layer = (b1 >> 1) & 3;
        rate_index = (b2 >> 4) & 15;
        if ((version != 3 && version != 2 && version != 0) || layer != 1 || rate_index == 0 || rate_index == 15) continue;
        rate = version == 3 ? mpeg1_rates[rate_index] : mpeg2_rates[rate_index];
        duration = rate > 0 ? (unsigned long)((file_size * 8) / (unsigned long long)rate) : 0;
        break;
    }
done:
    free(data);
    return duration;
}

static int mp3_bitrate_mode(const wchar_t *path)
{
    unsigned char *data = (unsigned char *)malloc(65536);
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    DWORD read = 0;
    size_t index;
    int mode = 0;
    if (data == NULL) return 0;
    if (file == INVALID_HANDLE_VALUE) {
        free(data);
        return 0;
    }
    if (!ReadFile(file, data, 65536, &read, NULL)) {
        read = 0;
    }
    CloseHandle(file);
    for (index = 0; index + 4 <= read; ++index) {
        if (memcmp(data + index, "Xing", 4) == 0 || memcmp(data + index, "VBRI", 4) == 0) {
            mode = 2;
            break;
        }
        if (memcmp(data + index, "Info", 4) == 0) {
            mode = 1;
            break;
        }
    }
    free(data);
    return mode;
}

static INIT_ONCE ffmpeg_init_once = INIT_ONCE_STATIC_INIT;
static HMODULE ffmpeg_modules[3];
static BOOL ffmpeg_ready;

static BOOL CALLBACK initialize_ffmpeg(PINIT_ONCE once, PVOID parameter, PVOID *context)
{
    static const wchar_t *names[3] = {L"avutil-61.dll", L"avcodec-63.dll", L"avformat-63.dll"};
    int index;
    (void)once;
    (void)parameter;
    (void)context;
    for (index = 0; index < 3; ++index) {
        ffmpeg_modules[index] = LoadLibraryExW(
            names[index], NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
        if (ffmpeg_modules[index] == NULL) {
            while (index > 0) FreeLibrary(ffmpeg_modules[--index]);
            return TRUE;
        }
    }
    ffmpeg_ready = TRUE;
    return TRUE;
}

static BOOL ffmpeg_available(void)
{
    if (!InitOnceExecuteOnce(&ffmpeg_init_once, initialize_ffmpeg, NULL, NULL)) return FALSE;
    return ffmpeg_ready;
}

static void utf8_to_wide(const char *source, wchar_t *destination, size_t count)
{
    if (destination == NULL || count == 0) return;
    destination[0] = L'\0';
    if (source != NULL)
        MultiByteToWideChar(CP_UTF8, 0, source, -1, destination, (int)count);
    destination[count - 1] = L'\0';
}

static void copy_tag(AVDictionary *stream_tags, AVDictionary *format_tags, const char *name,
                     wchar_t *destination, size_t count)
{
    AVDictionaryEntry *entry = av_dict_get(stream_tags, name, NULL, 0);
    if (entry == NULL) entry = av_dict_get(format_tags, name, NULL, 0);
    if (entry != NULL) utf8_to_wide(entry->value, destination, count);
}

static void ffmpeg_media_info(const wchar_t *path, MediaInfo *info)
{
    char *utf8_path = NULL;
    AVFormatContext *format = NULL;
    const AVCodec *decoder = NULL;
    AVStream *stream;
    AVCodecParameters *parameters;
    char layout[128] = {0};
    int utf8_count;
    int index;
    if (!ffmpeg_available()) return;
    utf8_count = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
    if (utf8_count <= 0) return;
    utf8_path = (char *)malloc((size_t)utf8_count);
    if (utf8_path == NULL ||
        WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8_path, utf8_count, NULL, NULL) == 0) {
        free(utf8_path);
        return;
    }
    index = avformat_open_input(&format, utf8_path, NULL, NULL);
    free(utf8_path);
    if (index < 0) return;
    if (avformat_find_stream_info(format, NULL) < 0) goto done;
    index = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
    if (index < 0) goto done;
    stream = format->streams[index];
    parameters = stream->codecpar;
    if (format->duration > 0)
        info->duration_ms = (unsigned long)(format->duration * 1000ULL / AV_TIME_BASE);
    else if (stream->duration > 0)
        info->duration_ms = (unsigned long)(stream->duration * 1000.0 * av_q2d(stream->time_base));
    if (parameters->bit_rate > 0) info->bitrate_kbps = (unsigned long)(parameters->bit_rate / 1000);
    else if (format->bit_rate > 0) info->bitrate_kbps = (unsigned long)(format->bit_rate / 1000);
    info->sample_rate_hz = (unsigned long)parameters->sample_rate;
    info->channels = (unsigned long)parameters->ch_layout.nb_channels;
    info->bit_depth = (unsigned long)(parameters->bits_per_raw_sample > 0 ?
                      parameters->bits_per_raw_sample : parameters->bits_per_coded_sample);
    if (info->bit_depth == 0) info->bit_depth = (unsigned long)av_get_bits_per_sample(parameters->codec_id);
    utf8_to_wide(format->iformat != NULL && format->iformat->long_name != NULL ?
                 format->iformat->long_name : format->iformat != NULL ? format->iformat->name : NULL,
                 info->container, _countof(info->container));
    utf8_to_wide(avcodec_get_name(parameters->codec_id), info->codec, _countof(info->codec));
    if (av_channel_layout_describe(&parameters->ch_layout, layout, sizeof(layout)) >= 0)
        utf8_to_wide(layout, info->channel_layout, _countof(info->channel_layout));
    copy_tag(stream->metadata, format->metadata, "title", info->title, _countof(info->title));
    copy_tag(stream->metadata, format->metadata, "artist", info->artist, _countof(info->artist));
    copy_tag(stream->metadata, format->metadata, "album", info->album, _countof(info->album));
done:
    avformat_close_input(&format);
}

static void shell_media_info(const wchar_t *path, MediaInfo *info)
{
    IShellItem2 *item = NULL;
    HRESULT com_result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    BOOL uninitialize = SUCCEEDED(com_result);
    if (SUCCEEDED(SHCreateItemFromParsingName(path, NULL, &IID_IShellItem2, (void **)&item))) {
        ULONGLONG duration = 0;
        ULONG value = 0;
        if (SUCCEEDED(IShellItem2_GetUInt64(item, &PKEY_Media_Duration, &duration)))
            info->duration_ms = (unsigned long)(duration / 10000ULL);
        if (SUCCEEDED(IShellItem2_GetUInt32(item, &PKEY_Audio_EncodingBitrate, &value)))
            info->bitrate_kbps = value / 1000;
        if (SUCCEEDED(IShellItem2_GetUInt32(item, &PKEY_Audio_SampleRate, &value)))
            info->sample_rate_hz = value;
        if (SUCCEEDED(IShellItem2_GetUInt32(item, &PKEY_Audio_ChannelCount, &value)))
            info->channels = value;
        IShellItem2_Release(item);
    }
    if (uninitialize) CoUninitialize();
}

void media_read_info(const wchar_t *path, MediaInfo *info)
{
    HANDLE file;
    LARGE_INTEGER size;
    const wchar_t *extension = wcsrchr(path, L'.');
    if (info == NULL) return;
    ZeroMemory(info, sizeof(*info));
    if (tracker_is_path(path)) {
        tracker_read_info(path, info);
        return;
    }
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        if (!GetFileSizeEx(file, &size)) size.QuadPart = 0;
        if (extension != NULL && _wcsicmp(extension, L".wav") == 0)
            info->duration_ms = wav_duration(file);
        else if (extension != NULL && _wcsicmp(extension, L".mp3") == 0)
            info->duration_ms = mp3_duration(file, (unsigned long long)size.QuadPart);
        CloseHandle(file);
    }
    shell_media_info(path, info);
    ffmpeg_media_info(path, info);
    if (extension != NULL && _wcsicmp(extension, L".mp3") == 0) {
        int mode = mp3_bitrate_mode(path);
        wcscpy_s(info->bitrate_mode, _countof(info->bitrate_mode),
                 mode == 2 ? L"Variable" : mode == 1 ? L"Constant" : L"Unknown");
    } else if (info->bitrate_kbps > 0)
        wcscpy_s(info->bitrate_mode, _countof(info->bitrate_mode), L"Reported average");
}

unsigned long media_duration_ms(const wchar_t *path)
{
    MediaInfo info;
    media_read_info(path, &info);
    return info.duration_ms;
}
