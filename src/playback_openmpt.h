#ifndef AUDIOCOMMANDER_PLAYBACK_OPENMPT_H
#define AUDIOCOMMANDER_PLAYBACK_OPENMPT_H

#include <stdbool.h>
#include <wchar.h>

bool playback_openmpt_play(const wchar_t *path, int volume_percent);
void playback_openmpt_stop(void);
bool playback_openmpt_is_active(void);
unsigned long playback_openmpt_position_ms(void);
unsigned long playback_openmpt_duration_ms(void);
bool playback_openmpt_seek_ms(unsigned long position_ms);
void playback_openmpt_set_volume(int percent);

#endif
