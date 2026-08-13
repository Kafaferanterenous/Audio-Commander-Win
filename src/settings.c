#include "settings.h"

#include <wchar.h>

typedef struct ExternalThemeDefinition {
    const wchar_t *name;
    const wchar_t *file_name;
} ExternalThemeDefinition;

static const ExternalThemeDefinition external_themes[] = {
    {L"Acryl", L"Acryl.skn"},
    {L"Air", L"Air.skn"},
    {L"MetroUI", L"MetroUI.skn"},
    {L"Office 2003", L"Office2003.skn"},
    {L"Office 2007 Black", L"Office2007 Black.skn"},
    {L"XP Luna", L"XPLuna.skn"},
    {L"XP Silver", L"XPSilver.skn"},
    {L"Zest", L"Zest.skn"}
};

static ThemeColors pastel_colors(void)
{
    return (ThemeColors){
        RGB(244,235,241), RGB(255,250,244), RGB(72,62,70),
        RGB(183,205,184), RGB(35,45,36), RGB(181,158,174),
        RGB(151,178,153), RGB(255,252,248), RGB(232,220,228),
        RGB(238,224,234), RGB(72,62,70), RGB(151,178,153)};
}

bool settings_theme_is_external(AppTheme theme)
{
    return theme >= APP_THEME_ACRYL && theme < APP_THEME_COUNT;
}

static const ExternalThemeDefinition *external_theme_definition(AppTheme theme)
{
    size_t index;
    if (!settings_theme_is_external(theme)) return NULL;
    index = (size_t)(theme - APP_THEME_ACRYL);
    return index < ARRAYSIZE(external_themes) ? &external_themes[index] : NULL;
}

static bool settings_skin_path(AppTheme theme, wchar_t *path, size_t count)
{
    const ExternalThemeDefinition *definition = external_theme_definition(theme);
    wchar_t *slash;
    DWORD length;
    if (definition == NULL || path == NULL || count < 2 || count > MAXDWORD)
        return false;
    length = GetModuleFileNameW(NULL, path, (DWORD)count);
    if (length == 0 || length >= count) return false;
    slash = wcsrchr(path, L'\\');
    if (slash == NULL) return false;
    return swprintf_s(slash + 1, count - (size_t)(slash + 1 - path),
                      L"skins\\%ls", definition->file_name) > 0;
}

bool settings_theme_available(AppTheme theme)
{
    wchar_t path[MAX_PATH];
    ThemeColors colors;
    if (theme == APP_THEME_PASTEL || theme == APP_THEME_WINDOWS_NATIVE)
        return true;
    return settings_skin_path(theme, path, ARRAYSIZE(path)) &&
           skin_load_file(path, &colors);
}

bool settings_ini_path(wchar_t *path, size_t count)
{
    wchar_t *slash;
    DWORD length;
    if (path == NULL || count == 0 || count > MAXDWORD) return false;
    length = GetModuleFileNameW(NULL, path, (DWORD)count);
    if (length == 0 || length >= count) return false;
    slash = wcsrchr(path, L'\\');
    if (slash == NULL) return false;
    return wcscpy_s(slash + 1, count - (size_t)(slash + 1 - path),
                    L"audiocommander.ini") == 0;
}

bool settings_first_run_accepted(const wchar_t *path)
{
    wchar_t value[8];
    if (path == NULL || path[0] == L'\0') return false;
    if (GetPrivateProfileStringW(L"Legal", L"Accepted", L"", value,
                                 ARRAYSIZE(value), path) != 1)
        return false;
    return wcscmp(value, L"1") == 0;
}

bool settings_record_first_run_acceptance(const wchar_t *path)
{
    if (path == NULL || path[0] == L'\0') return false;
    if (!WritePrivateProfileStringW(L"Legal", L"Accepted", L"1", path)) return false;
    WritePrivateProfileStringW(NULL, NULL, NULL, path);
    return settings_first_run_accepted(path);
}

