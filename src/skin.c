#include "skin.h"

#include <wchar.h>

static bool parse_hex_color(const wchar_t *value, COLORREF *color)
{
    unsigned int red, green, blue;
    wchar_t extra;
    if (value == NULL || color == NULL || wcslen(value) != 7 ||
        value[0] != L'#' ||
        swscanf_s(value + 1, L"%2x%2x%2x%c",
                  &red, &green, &blue, &extra, 1) != 3)
        return false;
    *color = RGB(red, green, blue);
    return true;
}

static bool read_color(const wchar_t *path, const wchar_t *key, COLORREF *color)
{
    wchar_t value[16];
    DWORD length = GetPrivateProfileStringW(
        L"Colors", key, L"", value, ARRAYSIZE(value), path);
    return length != 0 && length < ARRAYSIZE(value) - 1 &&
           parse_hex_color(value, color);
}

bool skin_load_file(const wchar_t *path, ThemeColors *colors)
{
    wchar_t format[8];
    ThemeColors loaded;
    if (path == NULL || path[0] == L'\0' || colors == NULL)
        return false;
    if (GetPrivateProfileStringW(L"Skin", L"Format", L"", format,
                                 ARRAYSIZE(format), path) != 1 ||
        wcscmp(format, L"1") != 0)
        return false;
    if (!read_color(path, L"Window", &loaded.window) ||
        !read_color(path, L"Control", &loaded.control) ||
        !read_color(path, L"Text", &loaded.text) ||
        !read_color(path, L"Selection", &loaded.selection) ||
        !read_color(path, L"SelectionText", &loaded.selection_text) ||
        !read_color(path, L"Border", &loaded.border) ||
        !read_color(path, L"HotBorder", &loaded.hot_border) ||
        !read_color(path, L"ButtonTop", &loaded.button_top) ||
        !read_color(path, L"ButtonBottom", &loaded.button_bottom) ||
        !read_color(path, L"Summary", &loaded.summary) ||
        !read_color(path, L"SummaryText", &loaded.summary_text) ||
        !read_color(path, L"Progress", &loaded.progress))
        return false;
    *colors = loaded;
    return true;
}
