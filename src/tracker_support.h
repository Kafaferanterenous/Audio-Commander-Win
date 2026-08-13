#ifndef AUDIOCOMMANDER_TRACKER_SUPPORT_H
#define AUDIOCOMMANDER_TRACKER_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <wchar.h>

#include <libopenmpt/libopenmpt.h>

#include "media_info.h"

typedef struct TrackerModule {
    openmpt_module *module;
    void *file_data;
    size_t file_size;
} TrackerModule;

bool tracker_is_path(const wchar_t *path);
bool tracker_module_open(const wchar_t *path, TrackerModule *tracker);
void tracker_module_close(TrackerModule *tracker);
bool tracker_read_info(const wchar_t *path, MediaInfo *info);

#endif
