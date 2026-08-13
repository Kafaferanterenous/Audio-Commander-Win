#define COBJMACROS
#include "playback_mf.h"

#include <windows.h>
#include <mfapi.h>
#include <mfplay.h>
#include <propidl.h>

typedef struct MediaPlayerCallback {
    IMFPMediaPlayerCallback interface_value;
    LONG references;
} MediaPlayerCallback;

static IMFPMediaPlayer *media_player;
static volatile LONG playback_finished;
static bool media_foundation_started;
static bool com_initialized;

static HRESULT STDMETHODCALLTYPE callback_query_interface(IMFPMediaPlayerCallback *interface_value,
                                                          REFIID interface_id, void **object)
{
    MediaPlayerCallback *callback = (MediaPlayerCallback *)interface_value;
    if (object == NULL) return E_POINTER;
    *object = NULL;
    if (IsEqualIID(interface_id, &IID_IUnknown) ||
        IsEqualIID(interface_id, &IID_IMFPMediaPlayerCallback)) {
        *object = interface_value;
        InterlockedIncrement(&callback->references);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE callback_add_ref(IMFPMediaPlayerCallback *interface_value)
{
    MediaPlayerCallback *callback = (MediaPlayerCallback *)interface_value;
    return (ULONG)InterlockedIncrement(&callback->references);
}

static ULONG STDMETHODCALLTYPE callback_release(IMFPMediaPlayerCallback *interface_value)
{
    MediaPlayerCallback *callback = (MediaPlayerCallback *)interface_value;
    LONG references = InterlockedDecrement(&callback->references);
    if (references < 1) {
        InterlockedExchange(&callback->references, 1);
        references = 1;
    }
    return (ULONG)references;
}

static void STDMETHODCALLTYPE callback_event(IMFPMediaPlayerCallback *interface_value,
                                             MFP_EVENT_HEADER *event_header)
{
    (void)interface_value;
    if (event_header == NULL || FAILED(event_header->hrEvent) ||
        event_header->eEventType == MFP_EVENT_TYPE_ERROR ||
        event_header->eEventType == MFP_EVENT_TYPE_PLAYBACK_ENDED)
        InterlockedExchange(&playback_finished, 1);
}

static IMFPMediaPlayerCallbackVtbl callback_vtable = {
    callback_query_interface,
    callback_add_ref,
    callback_release,
    callback_event
};

static MediaPlayerCallback callback = {{&callback_vtable}, 1};

static unsigned long time_value_ms(bool duration)
{
    PROPVARIANT value;
    HRESULT result;
    LONGLONG time_100ns = 0;
    if (media_player == NULL) return 0;
    PropVariantInit(&value);
    if (duration) {
        IMFPMediaItem *item = NULL;
        result = IMFPMediaPlayer_GetMediaItem(media_player, &item);
        if (SUCCEEDED(result) && item != NULL) {
            result = IMFPMediaItem_GetDuration(item, &MFP_POSITIONTYPE_100NS, &value);
            IMFPMediaItem_Release(item);
        }
    } else {
        result = IMFPMediaPlayer_GetPosition(media_player, &MFP_POSITIONTYPE_100NS, &value);
    }
    if (SUCCEEDED(result) && value.vt == VT_I8) time_100ns = value.hVal.QuadPart;
    else if (SUCCEEDED(result) && value.vt == VT_UI8) time_100ns = (LONGLONG)value.uhVal.QuadPart;
    PropVariantClear(&value);
    if (time_100ns <= 0) return 0;
    return (unsigned long)(time_100ns / 10000);
}

bool playback_mf_play(const wchar_t *path, int volume_percent)
{
    HRESULT result;
    playback_mf_stop();
    result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    com_initialized = SUCCEEDED(result);
    if (FAILED(result) && result != RPC_E_CHANGED_MODE) return false;
    result = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    if (FAILED(result)) {
        if (com_initialized) CoUninitialize();
        com_initialized = false;
        return false;
    }
    media_foundation_started = true;
    InterlockedExchange(&playback_finished, 0);
    result = MFPCreateMediaPlayer(path, TRUE, MFP_OPTION_NONE,
                                  &callback.interface_value, NULL, &media_player);
    if (FAILED(result) || media_player == NULL) {
        playback_mf_stop();
        return false;
    }
    playback_mf_set_volume(volume_percent);
    return true;
}

void playback_mf_stop(void)
{
    if (media_player != NULL) {
        IMFPMediaPlayer_Shutdown(media_player);
        IMFPMediaPlayer_Release(media_player);
        media_player = NULL;
    }
    InterlockedExchange(&playback_finished, 1);
    if (media_foundation_started) {
        MFShutdown();
        media_foundation_started = false;
    }
    if (com_initialized) {
        CoUninitialize();
        com_initialized = false;
    }
}

bool playback_mf_is_active(void)
{
    return media_player != NULL && InterlockedCompareExchange(&playback_finished, 0, 0) == 0;
}

unsigned long playback_mf_position_ms(void)
{
    return time_value_ms(false);
}

unsigned long playback_mf_duration_ms(void)
{
    return time_value_ms(true);
}

bool playback_mf_seek_ms(unsigned long position_ms)
{
    PROPVARIANT value;
    HRESULT result;
    if (media_player == NULL) return false;
    PropVariantInit(&value);
    value.vt = VT_I8;
    value.hVal.QuadPart = (LONGLONG)position_ms * 10000;
    result = IMFPMediaPlayer_SetPosition(media_player, &MFP_POSITIONTYPE_100NS, &value);
    PropVariantClear(&value);
    if (FAILED(result)) return false;
    InterlockedExchange(&playback_finished, 0);
    return SUCCEEDED(IMFPMediaPlayer_Play(media_player));
}

void playback_mf_set_volume(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    if (media_player != NULL) IMFPMediaPlayer_SetVolume(media_player, (float)percent / 100.0f);
}