static bool load_directory_value(const wchar_t *ini_path, const wchar_t *key,
                                 wchar_t *value, size_t count)
{
    DWORD length;
    DWORD capacity;
    if (ini_path == NULL || key == NULL || value == NULL || count < 2 ||
        count > MAXDWORD)
        return false;
    value[0] = L'\0';
    capacity = (DWORD)(count > 32767 ? 32767 : count);
    length = GetPrivateProfileStringW(L"Folders", key, L"", value,
                                      capacity, ini_path);
    if (length == 0 || length >= capacity - 1) {
        value[0] = L'\0';
        return false;
    }
    return true;
}

bool settings_load_last_directories(wchar_t *left, size_t left_count,
                                    wchar_t *right, size_t right_count)
{
    wchar_t path[MAX_PATH];
    bool left_loaded, right_loaded;
    if (left == NULL || right == NULL || left_count < 2 || right_count < 2 ||
        left_count > MAXDWORD || right_count > MAXDWORD)
        return false;
    left[0] = L'\0';
    right[0] = L'\0';
    if (!settings_ini_path(path, ARRAYSIZE(path))) return false;
    left_loaded = load_directory_value(path, L"Left", left, left_count);
    right_loaded = load_directory_value(path, L"Right", right, right_count);
    return left_loaded || right_loaded;
}

bool settings_save_last_directories(const wchar_t *left, const wchar_t *right)
{
    wchar_t path[MAX_PATH];
    if (left == NULL || left[0] == L'\0' || right == NULL || right[0] == L'\0' ||
        !settings_ini_path(path, ARRAYSIZE(path)))
        return false;
    if (!WritePrivateProfileStringW(L"Folders", L"Left", left, path) ||
        !WritePrivateProfileStringW(L"Folders", L"Right", right, path))
        return false;
    WritePrivateProfileStringW(NULL, NULL, NULL, path);
    return true;
}

bool settings_clear_last_directories(void)
{
    wchar_t path[MAX_PATH];
    bool left_cleared, right_cleared;
    if (!settings_ini_path(path, ARRAYSIZE(path))) return false;
    left_cleared = WritePrivateProfileStringW(L"Folders", L"Left", NULL, path) != FALSE;
    right_cleared = WritePrivateProfileStringW(L"Folders", L"Right", NULL, path) != FALSE;
    WritePrivateProfileStringW(NULL, NULL, NULL, path);
    return left_cleared && right_cleared;
}

const wchar_t *settings_theme_name(AppTheme theme)
{
    static const wchar_t *names[APP_THEME_COUNT] = {
        L"Pastel", L"Windows Native", L"Acryl", L"Air", L"MetroUI",
        L"Office 2003", L"Office 2007 Black", L"XP Luna", L"XP Silver",
        L"Zest"
    };
    return theme >= 0 && theme < APP_THEME_COUNT ? names[theme] : names[APP_THEME_PASTEL];
}

const wchar_t *settings_language_name(AppLanguage language)
{
    static const wchar_t *names[APP_LANGUAGE_COUNT] = {
        L"English", L"\x4E2D\x6587\xFF08\x7B80\x4F53\xFF09", L"Italiano", L"Polski"
    };
    return language >= 0 && language < APP_LANGUAGE_COUNT ? names[language] : names[APP_LANGUAGE_ENGLISH];
}

