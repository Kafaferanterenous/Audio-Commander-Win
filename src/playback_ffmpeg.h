#ifndef AUDIOCOMMANDER_PLAYBACK_FFMPEG_H
#define AUDIOCOMMANDER_PLAYBACK_FFMPEG_H

#include <stdbool.h>
#include <wchar.h>

bool playback_ffmpeg_play(const wchar_t *path, int volume_percent);
void playback_ffmpeg_stop(void);
bool playback_ffmpeg_is_active(void);
unsigned long playback_ffmpeg_position_ms(void);
unsigned long playback_ffmpeg_duration_ms(void);
bool playback_ffmpeg_seek_ms(unsigned long position_ms);
void playback_ffmpeg_set_volume(int percent);

#endif
