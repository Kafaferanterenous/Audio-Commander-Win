#ifndef AUDIOCOMMANDER_PLAYBACK_MF_H
#define AUDIOCOMMANDER_PLAYBACK_MF_H

#include <stdbool.h>
#include <wchar.h>

bool playback_mf_play(const wchar_t *path, int volume_percent);
void playback_mf_stop(void);
bool playback_mf_is_active(void);
unsigned long playback_mf_position_ms(void);
unsigned long playback_mf_duration_ms(void);
bool playback_mf_seek_ms(unsigned long position_ms);
void playback_mf_set_volume(int percent);

#endif
