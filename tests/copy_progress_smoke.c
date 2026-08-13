#include "copy_progress.h"

#include <stdio.h>

int wmain(void)
{
    if (copy_progress_should_show(0, false)) return 1;
    if (copy_progress_should_show(COPY_PROGRESS_REVEAL_DELAY_MS - 1, false)) return 2;
    if (!copy_progress_should_show(COPY_PROGRESS_REVEAL_DELAY_MS, false)) return 3;
    if (!copy_progress_should_show(COPY_PROGRESS_REVEAL_DELAY_MS + 5000, false)) return 4;
    if (copy_progress_should_show(COPY_PROGRESS_REVEAL_DELAY_MS + 5000, true)) return 5;
    wprintf(L"copy progress remains hidden through %llu ms and reveals at %llu ms only while active\n",
            COPY_PROGRESS_REVEAL_DELAY_MS - 1, COPY_PROGRESS_REVEAL_DELAY_MS);
    return 0;
}
