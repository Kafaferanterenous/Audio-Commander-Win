#ifndef AUDIOCOMMANDER_PLAYBACK_H
#define AUDIOCOMMANDER_PLAYBACK_H

#include <stdbool.h>
#include <wchar.h>

bool playback_play(const wchar_t *path);
void playback_stop(void);
bool playback_is_active(void);
unsigned long playback_position_ms(void);
unsigned long playback_duration_ms(void);
bool playback_seek_ms(unsigned long position_ms);
void playback_set_volume(int percent);

#endif
