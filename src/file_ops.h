#ifndef AUDIOCOMMANDER_FILE_OPS_H
#define AUDIOCOMMANDER_FILE_OPS_H

#include <windows.h>
#include <stdbool.h>

typedef enum FileOpsDeleteDisposition {
    FILE_OPS_DELETE_RECYCLE,
    FILE_OPS_DELETE_PERMANENT
} FileOpsDeleteDisposition;

typedef void (*FileOpsCopyProgressCallback)(ULONGLONG total_bytes,
                                            ULONGLONG transferred_bytes,
                                            void *context);

bool file_ops_exists(const wchar_t *path);
bool file_ops_copy(const wchar_t *source, const wchar_t *destination, bool overwrite);
bool file_ops_copy_with_progress(const wchar_t *source, const wchar_t *destination,
                                 bool overwrite, FileOpsCopyProgressCallback callback,
                                 void *context);
bool file_ops_move(const wchar_t *source, const wchar_t *destination, bool overwrite);
bool file_ops_copy_directory(HWND owner, const wchar_t *source, const wchar_t *destination_folder);
bool file_ops_move_directory(HWND owner, const wchar_t *source, const wchar_t *destination_folder);
bool file_ops_recycle(HWND owner, const wchar_t *path);
bool file_ops_delete_permanently(HWND owner, const wchar_t *path);
FileOpsDeleteDisposition file_ops_delete_disposition(const wchar_t *path);
FileOpsDeleteDisposition file_ops_delete_disposition_for_drive_type(UINT drive_type);
UINT file_ops_delete_flags(FileOpsDeleteDisposition disposition);

#endif