void settings_load(AppSettings *settings)
{
    wchar_t path[MAX_PATH];
    LOGFONTW system_font;
    int dpi = 96;
    GetObjectW(GetStockObject(DEFAULT_GUI_FONT), sizeof(system_font), &system_font);
    wcscpy_s(settings->font_face, ARRAYSIZE(settings->font_face), system_font.lfFaceName);
    settings->font_points = MulDiv(-system_font.lfHeight, 72, dpi) + 2;
    if (settings->font_points < 8) settings->font_points = 11;
    settings->theme = APP_THEME_PASTEL;
    settings->language = APP_LANGUAGE_ENGLISH;
    settings->monospace = FALSE;
    settings->volume = 75;
    settings->sequential_playback = FALSE;
    settings->interface_scale = 100;
    settings->opacity = 100;
    settings->remember_window = FALSE;
    settings->remember_directories = FALSE;
    settings->window_x = CW_USEDEFAULT;
    settings->window_y = CW_USEDEFAULT;
    settings->window_width = 0;
    settings->window_height = 0;
    if (!settings_ini_path(path, ARRAYSIZE(path))) return;
    GetPrivateProfileStringW(L"Appearance", L"FontFace", settings->font_face,
                             settings->font_face, ARRAYSIZE(settings->font_face), path);
    settings->font_points = GetPrivateProfileIntW(L"Appearance", L"FontSize",
                                                    settings->font_points, path);
    {
        int stored_theme = GetPrivateProfileIntW(
            L"Appearance", L"Theme", settings->theme, path);
        int version = GetPrivateProfileIntW(L"Appearance", L"Version", 1, path);
        if (version == 4) {
            switch (stored_theme) {
            case 5: stored_theme = APP_THEME_WINDOWS_NATIVE; break;
            case 6: stored_theme = APP_THEME_OFFICE_2003_SKIN; break;
            default: stored_theme = APP_THEME_PASTEL; break;
            }
        } else if (version < 4) {
            stored_theme = APP_THEME_PASTEL;
        }
        settings->theme = (AppTheme)stored_theme;
    }
    settings->language = (AppLanguage)GetPrivateProfileIntW(
        L"Appearance", L"Language", APP_LANGUAGE_ENGLISH, path);
    settings->monospace = GetPrivateProfileIntW(L"Appearance", L"Monospace", 0, path) != 0;
    settings->volume = GetPrivateProfileIntW(L"Playback", L"Volume", 75, path);
    settings->sequential_playback = GetPrivateProfileIntW(
        L"Playback", L"Sequential", 0, path) != 0;
    settings->interface_scale = GetPrivateProfileIntW(L"Appearance", L"InterfaceSize", 100, path);
    settings->opacity = GetPrivateProfileIntW(L"Appearance", L"Opacity", 100, path);
    settings->remember_window = GetPrivateProfileIntW(L"Window", L"RememberPosition", 0, path) != 0;
    settings->remember_directories = GetPrivateProfileIntW(
        L"Folders", L"Remember", 0, path) != 0;
    settings->window_x = GetPrivateProfileIntW(L"Window", L"X", CW_USEDEFAULT, path);
    settings->window_y = GetPrivateProfileIntW(L"Window", L"Y", CW_USEDEFAULT, path);
    settings->window_width = GetPrivateProfileIntW(L"Window", L"Width", 0, path);
    settings->window_height = GetPrivateProfileIntW(L"Window", L"Height", 0, path);
    if (settings->font_points < 6 || settings->font_points > 72) settings->font_points = 11;
    if (settings->theme < 0 || settings->theme >= APP_THEME_COUNT ||
        !settings_theme_available(settings->theme))
        settings->theme = APP_THEME_PASTEL;
    if (settings->language < 0 || settings->language >= APP_LANGUAGE_COUNT)
        settings->language = APP_LANGUAGE_ENGLISH;
    if (settings->volume < 0 || settings->volume > 100) settings->volume = 75;
    if (settings->interface_scale < 75 || settings->interface_scale > 200) settings->interface_scale = 100;
    if (settings->opacity < 50 || settings->opacity > 100) settings->opacity = 100;
}

