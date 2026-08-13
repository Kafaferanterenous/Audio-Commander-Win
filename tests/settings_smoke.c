#include "settings.h"

#include <windows.h>
#include <stdio.h>
#include <wchar.h>

int wmain(void)
{
    int index;
    COLORREF previous = CLR_INVALID;
    wchar_t temp_folder[MAX_PATH], temp_ini[MAX_PATH], value[16];
    if (GetTempPathW(ARRAYSIZE(temp_folder), temp_folder) == 0 ||
        GetTempFileNameW(temp_folder, L"acs", 0, temp_ini) == 0)
        return 7;
    DeleteFileW(temp_ini);
    if (settings_first_run_accepted(temp_ini) ||
        settings_first_run_accepted(NULL) ||
        settings_record_first_run_acceptance(NULL) ||
        settings_record_first_run_acceptance(L"?:\\AudioCommander\\unwritable.ini"))
        return 8;
    WritePrivateProfileStringW(L"Legal", L"Accepted", L"2", temp_ini);
    if (settings_first_run_accepted(temp_ini)) return 11;
    WritePrivateProfileStringW(L"Legal", L"Accepted", L"01", temp_ini);
    if (settings_first_run_accepted(temp_ini)) return 12;
    WritePrivateProfileStringW(L"Legal", L"Accepted", L"1x", temp_ini);
    if (settings_first_run_accepted(temp_ini)) return 13;
    if (!settings_record_first_run_acceptance(temp_ini) ||
        !settings_first_run_accepted(temp_ini))
        return 9;
    GetPrivateProfileStringW(L"Legal", L"Accepted", L"", value, ARRAYSIZE(value), temp_ini);
    if (wcscmp(value, L"1") != 0) return 10;
    DeleteFileW(temp_ini);
    for (index = 0; index < APP_THEME_COUNT; ++index) {
        const wchar_t *name = settings_theme_name((AppTheme)index);
        ThemeColors colors = settings_theme_colors((AppTheme)index);
        AppSettings settings = {0};
        HFONT font;
        if (name == NULL || name[0] == L'\0' ||
            !settings_theme_available((AppTheme)index) ||
            (index != APP_THEME_WINDOWS_NATIVE && colors.window == previous))
            return 2;
        previous = colors.window;
        wcscpy_s(settings.font_face, ARRAYSIZE(settings.font_face), L"Segoe UI");
        settings.font_points = index == 0 ? 6 : 72;
        settings.monospace = (index & 1) != 0;
        font = settings_create_font(&settings, NULL);
        if (font == NULL) return 3;
        DeleteObject(font);
        wprintf(L"theme[%d]=%ls window=%06lx control=%06lx text=%06lx\n",
                index, name, (unsigned long)colors.window,
                (unsigned long)colors.control, (unsigned long)colors.text);
    }
    if (APP_THEME_COUNT != 10 ||
        wcscmp(settings_theme_name(APP_THEME_PASTEL), L"Pastel") != 0 ||
        wcscmp(settings_theme_name(APP_THEME_WINDOWS_NATIVE), L"Windows Native") != 0 ||
        wcscmp(settings_theme_name(APP_THEME_OFFICE_2003_SKIN), L"Office 2003") != 0 ||
        settings_theme_colors(APP_THEME_OFFICE_2003_SKIN).hot_border != RGB(230,139,44) ||
        !settings_theme_is_external(APP_THEME_OFFICE_2003_SKIN) ||
        settings_theme_is_external(APP_THEME_WINDOWS_NATIVE) ||
        wcscmp(settings_theme_name((AppTheme)-1), L"Pastel") != 0)
        return 4;
    for (index = 0; index < APP_LANGUAGE_COUNT; ++index) {
        if (settings_language_name((AppLanguage)index)[0] == L'\0') return 5;
    }
    if (APP_LANGUAGE_COUNT != 4 ||
        wcscmp(settings_language_name((AppLanguage)-1), L"English") != 0)
        return 6;
    {
        AppSettings saved;
        AppSettings loaded;
        wchar_t settings_path[MAX_PATH];
        int persistence_error = 0;
        if (!settings_ini_path(settings_path, ARRAYSIZE(settings_path))) return 14;
        if (GetFileAttributesW(settings_path) != INVALID_FILE_ATTRIBUTES) return 15;
        WritePrivateProfileStringW(L"Appearance", L"Version", L"4", settings_path);
        WritePrivateProfileStringW(L"Appearance", L"Theme", L"5", settings_path);
        settings_load(&saved);
        if (saved.theme != APP_THEME_WINDOWS_NATIVE) persistence_error = 25;
        WritePrivateProfileStringW(L"Appearance", L"Theme", L"6", settings_path);
        settings_load(&saved);
        if (saved.theme != APP_THEME_OFFICE_2003_SKIN) persistence_error = 26;
        WritePrivateProfileStringW(L"Appearance", L"Theme", L"1", settings_path);
        settings_load(&saved);
        if (saved.theme != APP_THEME_PASTEL) persistence_error = 27;
        if (!DeleteFileW(settings_path) && persistence_error == 0)
            persistence_error = 28;
        settings_load(&saved);
        if (saved.theme != APP_THEME_PASTEL ||
            saved.sequential_playback || saved.remember_directories)
            persistence_error = 16;
        saved.sequential_playback = TRUE;
        saved.remember_directories = TRUE;
        settings_save(&saved);
        settings_load(&loaded);
        if (!loaded.sequential_playback || !loaded.remember_directories)
            persistence_error = 17;
        if (!settings_save_last_directories(L"C:\\Audio Left", L"D:\\Audio Right"))
            persistence_error = 20;
        {
            wchar_t left[64], right[64];
            if (!settings_load_last_directories(left, ARRAYSIZE(left),
                                                right, ARRAYSIZE(right)) ||
                wcscmp(left, L"C:\\Audio Left") != 0 ||
                wcscmp(right, L"D:\\Audio Right") != 0)
                persistence_error = 21;
        }
        {
            wchar_t *left = (wchar_t *)malloc(32768 * sizeof(*left));
            wchar_t *right = (wchar_t *)malloc(32768 * sizeof(*right));
            if (left == NULL || right == NULL ||
                !settings_load_last_directories(left, 32768, right, 32768) ||
                wcscmp(left, L"C:\\Audio Left") != 0 ||
                wcscmp(right, L"D:\\Audio Right") != 0)
                persistence_error = 24;
            free(left);
            free(right);
        }
        loaded.sequential_playback = FALSE;
        loaded.remember_directories = FALSE;
        settings_save(&loaded);
        if (!settings_clear_last_directories()) persistence_error = 22;
        settings_load(&saved);
        if (saved.sequential_playback || saved.remember_directories)
            persistence_error = 18;
        {
            wchar_t left[64] = L"x", right[64] = L"x";
            if (settings_load_last_directories(left, ARRAYSIZE(left),
                                               right, ARRAYSIZE(right)) ||
                left[0] != L'\0' || right[0] != L'\0')
                persistence_error = 23;
        }
        if (!DeleteFileW(settings_path) && persistence_error == 0) persistence_error = 19;
        if (persistence_error != 0) return persistence_error;
    }
    wprintf(L"first-run acceptance, sequential and dual-directory persistence, two built-ins, eight external skins, four languages, and font creation passed\n");
    return 0;
}
