#ifndef AUDIOCOMMANDER_SKIN_H
#define AUDIOCOMMANDER_SKIN_H

#include <windows.h>
#include <stdbool.h>

typedef struct ThemeColors {
    COLORREF window;
    COLORREF control;
    COLORREF text;
    COLORREF selection;
    COLORREF selection_text;
    COLORREF border;
    COLORREF hot_border;
    COLORREF button_top;
    COLORREF button_bottom;
    COLORREF summary;
    COLORREF summary_text;
    COLORREF progress;
} ThemeColors;

bool skin_load_file(const wchar_t *path, ThemeColors *colors);

#endif