void settings_save(const AppSettings *settings)
{
    wchar_t path[MAX_PATH];
    wchar_t number[16];
    if (!settings_ini_path(path, ARRAYSIZE(path))) return;
    WritePrivateProfileStringW(L"Appearance", L"Version", L"5", path);
    WritePrivateProfileStringW(L"Appearance", L"FontFace", settings->font_face, path);
    swprintf_s(number, ARRAYSIZE(number), L"%d", settings->font_points);
    WritePrivateProfileStringW(L"Appearance", L"FontSize", number, path);
    swprintf_s(number, ARRAYSIZE(number), L"%d", (int)settings->theme);
    WritePrivateProfileStringW(L"Appearance", L"Theme", number, path);
    swprintf_s(number, ARRAYSIZE(number), L"%d", (int)settings->language);
    WritePrivateProfileStringW(L"Appearance", L"Language", number, path);
    WritePrivateProfileStringW(L"Appearance", L"Monospace", settings->monospace ? L"1" : L"0", path);
    swprintf_s(number, ARRAYSIZE(number), L"%d", settings->interface_scale);
    WritePrivateProfileStringW(L"Appearance", L"InterfaceSize", number, path);
    swprintf_s(number, ARRAYSIZE(number), L"%d", settings->opacity);
    WritePrivateProfileStringW(L"Appearance", L"Opacity", number, path);
    swprintf_s(number, ARRAYSIZE(number), L"%d", settings->volume);
    WritePrivateProfileStringW(L"Playback", L"Volume", number, path);
    WritePrivateProfileStringW(L"Playback", L"Sequential",
                               settings->sequential_playback ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"Window", L"RememberPosition", settings->remember_window ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"Folders", L"Remember",
                               settings->remember_directories ? L"1" : L"0", path);
    swprintf_s(number, ARRAYSIZE(number), L"%d", settings->window_x);
    WritePrivateProfileStringW(L"Window", L"X", number, path);
    swprintf_s(number, ARRAYSIZE(number), L"%d", settings->window_y);
    WritePrivateProfileStringW(L"Window", L"Y", number, path);
    swprintf_s(number, ARRAYSIZE(number), L"%d", settings->window_width);
    WritePrivateProfileStringW(L"Window", L"Width", number, path);
    swprintf_s(number, ARRAYSIZE(number), L"%d", settings->window_height);
    WritePrivateProfileStringW(L"Window", L"Height", number, path);
    WritePrivateProfileStringW(NULL, NULL, NULL, path);
}

HFONT settings_create_font(const AppSettings *settings, HWND dpi_window)
{
    HDC dc = GetDC(dpi_window);
    int dpi = dc != NULL ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    LOGFONTW font = {0};
    if (dc != NULL) ReleaseDC(dpi_window, dc);
    font.lfHeight = -MulDiv(settings->font_points, dpi, 72);
    font.lfWeight = FW_NORMAL;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(font.lfFaceName, ARRAYSIZE(font.lfFaceName),
             settings->monospace ? L"Consolas" : settings->font_face);
    return CreateFontIndirectW(&font);
}

ThemeColors settings_theme_colors(AppTheme theme)
{
    ThemeColors colors = pastel_colors();
    wchar_t path[MAX_PATH];
    if (settings_theme_is_external(theme)) {
        if (settings_skin_path(theme, path, ARRAYSIZE(path)))
            skin_load_file(path, &colors);
        return colors;
    }
    switch (theme) {
    case APP_THEME_PASTEL:
        break;
    case APP_THEME_WINDOWS_NATIVE:
        colors = (ThemeColors){
            GetSysColor(COLOR_BTNFACE), GetSysColor(COLOR_WINDOW),
            GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_HIGHLIGHT),
            GetSysColor(COLOR_HIGHLIGHTTEXT), GetSysColor(COLOR_3DSHADOW),
            GetSysColor(COLOR_HOTLIGHT), GetSysColor(COLOR_BTNHIGHLIGHT),
            GetSysColor(COLOR_BTNFACE), GetSysColor(COLOR_BTNFACE),
            GetSysColor(COLOR_BTNTEXT), GetSysColor(COLOR_HIGHLIGHT)}; break;
    default:
        break;
    }
    return colors;
}
