#ifndef AUDIOCOMMANDER_SETTINGS_H
#define AUDIOCOMMANDER_SETTINGS_H

#include <windows.h>
#include <stdbool.h>
#include <stddef.h>

#include "skin.h"

typedef enum AppTheme {
    APP_THEME_PASTEL,
    APP_THEME_WINDOWS_NATIVE,
    APP_THEME_ACRYL,
    APP_THEME_AIR,
    APP_THEME_METRO_UI,
    APP_THEME_OFFICE_2003_SKIN,
    APP_THEME_OFFICE_2007_BLACK,
    APP_THEME_XP_LUNA,
    APP_THEME_XP_SILVER,
    APP_THEME_ZEST,
    APP_THEME_COUNT
} AppTheme;

typedef enum AppLanguage {
    APP_LANGUAGE_ENGLISH,
    APP_LANGUAGE_CHINESE_SIMPLIFIED,
    APP_LANGUAGE_ITALIAN,
    APP_LANGUAGE_POLISH,
    APP_LANGUAGE_COUNT
} AppLanguage;

typedef struct AppSettings {
    wchar_t font_face[LF_FACESIZE];
    int font_points;
    BOOL monospace;
    int volume;
    BOOL sequential_playback;
    int interface_scale;
    int opacity;
    BOOL remember_window;
    BOOL remember_directories;
    int window_x;
    int window_y;
    int window_width;
    int window_height;
    AppTheme theme;
    AppLanguage language;
} AppSettings;

void settings_load(AppSettings *settings);
void settings_save(const AppSettings *settings);
bool settings_ini_path(wchar_t *path, size_t count);
bool settings_first_run_accepted(const wchar_t *path);
bool settings_record_first_run_acceptance(const wchar_t *path);
bool settings_load_last_directories(wchar_t *left, size_t left_count,
                                    wchar_t *right, size_t right_count);
bool settings_save_last_directories(const wchar_t *left, const wchar_t *right);
bool settings_clear_last_directories(void);
HFONT settings_create_font(const AppSettings *settings, HWND dpi_window);
ThemeColors settings_theme_colors(AppTheme theme);
const wchar_t *settings_theme_name(AppTheme theme);
bool settings_theme_is_external(AppTheme theme);
bool settings_theme_available(AppTheme theme);
const wchar_t *settings_language_name(AppLanguage language);

#endif
