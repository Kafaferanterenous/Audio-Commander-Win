#include "file_ops.h"

#include <shellapi.h>
#include <wchar.h>
#include <string.h>

bool file_ops_exists(const wchar_t *path)
{
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

typedef struct FileOpsCopyProgressBridge {
    FileOpsCopyProgressCallback callback;
    void *context;
} FileOpsCopyProgressBridge;

static DWORD CALLBACK copy_progress_routine(
    LARGE_INTEGER total_file_size, LARGE_INTEGER total_bytes_transferred,
    LARGE_INTEGER stream_size, LARGE_INTEGER stream_bytes_transferred,
    DWORD stream_number, DWORD callback_reason, HANDLE source_file,
    HANDLE destination_file, LPVOID data)
{
    FileOpsCopyProgressBridge *bridge = (FileOpsCopyProgressBridge *)data;
    (void)stream_size;
    (void)stream_bytes_transferred;
    (void)stream_number;
    (void)callback_reason;
    (void)source_file;
    (void)destination_file;
    if (bridge != NULL && bridge->callback != NULL)
        bridge->callback((ULONGLONG)total_file_size.QuadPart,
                         (ULONGLONG)total_bytes_transferred.QuadPart,
                         bridge->context);
    return PROGRESS_CONTINUE;
}

bool file_ops_copy_with_progress(const wchar_t *source, const wchar_t *destination,
                                 bool overwrite, FileOpsCopyProgressCallback callback,
                                 void *context)
{
    FileOpsCopyProgressBridge bridge;
    DWORD flags = overwrite ? 0 : COPY_FILE_FAIL_IF_EXISTS;
    bridge.callback = callback;
    bridge.context = context;
    return CopyFileExW(source, destination,
                       callback != NULL ? copy_progress_routine : NULL,
                       callback != NULL ? &bridge : NULL, NULL, flags) != FALSE;
}

bool file_ops_copy(const wchar_t *source, const wchar_t *destination, bool overwrite)
{
    return file_ops_copy_with_progress(source, destination, overwrite, NULL, NULL);
}

bool file_ops_move(const wchar_t *source, const wchar_t *destination, bool overwrite)
{
    DWORD flags = MOVEFILE_COPY_ALLOWED;
    if (overwrite) flags |= MOVEFILE_REPLACE_EXISTING;
    return MoveFileExW(source, destination, flags) != FALSE;
}

static bool shell_transfer(HWND owner, UINT operation_code, const wchar_t *source,
                           const wchar_t *destination_folder)
{
    wchar_t from[MAX_PATH + 2];
    wchar_t to[MAX_PATH + 2];
    SHFILEOPSTRUCTW operation = {0};
    size_t from_length = wcslen(source);
    size_t to_length = wcslen(destination_folder);
    if (from_length >= MAX_PATH || to_length >= MAX_PATH) {
        SetLastError(ERROR_FILENAME_EXCED_RANGE);
        return false;
    }
    memcpy(from, source, (from_length + 1) * sizeof(*from));
    memcpy(to, destination_folder, (to_length + 1) * sizeof(*to));
    from[from_length + 1] = L'\0';
    to[to_length + 1] = L'\0';
    operation.hwnd = owner;
    operation.wFunc = operation_code;
    operation.pFrom = from;
    operation.pTo = to;
    operation.fFlags = FOF_NOCONFIRMMKDIR;
    return SHFileOperationW(&operation) == 0 && !operation.fAnyOperationsAborted;
}

bool file_ops_copy_directory(HWND owner, const wchar_t *source, const wchar_t *destination_folder)
{
    return shell_transfer(owner, FO_COPY, source, destination_folder);
}

bool file_ops_move_directory(HWND owner, const wchar_t *source, const wchar_t *destination_folder)
{
    return shell_transfer(owner, FO_MOVE, source, destination_folder);
}

UINT file_ops_delete_flags(FileOpsDeleteDisposition disposition)
{
    UINT flags = FOF_NOCONFIRMATION;
    if (disposition == FILE_OPS_DELETE_RECYCLE)
        flags |= FOF_ALLOWUNDO | FOF_WANTNUKEWARNING;
    return flags;
}

static bool shell_delete(HWND owner, const wchar_t *path,
                         FileOpsDeleteDisposition disposition)
{
    wchar_t from[MAX_PATH + 2];
    SHFILEOPSTRUCTW operation = {0};
    size_t length = wcslen(path);
    if (length >= MAX_PATH) {
        SetLastError(ERROR_FILENAME_EXCED_RANGE);
        return false;
    }
    memcpy(from, path, (length + 1) * sizeof(*from));
    from[length + 1] = L'\0';
    operation.hwnd = owner;
    operation.wFunc = FO_DELETE;
    operation.pFrom = from;
    operation.fFlags = (FILEOP_FLAGS)file_ops_delete_flags(disposition);
    return SHFileOperationW(&operation) == 0 && !operation.fAnyOperationsAborted;
}

bool file_ops_recycle(HWND owner, const wchar_t *path)
{
    return shell_delete(owner, path, FILE_OPS_DELETE_RECYCLE);
}

bool file_ops_delete_permanently(HWND owner, const wchar_t *path)
{
    return shell_delete(owner, path, FILE_OPS_DELETE_PERMANENT);
}

FileOpsDeleteDisposition file_ops_delete_disposition_for_drive_type(UINT drive_type)
{
    return drive_type == DRIVE_REMOVABLE ? FILE_OPS_DELETE_PERMANENT :
                                           FILE_OPS_DELETE_RECYCLE;
}

FileOpsDeleteDisposition file_ops_delete_disposition(const wchar_t *path)
{
    wchar_t volume_path[MAX_PATH];
    if (path == NULL || path[0] == L'\0' ||
        !GetVolumePathNameW(path, volume_path, ARRAYSIZE(volume_path)))
        return FILE_OPS_DELETE_RECYCLE;
    return file_ops_delete_disposition_for_drive_type(GetDriveTypeW(volume_path));
}
