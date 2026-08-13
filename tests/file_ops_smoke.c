#include "file_ops.h"

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

typedef struct CopyProgressEvidence {
    unsigned int callbacks;
    ULONGLONG total;
    ULONGLONG transferred;
} CopyProgressEvidence;

static void record_copy_progress(ULONGLONG total, ULONGLONG transferred,
                                 void *context)
{
    CopyProgressEvidence *evidence = (CopyProgressEvidence *)context;
    evidence->callbacks++;
    evidence->total = total;
    evidence->transferred = transferred;
}

static int join(const wchar_t *left, const wchar_t *right, wchar_t *out, size_t count)
{
    return swprintf_s(out, count, L"%ls\\%ls", left, right) > 0;
}

static int write_text(const wchar_t *path, const char *text)
{
    DWORD written;
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    WriteFile(file, text, (DWORD)strlen(text), &written, NULL);
    CloseHandle(file);
    return written == strlen(text);
}

int wmain(int argc, wchar_t **argv)
{
    wchar_t source[1024], copy[1024], moved[1024], tree[1024], child[1024];
    wchar_t destination[1024], copied_child[1024], moved_tree[1024];
    CopyProgressEvidence progress = {0};
    if (argc == 3 && wcscmp(argv[1], L"--classify") == 0) {
        FileOpsDeleteDisposition disposition = file_ops_delete_disposition(argv[2]);
        wprintf(L"%ls => %ls\n", argv[2],
                disposition == FILE_OPS_DELETE_PERMANENT ?
                    L"permanent delete" : L"Recycle Bin");
        return 0;
    }
    if (file_ops_delete_disposition_for_drive_type(DRIVE_REMOVABLE) !=
            FILE_OPS_DELETE_PERMANENT ||
        file_ops_delete_disposition_for_drive_type(DRIVE_FIXED) !=
            FILE_OPS_DELETE_RECYCLE ||
        file_ops_delete_disposition_for_drive_type(DRIVE_REMOTE) !=
            FILE_OPS_DELETE_RECYCLE ||
        (file_ops_delete_flags(FILE_OPS_DELETE_RECYCLE) & FOF_ALLOWUNDO) == 0 ||
        (file_ops_delete_flags(FILE_OPS_DELETE_PERMANENT) & FOF_ALLOWUNDO) != 0 ||
        (file_ops_delete_flags(FILE_OPS_DELETE_PERMANENT) & FOF_WANTNUKEWARNING) != 0 ||
        (file_ops_delete_flags(FILE_OPS_DELETE_PERMANENT) & FOF_NOCONFIRMATION) == 0)
        return 12;
    if (file_ops_delete_disposition(NULL) != FILE_OPS_DELETE_RECYCLE ||
        file_ops_delete_disposition(L"") != FILE_OPS_DELETE_RECYCLE ||
        file_ops_delete_disposition(L"?:\\AudioCommander-nonexistent") !=
            FILE_OPS_DELETE_RECYCLE)
        return 13;
    if (argc == 1) {
        wprintf(L"removable-drive permanent-delete policy passed\n");
        return 0;
    }
    if ((argc != 2 && argc != 3) || !CreateDirectoryW(argv[1], NULL)) return 2;
    join(argv[1], L"source.txt", source, ARRAYSIZE(source));
    join(argv[1], L"copy.txt", copy, ARRAYSIZE(copy));
    join(argv[1], L"moved.txt", moved, ARRAYSIZE(moved));
    if (!write_text(source, "alpha") ||
        !file_ops_copy_with_progress(source, copy, false,
                                     record_copy_progress, &progress)) return 3;
    if (progress.callbacks == 0 || progress.total != 5 ||
        progress.transferred != progress.total) return 12;
    if (file_ops_copy(source, copy, false) || !file_ops_copy(source, copy, true)) return 4;
    if (!file_ops_move(copy, moved, false) || file_ops_exists(copy) || !file_ops_exists(moved)) return 5;

    join(argv[1], L"tree", tree, ARRAYSIZE(tree));
    join(tree, L"child", child, ARRAYSIZE(child));
    CreateDirectoryW(tree, NULL);
    CreateDirectoryW(child, NULL);
    join(child, L"payload.txt", source, ARRAYSIZE(source));
    if (!write_text(source, "nested")) return 6;
    join(argv[1], L"copy-destination", destination, ARRAYSIZE(destination));
    CreateDirectoryW(destination, NULL);
    if (!file_ops_copy_directory(NULL, tree, destination)) return 7;
    swprintf_s(copied_child, ARRAYSIZE(copied_child), L"%ls\\tree\\child\\payload.txt", destination);
    if (!file_ops_exists(copied_child)) return 8;

    join(argv[1], L"move-destination", destination, ARRAYSIZE(destination));
    CreateDirectoryW(destination, NULL);
    if (!file_ops_move_directory(NULL, tree, destination)) return 9;
    swprintf_s(moved_tree, ARRAYSIZE(moved_tree), L"%ls\\tree\\child\\payload.txt", destination);
    if (file_ops_exists(tree) || !file_ops_exists(moved_tree)) return 10;
    if (argc == 3) {
        wchar_t cross_source[1024], cross_destination[1024];
        CreateDirectoryW(argv[2], NULL);
        join(argv[1], L"cross-drive-source.txt", cross_source, ARRAYSIZE(cross_source));
        join(argv[2], L"cross-drive-moved.txt", cross_destination, ARRAYSIZE(cross_destination));
        if (!write_text(cross_source, "cross drive") ||
            !file_ops_move(cross_source, cross_destination, false) ||
            file_ops_exists(cross_source) || !file_ops_exists(cross_destination)) return 11;
    }
    wprintf(L"removable-delete policy, file collision policy, and nested directory copy/move passed\n");
    return 0;
}
