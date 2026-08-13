#ifndef AUDIOCOMMANDER_COPY_PROGRESS_H
#define AUDIOCOMMANDER_COPY_PROGRESS_H

#include <windows.h>
#include <stdbool.h>

#define COPY_PROGRESS_REVEAL_DELAY_MS 600ULL

typedef struct CopyProgressStrings {
    const wchar_t *title;
    const wchar_t *patient_message;
    const wchar_t *file_format;
    const wchar_t *bytes_format;
} CopyProgressStrings;

bool copy_progress_should_show(ULONGLONG elapsed_ms, bool completed);
bool copy_progress_copy(HWND owner, const wchar_t *source,
                        const wchar_t *destination, bool overwrite,
                        const wchar_t *display_name, int item_number,
                        int item_count, const CopyProgressStrings *strings);

#endif
