#include "localization.h"

#include <stdio.h>
#include <wchar.h>

static int format_signature(const wchar_t *text, wchar_t *signature, size_t count)
{
    size_t used = 0;
    while (*text != L'\0') {
        if (*text++ != L'%') continue;
        if (*text == L'%') {
            ++text;
            continue;
        }
        while (*text != L'\0' && wcschr(L"diuoxXfFeEgGaAcspn", *text) == NULL) ++text;
        if (*text == L'\0' || used + 1 >= count) return 0;
        signature[used++] = *text++;
    }
    signature[used] = L'\0';
    return 1;
}

int wmain(void)
{
    int language;
    int text;
    for (language = 0; language < APP_LANGUAGE_COUNT; ++language) {
        if (!localization_language_complete((AppLanguage)language)) return 6;
        for (text = 0; text < UI_COUNT; ++text) {
            const wchar_t *value = localization_text((AppLanguage)language, (UiText)text);
            wchar_t expected[64], actual[64];
            if (value == NULL || value[0] == L'\0') {
                fwprintf(stderr, L"missing language=%d text=%d\n", language, text);
                return 2;
            }
            if (!format_signature(localization_text(APP_LANGUAGE_ENGLISH, (UiText)text),
                                  expected, ARRAYSIZE(expected)) ||
                !format_signature(value, actual, ARRAYSIZE(actual)) ||
                wcscmp(expected, actual) != 0) {
                fwprintf(stderr, L"format mismatch language=%d text=%d\n", language, text);
                return 7;
            }
        }
        if (localization_help_text((AppLanguage)language)[0] == L'\0') return 3;
        for (text = 0; text < APP_THEME_COUNT; ++text)
            if (localization_theme_name((AppLanguage)language, (AppTheme)text)[0] == L'\0')
                return 5;
    }
    if (wcscmp(localization_text((AppLanguage)-1, UI_SETTINGS), L"Settings...") != 0)
        return 4;
    wprintf(L"%d localized UI strings and Help passed for four languages\n", UI_COUNT);
    return 0;
}
