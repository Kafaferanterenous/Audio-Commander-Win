#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <uxtheme.h>
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "browser.h"
#include "playback.h"
#include "playlist.h"
#include "settings.h"
#include "file_ops.h"
#include "copy_progress.h"
#include "media_info.h"
#include "metadata_loader.h"
#include "localization.h"

#define TR(id) localization_text(app_settings.language, (id))

#define APP_CLASS L"AudioCommanderWindow"
#define SETTINGS_CLASS L"AudioCommanderSettings"
#define HELP_CLASS L"AudioCommanderHelp"
#define PATH_CAPACITY 32768
#define WM_NAVIGATE_PATH (WM_APP + 1)
#define WM_METADATA_BATCH (WM_APP + 2)

enum { ID_SETTINGS = 10,
       ID_INFO, ID_HELP, ID_PROGRESS, ID_VOLUME,
       ID_COPY_RIGHT = 20, ID_MOVE_RIGHT, ID_DELETE_LEFT, ID_SWAP_DIRS,
       ID_COPY_LEFT, ID_MOVE_LEFT, ID_DELETE_RIGHT, ID_STOP_ALL, ID_REFRESH_ALL,
       ID_NEW_FOLDER = 30, ID_REFRESH_ACTIVE, ID_ACCEL_COPY, ID_ACCEL_MOVE, ID_ACCEL_DELETE,
       ID_LEFT_DRIVE = 99, ID_LEFT_PATH = 100, ID_LEFT_UP, ID_LEFT_REFRESH, ID_LEFT_LIST, ID_LEFT_BROWSE,
       ID_RIGHT_DRIVE = 199, ID_RIGHT_PATH = 200, ID_RIGHT_UP, ID_RIGHT_REFRESH, ID_RIGHT_LIST, ID_RIGHT_BROWSE,
       ID_FONT_CHOOSE = 300, ID_MONOSPACE, ID_REMEMBER_POSITION, ID_SEQUENTIAL_PLAYBACK,
       ID_INTERFACE_SIZE, ID_OPACITY, ID_THEME_COMBO, ID_LANGUAGE_COMBO,
       ID_SETTINGS_OK, ID_SETTINGS_CANCEL, ID_REMEMBER_DIRECTORIES,
       ID_HELP_TEXT = 400, ID_HELP_CLOSE };

typedef struct Pane {
    HWND drive_combo;
    HWND path_edit;
    HWND up_button;
    HWND refresh_button;
    HWND browse_button;
    HWND summary_label;
    HWND list;
    HWND info_panel;
    wchar_t folder[PATH_CAPACITY];
    BrowserListing listing;
    BrowserSortColumn sort_column;
    BOOL sort_descending;
    LONG metadata_generation;
    BOOL metadata_loading;
    size_t *entry_index_by_id;
    size_t entry_index_capacity;
} Pane;

static Pane panes[2];
static wchar_t playing_path[PATH_CAPACITY];
static AppSettings app_settings;
static HFONT app_font;
static HFONT app_bold_font;
static HBRUSH app_background_brush;
static HBRUSH app_control_brush;
static HBRUSH app_summary_brush;
static ThemeColors app_colors;
static HWND settings_window;
static HWND help_window;
static HFONT help_font;
static AppSettings settings_candidate;
static BOOL settings_accepted;
static HWND progress_bar;
static HWND volume_slider;
static unsigned long playback_position;
static unsigned long playback_total;
static Playlist playback_queue;
static HWND operation_buttons[2][3];
static HWND swap_button;
static HWND stop_button;
static HWND refresh_all_button;
static BOOL info_visible;
static MetadataLoader *metadata_loader;

static void fill_pane(HWND owner, Pane *pane, const wchar_t *requested_folder);
static void stop_playback(void);

static COLORREF blend_color(COLORREF first, COLORREF second, int amount, int total)
{
    return RGB(
        (GetRValue(first) * (total - amount) + GetRValue(second) * amount) / total,
        (GetGValue(first) * (total - amount) + GetGValue(second) * amount) / total,
        (GetBValue(first) * (total - amount) + GetBValue(second) * amount) / total);
}

static void fill_vertical_gradient(HDC dc, const RECT *area,
                                   COLORREF top, COLORREF bottom)
{
    int height = area->bottom - area->top;
    int row;
    if (height <= 0) return;
    for (row = 0; row < height; ++row) {
        RECT line = {area->left, area->top + row, area->right, area->top + row + 1};
        HBRUSH brush = CreateSolidBrush(blend_color(top, bottom, row, height));
        if (brush != NULL) {
            FillRect(dc, &line, brush);
            DeleteObject(brush);
        }
    }
}

static void set_button_owner_draw(HWND button, BOOL owner_draw, BOOL disable_theme)
{
    LONG_PTR style;
    if (button == NULL) return;
    style = GetWindowLongPtrW(button, GWL_STYLE);
    style = (style & ~BS_TYPEMASK) | (owner_draw ? BS_OWNERDRAW : BS_PUSHBUTTON);
    SetWindowLongPtrW(button, GWL_STYLE, style);
    SetWindowTheme(button, disable_theme ? L"" : NULL,
                   disable_theme ? L"" : NULL);
    SetWindowPos(button, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                 SWP_NOACTIVATE | SWP_FRAMECHANGED);
    InvalidateRect(button, NULL, TRUE);
}

static void apply_main_button_style(HWND window)
{
    HWND buttons[18];
    int count = 0;
    int index;
    BOOL external = settings_theme_is_external(app_settings.theme);
    BOOL native = app_settings.theme == APP_THEME_WINDOWS_NATIVE;
    buttons[count++] = GetDlgItem(window, ID_SETTINGS);
    buttons[count++] = GetDlgItem(window, ID_INFO);
    buttons[count++] = GetDlgItem(window, ID_HELP);
    for (index = 0; index < 2; ++index) {
        buttons[count++] = panes[index].up_button;
        buttons[count++] = panes[index].refresh_button;
        buttons[count++] = panes[index].browse_button;
        buttons[count++] = operation_buttons[index][0];
        buttons[count++] = operation_buttons[index][1];
        buttons[count++] = operation_buttons[index][2];
    }
    buttons[count++] = swap_button;
    buttons[count++] = stop_button;
    buttons[count++] = refresh_all_button;
    for (index = 0; index < count; ++index) {
        int id = buttons[index] != NULL ? GetDlgCtrlID(buttons[index]) : 0;
        BOOL special = id == ID_SWAP_DIRS || id == ID_DELETE_LEFT ||
                       id == ID_DELETE_RIGHT;
        set_button_owner_draw(buttons[index], external || (!native && special),
                              external);
    }
}

static void apply_font(HWND control)
{
    SendMessageW(control, WM_SETFONT, (WPARAM)app_font, TRUE);
}

static wchar_t *duplicate_wide_text(const wchar_t *text)
{
    size_t count = wcslen(text) + 1;
    wchar_t *copy = (wchar_t *)malloc(count * sizeof(*copy));
    if (copy != NULL) memcpy(copy, text, count * sizeof(*copy));
    return copy;
}

static void refresh_drive_list(Pane *pane)
{
    DWORD drives = GetLogicalDrives();
    int drive;
    SendMessageW(pane->drive_combo, CB_RESETCONTENT, 0, 0);
    for (drive = 0; drive < 26; ++drive) {
        if ((drives & (1u << drive)) != 0) {
            wchar_t root[] = L"A:\\";
            int item;
            root[0] = (wchar_t)(L'A' + drive);
            item = (int)SendMessageW(pane->drive_combo, CB_ADDSTRING, 0, (LPARAM)root);
            if (towupper(pane->folder[0]) == root[0]) {
                SendMessageW(pane->drive_combo, CB_SETCURSEL, item, 0);
            }
        }
    }
}

static void apply_appearance(HWND window)
{
    int index;
    HFONT next_font = settings_create_font(&app_settings, window);
    HFONT next_bold_font = NULL;
    LOGFONTW bold_description;
    if (next_font == NULL) return;
    if (GetObjectW(next_font, sizeof(bold_description), &bold_description) == sizeof(bold_description)) {
        bold_description.lfWeight = FW_BOLD;
        next_bold_font = CreateFontIndirectW(&bold_description);
    }
    if (app_font != NULL) DeleteObject(app_font);
    if (app_bold_font != NULL) DeleteObject(app_bold_font);
    app_font = next_font;
    app_bold_font = next_bold_font;
    app_colors = settings_theme_colors(app_settings.theme);
    if (app_background_brush != NULL) DeleteObject(app_background_brush);
    if (app_control_brush != NULL) DeleteObject(app_control_brush);
    if (app_summary_brush != NULL) DeleteObject(app_summary_brush);
    app_background_brush = CreateSolidBrush(app_colors.window);
    app_control_brush = CreateSolidBrush(app_colors.control);
    app_summary_brush = CreateSolidBrush(app_colors.summary);
    apply_main_button_style(window);
    apply_font(GetDlgItem(window, ID_SETTINGS));
    apply_font(GetDlgItem(window, ID_INFO));
    apply_font(GetDlgItem(window, ID_HELP));
    apply_font(progress_bar);
    apply_font(volume_slider);
    for (index = 0; index < 2; ++index) {
        apply_font(panes[index].drive_combo);
        apply_font(panes[index].path_edit);
        apply_font(panes[index].up_button);
        apply_font(panes[index].refresh_button);
        apply_font(panes[index].browse_button);
        SendMessageW(panes[index].summary_label, WM_SETFONT,
                     (WPARAM)(app_bold_font != NULL ? app_bold_font : app_font), TRUE);
        apply_font(panes[index].list);
        apply_font(panes[index].info_panel);
        ListView_SetBkColor(panes[index].list, app_colors.control);
        ListView_SetTextBkColor(panes[index].list, app_colors.control);
        ListView_SetTextColor(panes[index].list, app_colors.text);
        ListView_SetOutlineColor(panes[index].list, app_colors.selection);
        if (app_settings.theme == APP_THEME_WINDOWS_NATIVE) {
            SetWindowTheme(panes[index].list, L"Explorer", NULL);
            SetWindowTheme(ListView_GetHeader(panes[index].list), NULL, NULL);
        } else if (settings_theme_is_external(app_settings.theme)) {
            SetWindowTheme(panes[index].list, L"", L"");
            SetWindowTheme(ListView_GetHeader(panes[index].list), L"", L"");
        } else {
            SetWindowTheme(panes[index].list, NULL, NULL);
            SetWindowTheme(ListView_GetHeader(panes[index].list), NULL, NULL);
        }
        InvalidateRect(panes[index].list, NULL, TRUE);
        InvalidateRect(panes[index].info_panel, NULL, TRUE);
    }
    for (index = 0; index < 3; ++index) {
        apply_font(operation_buttons[0][index]);
        apply_font(operation_buttons[1][index]);
    }
    apply_font(swap_button);
    apply_font(stop_button);
    apply_font(refresh_all_button);
    SendMessageW(progress_bar, PBM_SETBKCOLOR, 0,
                 app_settings.theme == APP_THEME_WINDOWS_NATIVE ?
                 CLR_DEFAULT : app_colors.control);
    SendMessageW(progress_bar, PBM_SETBARCOLOR, 0,
                 app_settings.theme == APP_THEME_WINDOWS_NATIVE ?
                 CLR_DEFAULT : app_colors.progress);
    SetClassLongPtrW(window, GCLP_HBRBACKGROUND, (LONG_PTR)app_background_brush);
    {
        LONG_PTR extended_style = GetWindowLongPtrW(window, GWL_EXSTYLE);
        if (app_settings.opacity < 100) {
            SetWindowLongPtrW(window, GWL_EXSTYLE, extended_style | WS_EX_LAYERED);
            SetLayeredWindowAttributes(window, 0,
                (BYTE)MulDiv(255, app_settings.opacity, 100), LWA_ALPHA);
        } else if ((extended_style & WS_EX_LAYERED) != 0) {
            SetWindowLongPtrW(window, GWL_EXSTYLE, extended_style & ~WS_EX_LAYERED);
            RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
        }
    }
    InvalidateRect(window, NULL, TRUE);
}

static LRESULT CALLBACK path_subclass(HWND control, UINT message, WPARAM w_param,
                                     LPARAM l_param, UINT_PTR subclass_id, DWORD_PTR data)
{
    (void)subclass_id;
    if (message == WM_KEYDOWN && w_param == VK_RETURN) {
        SendMessageW((HWND)data, WM_NAVIGATE_PATH, GetDlgCtrlID(control), 0);
        return 0;
    }
    return DefSubclassProc(control, message, w_param, l_param);
}

static void show_error(HWND owner, const wchar_t *message)
{
    MessageBoxW(owner, message, TR(UI_ERROR_TITLE), MB_OK | MB_ICONERROR);
}

static void show_last_error(HWND owner, const wchar_t *action)
{
    wchar_t detail[512];
    wchar_t message[768];
    wcscpy_s(detail, ARRAYSIZE(detail), TR(UI_UNKNOWN_WINDOWS_ERROR));
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                   GetLastError(), 0, detail, ARRAYSIZE(detail), NULL);
    swprintf_s(message, ARRAYSIZE(message), TR(UI_FAILED_FORMAT), action, detail);
    show_error(owner, message);
}

static int selected_item(const Pane *pane)
{
    return ListView_GetNextItem(pane->list, -1, LVNI_SELECTED);
}

static BOOL entry_path(const Pane *pane, int item, wchar_t *path, size_t path_count,
                       const BrowserEntry **selected_entry)
{
    if (item < 0 || (size_t)item >= pane->listing.count) return FALSE;
    if (!browser_join_path(pane->folder, pane->listing.entries[item].name, path, path_count)) return FALSE;
    if (selected_entry != NULL) *selected_entry = &pane->listing.entries[item];
    return TRUE;
}

static BOOL path_contains(const wchar_t *folder, const wchar_t *path)
{
    size_t length = wcslen(folder);
    return _wcsnicmp(folder, path, length) == 0 &&
           (path[length] == L'\0' || path[length] == L'\\' || path[length] == L'/');
}

static void update_info_panel(Pane *pane)
{
    int item = selected_item(pane);
    wchar_t *path;
    wchar_t text[4096];
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    SYSTEMTIME utc_time = {0}, local_time = {0};
    const BrowserEntry *entry;
    const wchar_t *extension;
    unsigned long seconds;
    unsigned long bitrate = 0;
    MediaInfo media = {0};
    wchar_t bit_depth_text[32];
    if (pane->info_panel == NULL) return;
    if (item < 0 || (size_t)item >= pane->listing.count) {
        SetWindowTextW(pane->info_panel, TR(UI_SELECT_AUDIO_INFO));
        return;
    }
    path = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*path));
    if (path == NULL) return;
    entry = &pane->listing.entries[item];
    if (!browser_join_path(pane->folder, entry->name, path, PATH_CAPACITY)) {
        SetWindowTextW(pane->info_panel, TR(UI_PATH_TOO_LONG_DISPLAY));
        free(path);
        return;
    }
    if (entry->kind != BROWSER_ENTRY_AUDIO_FILE) {
        swprintf_s(text, ARRAYSIZE(text), TR(UI_FOLDER_INFO_FORMAT), entry->name, path);
        SetWindowTextW(pane->info_panel, text);
        free(path);
        return;
    }
    ZeroMemory(&attributes, sizeof(attributes));
    GetFileAttributesExW(path, GetFileExInfoStandard, &attributes);
    FileTimeToSystemTime(&attributes.ftLastWriteTime, &utc_time);
    SystemTimeToTzSpecificLocalTime(NULL, &utc_time, &local_time);
    seconds = entry->duration_ms / 1000;
    if (entry->duration_ms > 0) bitrate = (unsigned long)((entry->size * 8) / entry->duration_ms);
    media_read_info(path, &media);
    if (media.duration_ms == 0) media.duration_ms = entry->duration_ms;
    if (media.bitrate_kbps == 0) media.bitrate_kbps = bitrate;
    seconds = media.duration_ms / 1000;
    if (media.bit_depth > 0) swprintf_s(bit_depth_text, ARRAYSIZE(bit_depth_text), L"%lu", media.bit_depth);
    else wcscpy_s(bit_depth_text, ARRAYSIZE(bit_depth_text), TR(UI_NA_COMPRESSED));
    extension = wcsrchr(entry->name, L'.');
    swprintf_s(text, ARRAYSIZE(text), TR(UI_AUDIO_INFO_FORMAT),
        entry->name, path, extension != NULL ? extension + 1 : TR(UI_UNKNOWN),
        media.container[0] ? media.container : TR(UI_UNKNOWN),
        media.codec[0] ? media.codec : TR(UI_UNKNOWN),
        entry->size, (entry->size + 1023) / 1024,
        seconds / 60, seconds % 60, media.duration_ms % 1000, media.bitrate_kbps,
        media.bitrate_mode[0] ? media.bitrate_mode : TR(UI_UNKNOWN),
        media.sample_rate_hz, bit_depth_text, media.channels,
        media.channel_layout[0] ? media.channel_layout : TR(UI_UNKNOWN),
        media.title[0] ? media.title : TR(UI_NONE),
        media.artist[0] ? media.artist : TR(UI_NONE),
        media.album[0] ? media.album : TR(UI_NONE),
        local_time.wYear, local_time.wMonth, local_time.wDay,
        local_time.wHour, local_time.wMinute, local_time.wSecond,
        (attributes.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? TR(UI_READ_ONLY) : L"",
        (attributes.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ? TR(UI_HIDDEN) : L"",
        (attributes.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) ? TR(UI_ARCHIVE) : TR(UI_NORMAL),
        _wcsicmp(path, playing_path) == 0 ? TR(UI_YES) : TR(UI_NO));
    SetWindowTextW(pane->info_panel, text);
    free(path);
}

static void update_operation_state(void)
{
    int pane_index, index;
    for (pane_index = 0; pane_index < 2; ++pane_index) {
        BOOL enabled = selected_item(&panes[pane_index]) >= 0;
        for (index = 0; index < 3; ++index)
            EnableWindow(operation_buttons[pane_index][index], enabled);
    }
    EnableWindow(swap_button, TRUE);
}

static BOOL confirm_overwrite(HWND owner, const wchar_t *destination)
{
    size_t message_count = wcslen(TR(UI_OVERWRITE_FORMAT)) + wcslen(destination) + 1;
    wchar_t *message = (wchar_t *)malloc(message_count * sizeof(*message));
    BOOL confirmed;
    if (message == NULL) return FALSE;
    swprintf_s(message, message_count, TR(UI_OVERWRITE_FORMAT), destination);
    confirmed = MessageBoxW(owner, message, TR(UI_CONFIRM_OVERWRITE),
                            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
    free(message);
    return confirmed;
}

static void transfer_between(HWND owner, int source_index, int destination_index, BOOL move_file)
{
    wchar_t *source;
    wchar_t *destination;
    const BrowserEntry *entry;
    int item = -1;
    int scan_item = -1;
    int file_number = 0;
    int file_count = 0;
    int processed = 0;
    BOOL failed = FALSE;
    CopyProgressStrings copy_strings = {
        TR(UI_COPY_PROGRESS_TITLE), TR(UI_COPY_PATIENT_MESSAGE),
        TR(UI_COPY_FILE_PROGRESS_FORMAT), TR(UI_COPY_BYTES_PROGRESS_FORMAT)
    };
    if (selected_item(&panes[source_index]) < 0) {
        show_error(owner, TR(UI_SELECT_SOURCE));
        return;
    }
    source = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*source));
    destination = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*destination));
    if (source == NULL || destination == NULL) {
        free(source);
        free(destination);
        return;
    }
    while ((scan_item = ListView_GetNextItem(panes[source_index].list, scan_item,
                                              LVNI_SELECTED)) >= 0) {
        if ((size_t)scan_item < panes[source_index].listing.count &&
            panes[source_index].listing.entries[scan_item].kind != BROWSER_ENTRY_DIRECTORY)
            file_count++;
    }
    while ((item = ListView_GetNextItem(panes[source_index].list, item, LVNI_SELECTED)) >= 0) {
        BOOL overwrite = FALSE;
        BOOL success;
        if (!entry_path(&panes[source_index], item, source, PATH_CAPACITY, &entry) ||
            !browser_join_path(panes[destination_index].folder, entry->name, destination, PATH_CAPACITY)) {
            show_error(owner, TR(UI_SOURCE_PATH_LONG));
            failed = TRUE;
            break;
        }
        if (_wcsicmp(source, destination) == 0) {
            show_error(owner, TR(UI_SAME_ITEM));
            failed = TRUE;
            break;
        }
        if (file_ops_exists(destination)) {
            if (entry->kind == BROWSER_ENTRY_DIRECTORY) {
                size_t warning_count = wcslen(TR(UI_FOLDER_MERGE_FORMAT)) +
                                       wcslen(destination) + 1;
                wchar_t *warning = (wchar_t *)malloc(warning_count * sizeof(*warning));
                int response;
                if (warning == NULL) continue;
                swprintf_s(warning, warning_count, TR(UI_FOLDER_MERGE_FORMAT), destination);
                response = MessageBoxW(owner, warning, TR(UI_CONFIRM_FOLDER_MERGE),
                                       MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
                free(warning);
                if (response != IDYES) continue;
            } else {
                if (!confirm_overwrite(owner, destination)) continue;
                overwrite = TRUE;
            }
        }
        if (playing_path[0] != L'\0' &&
            ((move_file && path_contains(source, playing_path)) || path_contains(destination, playing_path)))
            stop_playback();
        if (entry->kind == BROWSER_ENTRY_DIRECTORY) {
            success = move_file ? file_ops_move_directory(owner, source, panes[destination_index].folder) :
                                  file_ops_copy_directory(owner, source, panes[destination_index].folder);
        } else {
            file_number++;
            success = move_file ? file_ops_move(source, destination, overwrite) :
                                  copy_progress_copy(owner, source, destination, overwrite,
                                                     entry->name, file_number, file_count,
                                                     &copy_strings);
        }
        if (!success) {
            show_last_error(owner, move_file ? TR(UI_MOVE_ACTION) : TR(UI_COPY_ACTION));
            failed = TRUE;
            break;
        }
        processed++;
    }
    free(source);
    free(destination);
    if (processed > 0 || failed) {
        fill_pane(owner, &panes[source_index], panes[source_index].folder);
        fill_pane(owner, &panes[destination_index], panes[destination_index].folder);
    }
}

static void delete_selection(HWND owner, int pane_index)
{
    wchar_t *source;
    wchar_t message[512];
    FileOpsDeleteDisposition disposition;
    int item = -1;
    int selected_count = 0;
    int directory_count = 0;
    BOOL any_deleted = FALSE;
    while ((item = ListView_GetNextItem(panes[pane_index].list, item, LVNI_SELECTED)) >= 0) {
        selected_count++;
        if ((size_t)item < panes[pane_index].listing.count &&
            panes[pane_index].listing.entries[item].kind == BROWSER_ENTRY_DIRECTORY) directory_count++;
    }
    if (selected_count == 0) {
        show_error(owner, TR(UI_SELECT_ITEMS));
        return;
    }
    disposition = file_ops_delete_disposition(panes[pane_index].folder);
    if (disposition == FILE_OPS_DELETE_PERMANENT) {
        if (directory_count > 0)
            swprintf_s(message, ARRAYSIZE(message), TR(UI_DELETE_REMOVABLE_DIR_FORMAT),
                       selected_count, directory_count);
        else
            swprintf_s(message, ARRAYSIZE(message), TR(UI_DELETE_REMOVABLE_FILES_FORMAT),
                       selected_count);
    } else if (directory_count > 0) {
        swprintf_s(message, ARRAYSIZE(message), TR(UI_DELETE_DIR_FORMAT),
                   selected_count, directory_count);
    } else {
        swprintf_s(message, ARRAYSIZE(message), TR(UI_DELETE_FILES_FORMAT), selected_count);
    }
    if (MessageBoxW(owner, message,
                    disposition == FILE_OPS_DELETE_PERMANENT ?
                        TR(UI_CONFIRM_PERMANENT_DELETE) : TR(UI_CONFIRM_DELETE),
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    source = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*source));
    if (source == NULL) return;
    item = -1;
    while ((item = ListView_GetNextItem(panes[pane_index].list, item, LVNI_SELECTED)) >= 0) {
        if (!entry_path(&panes[pane_index], item, source, PATH_CAPACITY, NULL)) continue;
        if (playing_path[0] != L'\0' && path_contains(source, playing_path)) stop_playback();
        if (!(disposition == FILE_OPS_DELETE_PERMANENT ?
              file_ops_delete_permanently(owner, source) :
              file_ops_recycle(owner, source))) {
            show_last_error(owner, disposition == FILE_OPS_DELETE_PERMANENT ?
                                   TR(UI_PERMANENT_DELETE_ACTION) :
                                   TR(UI_RECYCLE_DELETE_ACTION));
            break;
        }
        any_deleted = TRUE;
    }
    free(source);
    if (any_deleted) fill_pane(owner, &panes[pane_index], panes[pane_index].folder);
}

static void swap_directories(HWND owner)
{
    wchar_t *left = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*left));
    wchar_t *right = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*right));
    if (left == NULL || right == NULL) {
        free(left);
        free(right);
        return;
    }
    wcscpy_s(left, PATH_CAPACITY, panes[0].folder);
    wcscpy_s(right, PATH_CAPACITY, panes[1].folder);
    fill_pane(owner, &panes[0], right);
    fill_pane(owner, &panes[1], left);
    free(left);
    free(right);
}

static void create_new_folder(HWND owner, Pane *pane)
{
    wchar_t name[64];
    wchar_t *path = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*path));
    int suffix = 2;
    size_t index;
    if (path == NULL) return;
    wcscpy_s(name, ARRAYSIZE(name), TR(UI_NEW_FOLDER));
    while (browser_join_path(pane->folder, name, path, PATH_CAPACITY) && file_ops_exists(path)) {
        swprintf_s(name, ARRAYSIZE(name), TR(UI_NEW_FOLDER_NUMBERED), suffix++);
    }
    if (!browser_join_path(pane->folder, name, path, PATH_CAPACITY)) {
        show_error(owner, TR(UI_NEW_FOLDER_PATH_LONG));
        free(path);
        return;
    }
    if (!CreateDirectoryW(path, NULL)) {
        show_last_error(owner, TR(UI_CREATE_FOLDER_ACTION));
        free(path);
        return;
    }
    free(path);
    fill_pane(owner, pane, pane->folder);
    for (index = 0; index < pane->listing.count; ++index) {
        if (_wcsicmp(pane->listing.entries[index].name, name) == 0) {
            SetFocus(pane->list);
            ListView_SetItemState(pane->list, (int)index, LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(pane->list, (int)index, FALSE);
            ListView_EditLabel(pane->list, (int)index);
            break;
        }
    }
}

static void show_list_context_menu(HWND owner, Pane *pane)
{
    HMENU menu = CreatePopupMenu();
    POINT point;
    int command;
    if (menu == NULL) return;
    AppendMenuW(menu, MF_STRING, ID_NEW_FOLDER, TR(UI_NEW_FOLDER_MENU));
    GetCursorPos(&point);
    command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             point.x, point.y, 0, owner, NULL);
    DestroyMenu(menu);
    if (command == ID_NEW_FOLDER) create_new_folder(owner, pane);
}

static int active_pane_index(void)
{
    HWND focus = GetFocus();
    int index;
    for (index = 0; index < 2; ++index) {
        if (focus == panes[index].list || focus == panes[index].path_edit ||
            focus == panes[index].drive_combo || focus == panes[index].up_button ||
            focus == panes[index].refresh_button || focus == panes[index].info_panel)
            return index;
    }
    return 0;
}

static void refresh_active_pane(HWND owner)
{
    int index = active_pane_index();
    fill_pane(owner, &panes[index], panes[index].folder);
}

static void refresh_all_panes(HWND owner)
{
    fill_pane(owner, &panes[0], panes[0].folder);
    fill_pane(owner, &panes[1], panes[1].folder);
}

static void reset_playback_display(void)
{
    playing_path[0] = L'\0';
    playback_position = 0;
    playback_total = 0;
    if (progress_bar != NULL) SendMessageW(progress_bar, PBM_SETPOS, 0, 0);
    if (progress_bar != NULL) InvalidateRect(progress_bar, NULL, TRUE);
    if (stop_button != NULL) EnableWindow(stop_button, FALSE);
    update_info_panel(&panes[0]);
    update_info_panel(&panes[1]);
}

static void stop_playback(void)
{
    playback_stop();
    playlist_clear(&playback_queue);
    reset_playback_display();
}

static void set_playing_path(const wchar_t *path)
{
    wcscpy_s(playing_path, ARRAYSIZE(playing_path), path);
    EnableWindow(stop_button, TRUE);
    playback_position = 0;
    playback_total = playback_duration_ms();
    SendMessageW(progress_bar, PBM_SETRANGE32, 0, playback_total);
    SendMessageW(progress_bar, PBM_SETPOS, 0, 0);
    InvalidateRect(progress_bar, NULL, TRUE);
    update_info_panel(&panes[0]);
    update_info_panel(&panes[1]);
}

static void advance_playback_sequence(void)
{
    const wchar_t *path;
    playback_stop();
    reset_playback_display();
    while ((path = playlist_next(&playback_queue)) != NULL) {
        if (playback_play(path)) {
            set_playing_path(path);
            return;
        }
    }
    playlist_clear(&playback_queue);
}

static LRESULT CALLBACK progress_subclass(HWND control, UINT message, WPARAM w_param,
                                         LPARAM l_param, UINT_PTR subclass_id, DWORD_PTR data)
{
    (void)subclass_id;
    (void)data;
    if (message == WM_LBUTTONDOWN && playing_path[0] != L'\0' && playback_total > 0) {
        RECT area;
        unsigned long target;
        GetClientRect(control, &area);
        target = area.right > 0 ? (unsigned long)
            ((unsigned long long)(short)LOWORD(l_param) * playback_total /
             (unsigned long)area.right) : 0;
        if (target > playback_total) target = playback_total;
        if (playback_seek_ms(target)) {
            playback_position = target;
            SendMessageW(progress_bar, PBM_SETPOS, playback_position, 0);
        }
        return 0;
    }
    if (message == WM_SETCURSOR && playing_path[0] != L'\0') {
        SetCursor(LoadCursorW(NULL, IDC_HAND));
        return TRUE;
    }
    return DefSubclassProc(control, message, w_param, l_param);
}

static void update_sort_header(Pane *pane)
{
    HWND header = ListView_GetHeader(pane->list);
    int index;
    for (index = 0; index < 3; ++index) {
        HDITEMW item = {0};
        item.mask = HDI_FORMAT;
        if (Header_GetItem(header, index, &item)) {
            item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
            if (index == (int)pane->sort_column)
                item.fmt |= pane->sort_descending ? HDF_SORTDOWN : HDF_SORTUP;
            Header_SetItem(header, index, &item);
        }
    }
}

static void update_directory_summary(Pane *pane)
{
    BrowserListingSummary summary;
    wchar_t text[256];
    unsigned long long seconds;
    unsigned long long hours;
    unsigned long long minutes;
    if (pane == NULL || pane->summary_label == NULL) return;
    browser_listing_summary(&pane->listing, &summary);
    seconds = summary.known_duration_ms / 1000ULL;
    hours = seconds / 3600ULL;
    minutes = (seconds / 60ULL) % 60ULL;
    seconds %= 60ULL;
    swprintf_s(text, ARRAYSIZE(text),
        TR(summary.unknown_duration_count == 0 ? UI_DIRECTORY_SUMMARY_FORMAT :
                                                UI_DIRECTORY_SUMMARY_UNKNOWN_FORMAT),
        summary.audio_file_count, (double)summary.total_size_bytes / 1000000.0,
        hours, minutes, seconds, summary.unknown_duration_count);
    SetWindowTextW(pane->summary_label, text);
}

static void render_pane_rows(Pane *pane)
{
    size_t index;
    if (pane == NULL || pane->list == NULL) return;
    if (pane->listing.count < SIZE_MAX &&
        pane->entry_index_capacity < pane->listing.count + 1) {
        size_t *grown = (size_t *)realloc(
            pane->entry_index_by_id,
            (pane->listing.count + 1) * sizeof(*pane->entry_index_by_id));
        if (grown != NULL) {
            pane->entry_index_by_id = grown;
            pane->entry_index_capacity = pane->listing.count + 1;
        }
    }
    if (pane->listing.count < SIZE_MAX &&
        pane->entry_index_capacity >= pane->listing.count + 1) {
        for (index = 0; index <= pane->listing.count; ++index)
            pane->entry_index_by_id[index] = SIZE_MAX;
        for (index = 0; index < pane->listing.count; ++index) {
            unsigned long long id = pane->listing.entries[index].entry_id;
            if (id <= pane->listing.count)
                pane->entry_index_by_id[(size_t)id] = index;
        }
    }
    SendMessageW(pane->list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(pane->list);
    for (index = 0; index < pane->listing.count; ++index) {
        const BrowserEntry *entry = &pane->listing.entries[index];
        LVITEMW item = {0};
        wchar_t size_text[32] = L"";
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = (int)index;
        item.pszText = entry->name;
        item.lParam = (LPARAM)index;
        ListView_InsertItem(pane->list, &item);
        if (entry->kind == BROWSER_ENTRY_DIRECTORY) {
            ListView_SetItemText(pane->list, (int)index, 1, (LPWSTR)TR(UI_FOLDER_LABEL));
        } else {
            wchar_t duration[32] = L"--:--";
            if (entry->duration_ms > 0) {
                unsigned long seconds = entry->duration_ms / 1000;
                swprintf_s(duration, ARRAYSIZE(duration), L"%lu:%02lu",
                           seconds / 60, seconds % 60);
            }
            swprintf_s(size_text, ARRAYSIZE(size_text), L"%llu KB",
                       (entry->size + 1023) / 1024);
            ListView_SetItemText(pane->list, (int)index, 1, size_text);
            ListView_SetItemText(pane->list, (int)index, 2, duration);
        }
    }
    SendMessageW(pane->list, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(pane->list, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

static unsigned long long *capture_selected_entry_ids(Pane *pane, size_t *selected_count)
{
    unsigned long long *ids;
    size_t capacity;
    int item = -1;
    *selected_count = 0;
    capacity = (size_t)ListView_GetSelectedCount(pane->list);
    if (capacity == 0 || capacity > SIZE_MAX / sizeof(*ids)) return NULL;
    ids = (unsigned long long *)calloc(capacity, sizeof(*ids));
    if (ids == NULL) return NULL;
    while (*selected_count < capacity &&
           (item = ListView_GetNextItem(pane->list, item, LVNI_SELECTED)) >= 0) {
        if ((size_t)item < pane->listing.count)
            ids[(*selected_count)++] = pane->listing.entries[item].entry_id;
    }
    return ids;
}

static void restore_selected_entry_ids(Pane *pane,
                                       const unsigned long long *ids,
                                       size_t selected_count)
{
    size_t index;
    BOOL focused = FALSE;
    for (index = 0; index < pane->listing.count; ++index) {
        size_t selected_index;
        for (selected_index = 0; selected_index < selected_count; ++selected_index) {
            if (pane->listing.entries[index].entry_id == ids[selected_index]) {
                UINT state = LVIS_SELECTED | (focused ? 0 : LVIS_FOCUSED);
                ListView_SetItemState(pane->list, (int)index, state,
                                      LVIS_SELECTED | LVIS_FOCUSED);
                focused = TRUE;
                break;
            }
        }
    }
}

static void sort_existing_pane(Pane *pane)
{
    size_t selected_count = 0;
    unsigned long long *selected_ids = capture_selected_entry_ids(pane, &selected_count);
    browser_listing_sort(&pane->listing, pane->sort_column, pane->sort_descending != FALSE);
    render_pane_rows(pane);
    if (selected_ids != NULL)
        restore_selected_entry_ids(pane, selected_ids, selected_count);
    free(selected_ids);
    update_sort_header(pane);
    update_directory_summary(pane);
    update_info_panel(pane);
    update_operation_state();
}

static void apply_metadata_batch(MetadataBatch *batch)
{
    Pane *pane;
    size_t update_index;
    if (batch == NULL || batch->pane_index >= ARRAYSIZE(panes)) return;
    pane = &panes[batch->pane_index];
    if (batch->generation != pane->metadata_generation) return;
    for (update_index = 0; update_index < batch->count; ++update_index) {
        unsigned long long id = batch->updates[update_index].entry_id;
        size_t entry_index = SIZE_MAX;
        BrowserEntry *entry;
        wchar_t duration[32] = L"--:--";
        if (id < pane->entry_index_capacity)
            entry_index = pane->entry_index_by_id[(size_t)id];
        if (entry_index >= pane->listing.count ||
            pane->listing.entries[entry_index].entry_id != id) {
            for (entry_index = 0; entry_index < pane->listing.count; ++entry_index) {
                if (pane->listing.entries[entry_index].entry_id == id) break;
            }
            if (entry_index == pane->listing.count) continue;
        }
        entry = &pane->listing.entries[entry_index];
        entry->duration_ms = batch->updates[update_index].duration_ms;
        if (entry->duration_ms > 0) {
            unsigned long seconds = entry->duration_ms / 1000;
            swprintf_s(duration, ARRAYSIZE(duration), L"%lu:%02lu",
                       seconds / 60, seconds % 60);
        }
        ListView_SetItemText(pane->list, (int)entry_index, 2, duration);
    }
    if (batch->complete) {
        pane->metadata_loading = FALSE;
        if (pane->sort_column == BROWSER_SORT_DURATION) {
            sort_existing_pane(pane);
            return;
        }
    }
    if (batch->complete) update_directory_summary(pane);
}

static void fill_pane(HWND owner, Pane *pane, const wchar_t *requested_folder)
{
    wchar_t *folder;
    BrowserListing next = {0};
    wchar_t **selected_names = NULL;
    size_t selected_count = 0;
    size_t index;
    size_t pane_index;
    if (pane == NULL || requested_folder == NULL || pane->path_edit == NULL ||
        pane->drive_combo == NULL || pane->list == NULL) {
        SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return;
    }
    folder = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*folder));
    if (folder == NULL) return;
    if (!browser_normalize_folder(requested_folder, folder, PATH_CAPACITY) ||
        !browser_list_folder(folder, &next)) {
        show_error(owner, TR(UI_FOLDER_OPEN_FAILED));
        SetWindowTextW(pane->path_edit, pane->folder);
        free(folder);
        return;
    }
    if (pane->list != NULL && pane->folder[0] != L'\0' &&
        _wcsicmp(folder, pane->folder) == 0) {
        int item = -1;
        size_t capacity = (size_t)ListView_GetSelectedCount(pane->list);
        if (capacity > 0)
            selected_names = (wchar_t **)calloc(capacity, sizeof(*selected_names));
        while (selected_names != NULL && selected_count < capacity &&
               (item = ListView_GetNextItem(pane->list, item, LVNI_SELECTED)) >= 0) {
            if ((size_t)item < pane->listing.count) {
                wchar_t *name = duplicate_wide_text(pane->listing.entries[item].name);
                if (name != NULL) selected_names[selected_count++] = name;
            }
        }
    }
    browser_listing_sort(&next, pane->sort_column, pane->sort_descending != FALSE);
    browser_listing_free(&pane->listing);
    pane->listing = next;
    wcscpy_s(pane->folder, ARRAYSIZE(pane->folder), folder);
    free(folder);
    SetWindowTextW(pane->path_edit, pane->folder);
    refresh_drive_list(pane);
    render_pane_rows(pane);
    update_sort_header(pane);
    update_directory_summary(pane);
    if (selected_names != NULL) {
        BOOL focused = FALSE;
        for (index = 0; index < pane->listing.count; ++index) {
            size_t selected_index;
            for (selected_index = 0; selected_index < selected_count; ++selected_index) {
                if (_wcsicmp(pane->listing.entries[index].name,
                             selected_names[selected_index]) == 0) {
                    UINT state = LVIS_SELECTED | (focused ? 0 : LVIS_FOCUSED);
                    ListView_SetItemState(pane->list, (int)index, state,
                                          LVIS_SELECTED | LVIS_FOCUSED);
                    focused = TRUE;
                    break;
                }
            }
        }
        for (index = 0; index < selected_count; ++index) free(selected_names[index]);
        free(selected_names);
    }
    update_info_panel(pane);
    update_operation_state();
    pane_index = (size_t)(pane - panes);
    pane->metadata_generation = InterlockedIncrement(&pane->metadata_generation);
    if (pane->metadata_generation <= 0) {
        InterlockedExchange(&pane->metadata_generation, 1);
        pane->metadata_generation = 1;
    }
    pane->metadata_loading = FALSE;
    if (metadata_loader != NULL) {
        metadata_loader_cancel(metadata_loader, pane_index, pane->metadata_generation);
        pane->metadata_loading = metadata_loader_submit(
            metadata_loader, pane_index, pane->metadata_generation,
            pane->folder, &pane->listing);
    }
}

static void navigate_edit(HWND owner, Pane *pane)
{
    wchar_t *path = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*path));
    if (path == NULL) return;
    GetWindowTextW(pane->path_edit, path, PATH_CAPACITY);
    fill_pane(owner, pane, path);
    free(path);
}

static void browse_for_folder(HWND owner, Pane *pane)
{
    HRESULT com_result;
    BOOL uninitialize;
    IFileDialog *dialog = NULL;
    IShellItem *initial = NULL;
    IShellItem *chosen = NULL;
    PWSTR selected = NULL;
    FILEOPENDIALOGOPTIONS options;
    BOOL failed = FALSE;
    if (pane == NULL) return;
    com_result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    uninitialize = SUCCEEDED(com_result);
    if (FAILED(com_result) && com_result != RPC_E_CHANGED_MODE) {
        show_error(owner, TR(UI_FOLDER_OPEN_FAILED));
        return;
    }
    com_result = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IFileDialog, (void **)&dialog);
    if (FAILED(com_result)) {
        failed = TRUE;
        goto done;
    }
    if (FAILED(IFileDialog_GetOptions(dialog, &options)) ||
        FAILED(IFileDialog_SetOptions(dialog, options | FOS_PICKFOLDERS |
                                      FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST |
                                      FOS_NOCHANGEDIR | FOS_DONTADDTORECENT))) {
        failed = TRUE;
        goto done;
    }
    IFileDialog_SetTitle(dialog, TR(UI_BROWSE));
    if (SUCCEEDED(SHCreateItemFromParsingName(pane->folder, NULL, &IID_IShellItem,
                                              (void **)&initial)))
        IFileDialog_SetFolder(dialog, initial);
    com_result = IFileDialog_Show(dialog, owner);
    if (com_result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) goto done;
    if (FAILED(com_result) || FAILED(IFileDialog_GetResult(dialog, &chosen)) ||
        FAILED(IShellItem_GetDisplayName(chosen, SIGDN_FILESYSPATH, &selected))) {
        failed = TRUE;
        goto done;
    }
    fill_pane(owner, pane, selected);
done:
    if (selected != NULL) CoTaskMemFree(selected);
    if (chosen != NULL) IShellItem_Release(chosen);
    if (initial != NULL) IShellItem_Release(initial);
    if (dialog != NULL) IFileDialog_Release(dialog);
    if (uninitialize) CoUninitialize();
    if (failed) show_error(owner, TR(UI_FOLDER_OPEN_FAILED));
}

static void activate_item(HWND owner, Pane *pane, int item_index)
{
    wchar_t *path;
    const BrowserEntry *entry;
    if (item_index < 0 || (size_t)item_index >= pane->listing.count) {
        return;
    }
    path = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*path));
    if (path == NULL) return;
    entry = &pane->listing.entries[item_index];
    if (!browser_join_path(pane->folder, entry->name, path, PATH_CAPACITY)) {
        show_error(owner, TR(UI_SELECTED_PATH_LONG));
        free(path);
        return;
    }
    if (entry->kind == BROWSER_ENTRY_DIRECTORY) {
        fill_pane(owner, pane, path);
    } else if (_wcsicmp(playing_path, path) == 0) {
        stop_playback();
    } else {
        playlist_clear(&playback_queue);
        if (playback_play(path)) {
            set_playing_path(path);
            if (app_settings.sequential_playback &&
                !playlist_build_after(&playback_queue, pane->folder,
                                      &pane->listing, (size_t)item_index))
                playlist_clear(&playback_queue);
        } else {
            stop_playback();
            show_error(owner, TR(UI_PLAY_FAILED));
        }
    }
    free(path);
}

static bool create_pane(HWND parent, Pane *pane, int base_id)
{
    LVCOLUMNW column = {0};
    if (parent == NULL || pane == NULL) return false;
    pane->sort_column = BROWSER_SORT_NAME;
    pane->sort_descending = FALSE;
    pane->drive_combo = CreateWindowW(WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                      0, 0, 0, 0, parent, (HMENU)(INT_PTR)(base_id - 1), NULL, NULL);
    pane->path_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                      0, 0, 0, 0, parent, (HMENU)(INT_PTR)base_id, NULL, NULL);
    pane->up_button = CreateWindowW(L"BUTTON", TR(UI_UP), WS_CHILD | WS_VISIBLE,
                                    0, 0, 0, 0, parent, (HMENU)(INT_PTR)(base_id + 1), NULL, NULL);
    pane->refresh_button = CreateWindowW(L"BUTTON", TR(UI_REFRESH), WS_CHILD | WS_VISIBLE,
                                         0, 0, 0, 0, parent, (HMENU)(INT_PTR)(base_id + 2), NULL, NULL);
    pane->browse_button = CreateWindowW(L"BUTTON", TR(UI_BROWSE), WS_CHILD | WS_VISIBLE,
                                        0, 0, 0, 0, parent,
                                        (HMENU)(INT_PTR)(base_id + 4), NULL, NULL);
    pane->summary_label = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE |
                                        SS_CENTER | SS_CENTERIMAGE,
                                        0, 0, 0, 0, parent,
                                        (HMENU)(INT_PTR)(base_id + 5), NULL, NULL);
    pane->list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                 WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_EDITLABELS,
                                 0, 0, 0, 0, parent, (HMENU)(INT_PTR)(base_id + 3), NULL, NULL);
    pane->info_panel = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                                       TR(UI_SELECT_AUDIO_INFO),
                                       WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
                                       0, 0, 0, 0, parent, NULL, NULL, NULL);
    if (pane->drive_combo == NULL || pane->path_edit == NULL ||
        pane->up_button == NULL || pane->refresh_button == NULL || pane->browse_button == NULL ||
        pane->summary_label == NULL || pane->list == NULL || pane->info_panel == NULL ||
        !SetWindowSubclass(pane->path_edit, path_subclass, 1, (DWORD_PTR)parent))
        return false;
    ListView_SetExtendedListViewStyle(pane->list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.cx = 280;
    column.pszText = (LPWSTR)TR(UI_NAME);
    if (ListView_InsertColumn(pane->list, 0, &column) < 0) return false;
    column.cx = 90;
    column.pszText = (LPWSTR)TR(UI_SIZE);
    if (ListView_InsertColumn(pane->list, 1, &column) < 0) return false;
    column.cx = 72;
    column.pszText = (LPWSTR)TR(UI_DURATION);
    if (ListView_InsertColumn(pane->list, 2, &column) < 0) return false;
    return true;
}

static void set_list_column_text(HWND list, int index, const wchar_t *text)
{
    LVCOLUMNW column = {0};
    column.mask = LVCF_TEXT;
    column.pszText = (LPWSTR)text;
    ListView_SetColumn(list, index, &column);
}

static void apply_language(HWND window)
{
    int index;
    SetWindowTextW(window, TR(UI_APP_TITLE));
    SetWindowTextW(GetDlgItem(window, ID_SETTINGS), TR(UI_SETTINGS));
    SetWindowTextW(GetDlgItem(window, ID_INFO), info_visible ? TR(UI_INFO_EXPANDED) : TR(UI_INFO));
    SetWindowTextW(GetDlgItem(window, ID_HELP), TR(UI_HELP));
    SetWindowTextW(operation_buttons[0][0], TR(UI_COPY_RIGHT));
    SetWindowTextW(operation_buttons[0][1], TR(UI_MOVE_RIGHT));
    SetWindowTextW(operation_buttons[0][2], TR(UI_DELETE));
    SetWindowTextW(operation_buttons[1][0], TR(UI_COPY_LEFT));
    SetWindowTextW(operation_buttons[1][1], TR(UI_MOVE_LEFT));
    SetWindowTextW(operation_buttons[1][2], TR(UI_DELETE));
    SetWindowTextW(stop_button, TR(UI_STOP));
    SetWindowTextW(swap_button, TR(UI_SWAP));
    SetWindowTextW(refresh_all_button, TR(UI_REFRESH));
    for (index = 0; index < 2; ++index) {
        SetWindowTextW(panes[index].up_button, TR(UI_UP));
        SetWindowTextW(panes[index].refresh_button, TR(UI_REFRESH));
        SetWindowTextW(panes[index].browse_button, TR(UI_BROWSE));
        set_list_column_text(panes[index].list, 0, TR(UI_NAME));
        set_list_column_text(panes[index].list, 1, TR(UI_SIZE));
        set_list_column_text(panes[index].list, 2, TR(UI_DURATION));
        update_directory_summary(&panes[index]);
        update_info_panel(&panes[index]);
    }
}

static void layout_panes(HWND window)
{
    RECT area;
    HDC dc;
    TEXTMETRICW metrics = {0};
    int index;
    int scale = app_settings.interface_scale;
    int margin = MulDiv(8, scale, 100);
    int gap = MulDiv(8, scale, 100);
    int drive_width = MulDiv(64, scale, 100);
    int top_height, button_width, browse_width, settings_width, info_width, help_width, toolbar_height;
    int center_width, center_button_width, center_group_width, pane_width;
    GetClientRect(window, &area);
    dc = GetDC(window);
    if (dc != NULL) {
        HFONT old = (HFONT)SelectObject(dc, app_font);
        SIZE size;
        GetTextMetricsW(dc, &metrics);
        GetTextExtentPoint32W(dc, TR(UI_REFRESH), lstrlenW(TR(UI_REFRESH)), &size);
        button_width = max(MulDiv(70, scale, 100), size.cx + MulDiv(24, scale, 100));
        GetTextExtentPoint32W(dc, TR(UI_BROWSE), lstrlenW(TR(UI_BROWSE)), &size);
        browse_width = max(MulDiv(76, scale, 100), size.cx + MulDiv(20, scale, 100));
        GetTextExtentPoint32W(dc, TR(UI_SETTINGS), lstrlenW(TR(UI_SETTINGS)), &size);
        settings_width = max(MulDiv(100, scale, 100), size.cx + MulDiv(24, scale, 100));
        GetTextExtentPoint32W(dc, TR(UI_INFO), lstrlenW(TR(UI_INFO)), &size);
        info_width = max(MulDiv(72, scale, 100), size.cx + MulDiv(24, scale, 100));
        GetTextExtentPoint32W(dc, TR(UI_HELP), lstrlenW(TR(UI_HELP)), &size);
        help_width = max(MulDiv(72, scale, 100), size.cx + MulDiv(24, scale, 100));
        SelectObject(dc, old);
        ReleaseDC(window, dc);
    } else {
        button_width = 70; browse_width = 76; settings_width = 100;
        info_width = 72; help_width = 72; metrics.tmHeight = 16;
    }
    top_height = max(MulDiv(30, scale, 100), metrics.tmHeight + MulDiv(12, scale, 100));
    toolbar_height = top_height + MulDiv(6, scale, 100);
    center_width = max(4, MulDiv(9, scale, 100));
    center_button_width = max(MulDiv(64, scale, 100), button_width);
    center_group_width = center_button_width * 3 + gap * 2;
    pane_width = max(160, (area.right - margin * 2 - center_width) / 2);
    for (index = 0; index < 2; ++index) {
        int x = margin + index * (pane_width + center_width);
        int edit_width = pane_width - drive_width - button_width * 2 - browse_width - gap * 4;
        int list_y;
        int summary_gap = max(2, MulDiv(4, scale, 100));
        int summary_height = max(MulDiv(18, scale, 100),
                                 metrics.tmHeight + MulDiv(2, scale, 100));
        int info_height = info_visible ? max(MulDiv(120, scale, 100), metrics.tmHeight * 7) : 0;
        int list_height;
        MoveWindow(panes[index].drive_combo, x, margin + toolbar_height, drive_width, 240, TRUE);
        MoveWindow(panes[index].path_edit, x + drive_width + gap, margin + toolbar_height, edit_width, top_height, TRUE);
        MoveWindow(panes[index].browse_button, x + drive_width + gap + edit_width + gap,
                   margin + toolbar_height, browse_width, top_height, TRUE);
        MoveWindow(panes[index].up_button, x + drive_width + gap + edit_width + gap + browse_width + gap,
                   margin + toolbar_height, button_width, top_height, TRUE);
        MoveWindow(panes[index].refresh_button,
                   x + drive_width + gap + edit_width + gap + browse_width + gap + button_width + gap,
                   margin + toolbar_height, button_width, top_height, TRUE);
        list_y = margin + toolbar_height + top_height + gap;
        {
            int intrusion = max(0, (center_group_width - center_width) / 2 + gap);
            int action_area = max(MulDiv(150, scale, 100), pane_width - intrusion);
            int action_start = index == 0 ? x : x + intrusion;
            int action_width = max(MulDiv(45, scale, 100), (action_area - gap * 2) / 3);
            int action_index;
            for (action_index = 0; action_index < 3; ++action_index)
                MoveWindow(operation_buttons[index][action_index], action_start + action_index * (action_width + gap),
                           list_y, action_width, top_height, TRUE);
            list_y += top_height + summary_gap;
        }
        MoveWindow(panes[index].summary_label, x, list_y, pane_width, summary_height, TRUE);
        list_y += summary_height + summary_gap;
        list_height = area.bottom - margin - list_y - (info_visible ? info_height + gap : 0);
        MoveWindow(panes[index].list, x, list_y, pane_width, max(60, list_height), TRUE);
        MoveWindow(panes[index].info_panel, x, list_y + max(60, list_height) + gap,
                   pane_width, info_height, TRUE);
        ShowWindow(panes[index].info_panel, info_visible ? SW_SHOW : SW_HIDE);
        ListView_SetColumnWidth(panes[index].list, 0, max(MulDiv(100, scale, 100), pane_width - MulDiv(180, scale, 100)));
        ListView_SetColumnWidth(panes[index].list, 1, MulDiv(90, scale, 100));
        ListView_SetColumnWidth(panes[index].list, 2, MulDiv(72, scale, 100));
    }
    {
        int center_start = margin + pane_width + (center_width - center_group_width) / 2;
        int center_y = margin + toolbar_height + top_height + gap;
        MoveWindow(stop_button, center_start, center_y, center_button_width, top_height, TRUE);
        MoveWindow(swap_button, center_start + center_button_width + gap, center_y,
                   center_button_width, top_height, TRUE);
        MoveWindow(refresh_all_button, center_start + (center_button_width + gap) * 2,
                   center_y, center_button_width, top_height, TRUE);
    }
    {
        int help_x = area.right - margin - help_width;
        int info_x = help_x - gap - info_width;
        int settings_x = info_x - gap - settings_width;
        int available = max(80, settings_x - margin - gap);
        int volume_width = min(170, max(80, available / 3));
        int progress_width = max(80, available - volume_width - gap);
        MoveWindow(progress_bar, margin, margin + 3, progress_width, max(18, top_height - 6), TRUE);
        MoveWindow(volume_slider, margin + progress_width + gap, margin, volume_width, top_height, TRUE);
        MoveWindow(GetDlgItem(window, ID_SETTINGS), settings_x, margin, settings_width, top_height, TRUE);
        MoveWindow(GetDlgItem(window, ID_INFO), info_x, margin, info_width, top_height, TRUE);
        MoveWindow(GetDlgItem(window, ID_HELP), help_x, margin, help_width, top_height, TRUE);
    }
}

static LRESULT CALLBACK help_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message) {
    case WM_CREATE:
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", localization_help_text(app_settings.language),
                        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
                        0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_HELP_TEXT, NULL, NULL);
        CreateWindowW(L"BUTTON", TR(UI_CLOSE), WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_HELP_CLOSE, NULL, NULL);
        SendDlgItemMessageW(window, ID_HELP_TEXT, WM_SETFONT, (WPARAM)help_font, TRUE);
        SendDlgItemMessageW(window, ID_HELP_CLOSE, WM_SETFONT, (WPARAM)help_font, TRUE);
        return 0;
    case WM_SIZE: {
        RECT area;
        const int margin = 18, button_width = 110, button_height = 36;
        GetClientRect(window, &area);
        MoveWindow(GetDlgItem(window, ID_HELP_TEXT), margin, margin,
                   max(100, area.right - margin * 2), max(100, area.bottom - margin * 3 - button_height), TRUE);
        MoveWindow(GetDlgItem(window, ID_HELP_CLOSE), area.right - margin - button_width,
                   area.bottom - margin - button_height, button_width, button_height, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(w_param) == ID_HELP_CLOSE) { DestroyWindow(window); return 0; }
        break;
    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)w_param, app_colors.text);
        SetBkColor((HDC)w_param, app_colors.control);
        return (LRESULT)app_control_brush;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_NCDESTROY:
        help_window = NULL;
        return 0;
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

static void show_help(HWND owner)
{
    AppSettings help_settings = app_settings;
    RECT owner_rect;
    MSG message;
    const int width = 616, height = 495;
    help_settings.font_points = min(72, app_settings.font_points + 2);
    help_font = settings_create_font(&help_settings, owner);
    GetWindowRect(owner, &owner_rect);
    help_window = CreateWindowExW(WS_EX_DLGMODALFRAME, HELP_CLASS, TR(UI_HELP_TITLE),
                                  WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
                                  owner_rect.left + ((owner_rect.right - owner_rect.left) - width) / 2,
                                  owner_rect.top + ((owner_rect.bottom - owner_rect.top) - height) / 2,
                                  width, height, owner, NULL, GetModuleHandleW(NULL), NULL);
    if (help_window == NULL) {
        if (help_font != NULL) DeleteObject(help_font);
        help_font = NULL;
        return;
    }
    EnableWindow(owner, FALSE);
    ShowWindow(help_window, SW_SHOW);
    while (help_window != NULL && GetMessageW(&message, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(help_window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);
    if (help_font != NULL) DeleteObject(help_font);
    help_font = NULL;
}

static void update_font_button(HWND window)
{
    wchar_t label[128];
    swprintf_s(label, ARRAYSIZE(label), L"%ls, %d pt...", settings_candidate.font_face,
               settings_candidate.font_points);
    SetWindowTextW(GetDlgItem(window, ID_FONT_CHOOSE), label);
}

static void choose_settings_font(HWND owner)
{
    LOGFONTW font = {0};
    CHOOSEFONTW chooser = {0};
    HDC dc = GetDC(owner);
    int dpi = dc != NULL ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc != NULL) ReleaseDC(owner, dc);
    font.lfHeight = -MulDiv(settings_candidate.font_points, dpi, 72);
    wcscpy_s(font.lfFaceName, ARRAYSIZE(font.lfFaceName), settings_candidate.font_face);
    chooser.lStructSize = sizeof(chooser);
    chooser.hwndOwner = owner;
    chooser.lpLogFont = &font;
    chooser.iPointSize = settings_candidate.font_points * 10;
    chooser.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_LIMITSIZE;
    chooser.nSizeMin = 6;
    chooser.nSizeMax = 72;
    if (ChooseFontW(&chooser)) {
        wcscpy_s(settings_candidate.font_face, ARRAYSIZE(settings_candidate.font_face), font.lfFaceName);
        settings_candidate.font_points = chooser.iPointSize / 10;
        update_font_button(owner);
    }
}

static BOOL CALLBACK set_child_font(HWND child, LPARAM font);

static LRESULT CALLBACK settings_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    int index;
    switch (message) {
    case WM_CREATE: {
        HWND control;
        wchar_t number[16];
        int scale = 100;
#define SCALE_UI(value) MulDiv((value), scale, 100)
        CreateWindowW(L"STATIC", TR(UI_FONT_AND_SIZE), WS_CHILD | WS_VISIBLE,
                      SCALE_UI(24), SCALE_UI(22), SCALE_UI(452), SCALE_UI(22), window, NULL, NULL, NULL);
        control = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                SCALE_UI(24), SCALE_UI(48), SCALE_UI(452), SCALE_UI(34), window, (HMENU)(INT_PTR)ID_FONT_CHOOSE, NULL, NULL);
        control = CreateWindowW(L"BUTTON", TR(UI_MONOSPACE),
                                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                SCALE_UI(24), SCALE_UI(94), SCALE_UI(452), SCALE_UI(26), window, (HMENU)(INT_PTR)ID_MONOSPACE, NULL, NULL);
        SendMessageW(control, BM_SETCHECK, settings_candidate.monospace ? BST_CHECKED : BST_UNCHECKED, 0);
        control = CreateWindowW(L"BUTTON", TR(UI_REMEMBER_WINDOW),
                                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                SCALE_UI(24), SCALE_UI(126), SCALE_UI(452), SCALE_UI(26),
                                window, (HMENU)(INT_PTR)ID_REMEMBER_POSITION, NULL, NULL);
        SendMessageW(control, BM_SETCHECK, settings_candidate.remember_window ? BST_CHECKED : BST_UNCHECKED, 0);
        control = CreateWindowW(L"BUTTON", TR(UI_PLAY_NEXT_AUTOMATICALLY),
                                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                SCALE_UI(24), SCALE_UI(158), SCALE_UI(452), SCALE_UI(26),
                                window, (HMENU)(INT_PTR)ID_SEQUENTIAL_PLAYBACK, NULL, NULL);
        SendMessageW(control, BM_SETCHECK,
                     settings_candidate.sequential_playback ? BST_CHECKED : BST_UNCHECKED, 0);
        control = CreateWindowW(L"BUTTON", TR(UI_REMEMBER_DIRECTORIES),
                                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                SCALE_UI(24), SCALE_UI(190), SCALE_UI(500), SCALE_UI(26),
                                window, (HMENU)(INT_PTR)ID_REMEMBER_DIRECTORIES, NULL, NULL);
        SendMessageW(control, BM_SETCHECK,
                     settings_candidate.remember_directories ? BST_CHECKED : BST_UNCHECKED, 0);
        CreateWindowW(L"STATIC", TR(UI_INTERFACE_SIZE), WS_CHILD | WS_VISIBLE,
                      SCALE_UI(24), SCALE_UI(230), SCALE_UI(190), SCALE_UI(22), window, NULL, NULL, NULL);
        CreateWindowW(L"STATIC", TR(UI_TRANSPARENCY), WS_CHILD | WS_VISIBLE,
                      SCALE_UI(270), SCALE_UI(230), SCALE_UI(190), SCALE_UI(22), window, NULL, NULL, NULL);
        swprintf_s(number, ARRAYSIZE(number), L"%d", settings_candidate.interface_scale);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", number, WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL,
                        SCALE_UI(24), SCALE_UI(254), SCALE_UI(160), SCALE_UI(30),
                        window, (HMENU)(INT_PTR)ID_INTERFACE_SIZE, NULL, NULL);
        CreateWindowW(L"STATIC", L"%  (75–200)", WS_CHILD | WS_VISIBLE,
                      SCALE_UI(190), SCALE_UI(258), SCALE_UI(80), SCALE_UI(22), window, NULL, NULL, NULL);
        swprintf_s(number, ARRAYSIZE(number), L"%d", 100 - settings_candidate.opacity);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", number, WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL,
                        SCALE_UI(270), SCALE_UI(254), SCALE_UI(160), SCALE_UI(30),
                        window, (HMENU)(INT_PTR)ID_OPACITY, NULL, NULL);
        CreateWindowW(L"STATIC", L"%  (0-50)", WS_CHILD | WS_VISIBLE,
                      SCALE_UI(436), SCALE_UI(258), SCALE_UI(70), SCALE_UI(22), window, NULL, NULL, NULL);
        CreateWindowW(L"STATIC", TR(UI_THEME), WS_CHILD | WS_VISIBLE,
                      SCALE_UI(24), SCALE_UI(304), SCALE_UI(452), SCALE_UI(22), window, NULL, NULL, NULL);
        control = CreateWindowW(WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                SCALE_UI(24), SCALE_UI(330), SCALE_UI(452), SCALE_UI(240), window, (HMENU)(INT_PTR)ID_THEME_COMBO, NULL, NULL);
        {
            int selected_item = 0;
            for (index = 0; index < APP_THEME_COUNT; ++index) {
                AppTheme theme = (AppTheme)index;
                int item;
                if (!settings_theme_available(theme)) continue;
                item = (int)SendMessageW(
                    control, CB_ADDSTRING, 0,
                    (LPARAM)localization_theme_name(
                        settings_candidate.language, theme));
                if (item >= 0) {
                    SendMessageW(control, CB_SETITEMDATA, item, (LPARAM)theme);
                    if (theme == settings_candidate.theme) selected_item = item;
                }
            }
            SendMessageW(control, CB_SETCURSEL, selected_item, 0);
        }
        CreateWindowW(L"STATIC", TR(UI_LANGUAGE), WS_CHILD | WS_VISIBLE,
                      SCALE_UI(24), SCALE_UI(370), SCALE_UI(452), SCALE_UI(22), window, NULL, NULL, NULL);
        control = CreateWindowW(WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                SCALE_UI(24), SCALE_UI(396), SCALE_UI(452), SCALE_UI(180),
                                window, (HMENU)(INT_PTR)ID_LANGUAGE_COMBO, NULL, NULL);
        for (index = 0; index < APP_LANGUAGE_COUNT; ++index)
            SendMessageW(control, CB_ADDSTRING, 0, (LPARAM)settings_language_name((AppLanguage)index));
        SendMessageW(control, CB_SETCURSEL, settings_candidate.language, 0);
        CreateWindowW(L"BUTTON", TR(UI_OK), WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      SCALE_UI(286), SCALE_UI(460), SCALE_UI(90), SCALE_UI(32), window, (HMENU)(INT_PTR)ID_SETTINGS_OK, NULL, NULL);
        CreateWindowW(L"BUTTON", TR(UI_CANCEL), WS_CHILD | WS_VISIBLE,
                      SCALE_UI(386), SCALE_UI(460), SCALE_UI(90), SCALE_UI(32), window, (HMENU)(INT_PTR)ID_SETTINGS_CANCEL, NULL, NULL);
        EnumChildWindows(window, set_child_font, (LPARAM)app_font);
        update_font_button(window);
#undef SCALE_UI
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(w_param) == ID_FONT_CHOOSE) {
            choose_settings_font(window);
            return 0;
        }
        if (LOWORD(w_param) == ID_SETTINGS_OK) {
            int theme_item = (int)SendDlgItemMessageW(
                window, ID_THEME_COMBO, CB_GETCURSEL, 0, 0);
            int language = (int)SendDlgItemMessageW(window, ID_LANGUAGE_COMBO, CB_GETCURSEL, 0, 0);
            if (theme_item >= 0) {
                LRESULT theme = SendDlgItemMessageW(
                    window, ID_THEME_COMBO, CB_GETITEMDATA, theme_item, 0);
                if (theme >= 0 && theme < APP_THEME_COUNT)
                    settings_candidate.theme = (AppTheme)theme;
            }
            if (language >= 0) settings_candidate.language = (AppLanguage)language;
            settings_candidate.monospace = SendDlgItemMessageW(window, ID_MONOSPACE, BM_GETCHECK, 0, 0) == BST_CHECKED;
            settings_candidate.remember_window = SendDlgItemMessageW(window, ID_REMEMBER_POSITION, BM_GETCHECK, 0, 0) == BST_CHECKED;
            settings_candidate.sequential_playback = SendDlgItemMessageW(
                window, ID_SEQUENTIAL_PLAYBACK, BM_GETCHECK, 0, 0) == BST_CHECKED;
            settings_candidate.remember_directories = SendDlgItemMessageW(
                window, ID_REMEMBER_DIRECTORIES, BM_GETCHECK, 0, 0) == BST_CHECKED;
            {
                BOOL size_valid, transparency_valid;
                UINT interface_size = GetDlgItemInt(window, ID_INTERFACE_SIZE, &size_valid, FALSE);
                UINT transparency = GetDlgItemInt(window, ID_OPACITY, &transparency_valid, FALSE);
                if (!size_valid || interface_size < 75 || interface_size > 200) {
                    MessageBoxW(window, localization_text(settings_candidate.language, UI_INVALID_INTERFACE),
                                localization_text(settings_candidate.language, UI_INVALID_INTERFACE_TITLE),
                                MB_OK | MB_ICONWARNING);
                    SetFocus(GetDlgItem(window, ID_INTERFACE_SIZE));
                    return 0;
                }
                if (!transparency_valid || transparency > 50) {
                    MessageBoxW(window, localization_text(settings_candidate.language, UI_INVALID_TRANSPARENCY),
                                localization_text(settings_candidate.language, UI_INVALID_TRANSPARENCY_TITLE),
                                MB_OK | MB_ICONWARNING);
                    SetFocus(GetDlgItem(window, ID_OPACITY));
                    return 0;
                }
                settings_candidate.interface_scale = (int)interface_size;
                settings_candidate.opacity = 100 - (int)transparency;
            }
            settings_accepted = TRUE;
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(w_param) == ID_SETTINGS_CANCEL) {
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC:
        SetTextColor((HDC)w_param, app_colors.text);
        SetBkColor((HDC)w_param, app_colors.window);
        return (LRESULT)app_background_brush;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_NCDESTROY:
        settings_window = NULL;
        return 0;
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

static BOOL CALLBACK set_child_font(HWND child, LPARAM font)
{
    SendMessageW(child, WM_SETFONT, (WPARAM)font, TRUE);
    return TRUE;
}

static BOOL show_settings(HWND owner)
{
    MSG message;
    int scale;
    settings_candidate = app_settings;
    scale = 100;
    settings_accepted = FALSE;
    settings_window = CreateWindowExW(WS_EX_DLGMODALFRAME, SETTINGS_CLASS, TR(UI_SETTINGS_TITLE),
                                      WS_CAPTION | WS_SYSMENU,
                                      CW_USEDEFAULT, CW_USEDEFAULT, MulDiv(560, scale, 100), MulDiv(552, scale, 100),
                                      owner, NULL, GetModuleHandleW(NULL), NULL);
    if (settings_window == NULL) return FALSE;
    EnableWindow(owner, FALSE);
    ShowWindow(settings_window, SW_SHOW);
    while (settings_window != NULL && GetMessageW(&message, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(settings_window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);
    return settings_accepted;
}

static void draw_themed_button(const DRAWITEMSTRUCT *draw)
{
    RECT area = draw->rcItem;
    RECT text_area = area;
    wchar_t text[64];
    COLORREF top = app_colors.button_top;
    COLORREF bottom = app_colors.button_bottom;
    COLORREF border = app_colors.border;
    COLORREF foreground = app_colors.text;
    HPEN pen, old_pen;
    HBRUSH old_brush;
    HFONT old_font;
    if ((draw->itemState & ODS_SELECTED) != 0) {
        top = blend_color(top, app_colors.hot_border, 1, 3);
        bottom = blend_color(bottom, app_colors.hot_border, 1, 3);
        border = app_colors.hot_border;
        OffsetRect(&text_area, 1, 1);
    } else if ((draw->itemState & (ODS_HOTLIGHT | ODS_FOCUS)) != 0) {
        top = blend_color(top, RGB(255,255,255), 1, 3);
        border = app_colors.hot_border;
    }
    if ((draw->itemState & ODS_DISABLED) != 0) {
        top = blend_color(top, app_colors.window, 1, 2);
        bottom = blend_color(bottom, app_colors.window, 1, 2);
        foreground = GetSysColor(COLOR_GRAYTEXT);
    }
    if (draw->CtlID == ID_DELETE_LEFT || draw->CtlID == ID_DELETE_RIGHT)
        border = RGB(190,75,55);
    fill_vertical_gradient(draw->hDC, &area, top, bottom);
    pen = CreatePen(PS_SOLID, 1, border);
    old_pen = (HPEN)SelectObject(draw->hDC, pen);
    old_brush = (HBRUSH)SelectObject(draw->hDC, GetStockObject(NULL_BRUSH));
    Rectangle(draw->hDC, area.left, area.top, area.right, area.bottom);
    SelectObject(draw->hDC, old_brush);
    SelectObject(draw->hDC, old_pen);
    DeleteObject(pen);
    GetWindowTextW(draw->hwndItem, text, ARRAYSIZE(text));
    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC, foreground);
    old_font = (HFONT)SelectObject(draw->hDC, app_font);
    DrawTextW(draw->hDC, text, -1, &text_area,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(draw->hDC, old_font);
    if ((draw->itemState & ODS_FOCUS) != 0) {
        InflateRect(&area, -3, -3);
        DrawFocusRect(draw->hDC, &area);
    }
}

static void draw_special_button(const DRAWITEMSTRUCT *draw)
{
    RECT area = draw->rcItem;
    wchar_t text[64];
    COLORREF background = app_colors.control;
    COLORREF foreground = app_colors.text;
    HBRUSH brush;
    HPEN pen, old_pen;
    HBRUSH old_brush;
    HFONT old_font;
    if (draw->CtlID == ID_SWAP_DIRS) {
        background = RGB(GetRValue(app_colors.window) * 3 / 4,
                         GetGValue(app_colors.window) * 3 / 4,
                         GetBValue(app_colors.window) * 3 / 4);
    }
    if ((draw->itemState & ODS_SELECTED) != 0)
        background = RGB(GetRValue(background) * 4 / 5,
                         GetGValue(background) * 4 / 5,
                         GetBValue(background) * 4 / 5);
    if ((draw->itemState & ODS_DISABLED) != 0) foreground = GetSysColor(COLOR_GRAYTEXT);
    brush = CreateSolidBrush(background);
    FillRect(draw->hDC, &area, brush);
    DeleteObject(brush);
    pen = CreatePen(PS_SOLID, draw->CtlID == ID_SWAP_DIRS ? 1 : 2,
                    draw->CtlID == ID_SWAP_DIRS ? app_colors.text : RGB(210, 25, 25));
    old_pen = (HPEN)SelectObject(draw->hDC, pen);
    old_brush = (HBRUSH)SelectObject(draw->hDC, GetStockObject(NULL_BRUSH));
    Rectangle(draw->hDC, area.left, area.top, area.right, area.bottom);
    SelectObject(draw->hDC, old_brush);
    SelectObject(draw->hDC, old_pen);
    DeleteObject(pen);
    GetWindowTextW(draw->hwndItem, text, ARRAYSIZE(text));
    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC, foreground);
    old_font = (HFONT)SelectObject(draw->hDC, app_font);
    DrawTextW(draw->hDC, text, -1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(draw->hDC, old_font);
    if ((draw->itemState & ODS_FOCUS) != 0) {
        InflateRect(&area, -4, -4);
        DrawFocusRect(draw->hDC, &area);
    }
}

static LRESULT draw_themed_header(NMCUSTOMDRAW *custom, HWND header)
{
    if (custom->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
    if (custom->dwDrawStage == CDDS_ITEMPREPAINT) {
        wchar_t text[128] = L"";
        HDITEMW item = {0};
        RECT area = custom->rc;
        RECT text_area = area;
        HPEN pen, old_pen;
        HFONT old_font;
        item.mask = HDI_TEXT;
        item.pszText = text;
        item.cchTextMax = ARRAYSIZE(text);
        Header_GetItem(header, (int)custom->dwItemSpec, &item);
        fill_vertical_gradient(custom->hdc, &area,
                               app_colors.button_top,
                               app_colors.button_bottom);
        pen = CreatePen(PS_SOLID, 1, app_colors.border);
        old_pen = (HPEN)SelectObject(custom->hdc, pen);
        MoveToEx(custom->hdc, area.right - 1, area.top, NULL);
        LineTo(custom->hdc, area.right - 1, area.bottom);
        MoveToEx(custom->hdc, area.left, area.bottom - 1, NULL);
        LineTo(custom->hdc, area.right, area.bottom - 1);
        SelectObject(custom->hdc, old_pen);
        DeleteObject(pen);
        text_area.left += 7;
        text_area.right -= 4;
        SetBkMode(custom->hdc, TRANSPARENT);
        SetTextColor(custom->hdc, app_colors.text);
        old_font = (HFONT)SelectObject(custom->hdc, app_font);
        DrawTextW(custom->hdc, text, -1, &text_area,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(custom->hdc, old_font);
        return CDRF_SKIPDEFAULT;
    }
    return CDRF_DODEFAULT;
}

static void fit_rect_to_work_area(RECT *rect)
{
    HMONITOR monitor = MonitorFromRect(rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {sizeof(info)};
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;
    GetMonitorInfoW(monitor, &info);
    width = min(width, info.rcWork.right - info.rcWork.left);
    height = min(height, info.rcWork.bottom - info.rcWork.top);
    rect->left = max(info.rcWork.left, min(rect->left, info.rcWork.right - width));
    rect->top = max(info.rcWork.top, min(rect->top, info.rcWork.bottom - height));
    rect->right = rect->left + width;
    rect->bottom = rect->top + height;
}

static RECT initial_window_rect(void)
{
    RECT work;
    RECT result;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    if (app_settings.remember_window && app_settings.window_width >= 500 &&
        app_settings.window_height >= 400) {
        result.left = app_settings.window_x;
        result.top = app_settings.window_y;
        result.right = result.left + app_settings.window_width;
        result.bottom = result.top + app_settings.window_height;
        fit_rect_to_work_area(&result);
        return result;
    }
    {
        int work_width = work.right - work.left;
        int work_height = work.bottom - work.top;
        int height = work_height * 80 / 100;
        int width = min(work_width * 90 / 100, height * 8 / 5);
        result.left = work.left + (work_width - width) / 2;
        result.top = work.top + (work_height - height) / 2;
        result.right = result.left + width;
        result.bottom = result.top + height;
    }
    return result;
}

static void save_window_geometry(HWND window)
{
    WINDOWPLACEMENT placement = {sizeof(placement)};
    if (GetWindowPlacement(window, &placement)) {
        RECT rect = placement.rcNormalPosition;
        app_settings.window_x = rect.left;
        app_settings.window_y = rect.top;
        app_settings.window_width = rect.right - rect.left;
        app_settings.window_height = rect.bottom - rect.top;
        settings_save(&app_settings);
    }
}

static BOOL saved_directory_is_available(const wchar_t *requested)
{
    DWORD attributes;
    if (requested == NULL || requested[0] == L'\0') return FALSE;
    attributes = GetFileAttributesW(requested);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static void persist_last_directories(void)
{
    if (app_settings.remember_directories &&
        panes[0].folder[0] != L'\0' && panes[1].folder[0] != L'\0')
        settings_save_last_directories(panes[0].folder, panes[1].folder);
    else
        settings_clear_last_directories();
}

static BOOL finish_label_edit(HWND window, int pane_index,
                              const NMLVDISPINFOW *edit)
{
    wchar_t *old_path = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*old_path));
    wchar_t *new_path = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*new_path));
    const BrowserEntry *entry;
    BOOL renamed = FALSE;
    if (old_path == NULL || new_path == NULL) goto done;
    entry = &panes[pane_index].listing.entries[edit->item.iItem];
    if (!browser_join_path(panes[pane_index].folder, entry->name,
                           old_path, PATH_CAPACITY) ||
        !browser_join_path(panes[pane_index].folder, edit->item.pszText,
                           new_path, PATH_CAPACITY))
        goto done;
    if (file_ops_exists(new_path)) {
        show_error(window, TR(UI_RENAME_EXISTS));
        goto done;
    }
    if (playing_path[0] != L'\0' && path_contains(old_path, playing_path))
        stop_playback();
    if (!MoveFileW(old_path, new_path)) {
        show_last_error(window, TR(UI_RENAME_ACTION));
        goto done;
    }
    fill_pane(window, &panes[pane_index], panes[pane_index].folder);
    renamed = TRUE;
done:
    free(old_path);
    free(new_path);
    return renamed;
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    int index;
    switch (message) {
    case WM_CREATE: {
        wchar_t *current = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*current));
        wchar_t *saved_left = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*saved_left));
        wchar_t *saved_right = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*saved_right));
        if (current == NULL || saved_left == NULL || saved_right == NULL) {
            free(current);
            free(saved_left);
            free(saved_right);
            return -1;
        }
        saved_left[0] = L'\0';
        saved_right[0] = L'\0';
        progress_bar = CreateWindowExW(0, PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                       0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_PROGRESS, NULL, NULL);
        SetWindowSubclass(progress_bar, progress_subclass, 1, 0);
        volume_slider = CreateWindowExW(0, TRACKBAR_CLASSW, L"Volume", WS_CHILD | WS_VISIBLE |
                                        TBS_HORZ | TBS_AUTOTICKS | TBS_TOOLTIPS,
                                        0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_VOLUME, NULL, NULL);
        SendMessageW(volume_slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(volume_slider, TBM_SETPOS, TRUE, app_settings.volume);
        playback_set_volume(app_settings.volume);
        CreateWindowW(L"BUTTON", TR(UI_SETTINGS), WS_CHILD | WS_VISIBLE,
                      0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_SETTINGS, NULL, NULL);
        CreateWindowW(L"BUTTON", TR(UI_INFO), WS_CHILD | WS_VISIBLE,
                      0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_INFO, NULL, NULL);
        CreateWindowW(L"BUTTON", TR(UI_HELP), WS_CHILD | WS_VISIBLE,
                      0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_HELP, NULL, NULL);
        operation_buttons[0][0] = CreateWindowW(L"BUTTON", TR(UI_COPY_RIGHT), WS_CHILD | WS_VISIBLE,
                                                0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_COPY_RIGHT, NULL, NULL);
        operation_buttons[0][1] = CreateWindowW(L"BUTTON", TR(UI_MOVE_RIGHT), WS_CHILD | WS_VISIBLE,
                                                0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_MOVE_RIGHT, NULL, NULL);
        operation_buttons[0][2] = CreateWindowW(L"BUTTON", TR(UI_DELETE), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                                0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_DELETE_LEFT, NULL, NULL);
        operation_buttons[1][0] = CreateWindowW(L"BUTTON", TR(UI_COPY_LEFT), WS_CHILD | WS_VISIBLE,
                                                0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_COPY_LEFT, NULL, NULL);
        operation_buttons[1][1] = CreateWindowW(L"BUTTON", TR(UI_MOVE_LEFT), WS_CHILD | WS_VISIBLE,
                                                0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_MOVE_LEFT, NULL, NULL);
        operation_buttons[1][2] = CreateWindowW(L"BUTTON", TR(UI_DELETE), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                                0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_DELETE_RIGHT, NULL, NULL);
        swap_button = CreateWindowW(L"BUTTON", TR(UI_SWAP), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                    0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_SWAP_DIRS, NULL, NULL);
        stop_button = CreateWindowW(L"BUTTON", TR(UI_STOP), WS_CHILD | WS_VISIBLE | WS_DISABLED,
                                    0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_STOP_ALL, NULL, NULL);
        refresh_all_button = CreateWindowW(L"BUTTON", TR(UI_REFRESH), WS_CHILD | WS_VISIBLE,
                                           0, 0, 0, 0, window, (HMENU)(INT_PTR)ID_REFRESH_ALL, NULL, NULL);
        if (!create_pane(window, &panes[0], ID_LEFT_PATH) ||
            !create_pane(window, &panes[1], ID_RIGHT_PATH)) {
            free(current);
            free(saved_left);
            free(saved_right);
            return -1;
        }
        metadata_loader = metadata_loader_create(window, WM_METADATA_BATCH,
                                                 ARRAYSIZE(panes), NULL, NULL);
        if (GetCurrentDirectoryW(PATH_CAPACITY, current) == 0) {
            free(current);
            free(saved_left);
            free(saved_right);
            return -1;
        }
        if (app_settings.remember_directories)
            settings_load_last_directories(saved_left, PATH_CAPACITY,
                                           saved_right, PATH_CAPACITY);
        fill_pane(window, &panes[0],
                  saved_directory_is_available(saved_left) ? saved_left : current);
        fill_pane(window, &panes[1],
                  saved_directory_is_available(saved_right) ? saved_right : current);
        free(current);
        free(saved_left);
        free(saved_right);
        apply_language(window);
        apply_appearance(window);
        SetTimer(window, 1, 250, NULL);
        return 0;
    }
    case WM_SIZE:
        layout_panes(window);
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *limits = (MINMAXINFO *)l_param;
        int scale = app_settings.interface_scale > 0 ? app_settings.interface_scale : 100;
        limits->ptMinTrackSize.x = MulDiv(900, scale, 100);
        limits->ptMinTrackSize.y = MulDiv(500, scale, 100);
        return 0;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *draw = (DRAWITEMSTRUCT *)l_param;
        if (settings_theme_is_external(app_settings.theme) &&
            draw->CtlType == ODT_BUTTON) {
            draw_themed_button(draw);
            return TRUE;
        }
        if (draw->CtlID == ID_SWAP_DIRS || draw->CtlID == ID_DELETE_LEFT ||
            draw->CtlID == ID_DELETE_RIGHT) {
            draw_special_button(draw);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(w_param) == ID_SETTINGS) {
            if (show_settings(window)) {
                app_settings = settings_candidate;
                if (!app_settings.sequential_playback) playlist_clear(&playback_queue);
                settings_save(&app_settings);
                persist_last_directories();
                apply_language(window);
                apply_appearance(window);
                layout_panes(window);
            }
            return 0;
        }
        if (LOWORD(w_param) == ID_INFO) {
            info_visible = !info_visible;
            SetWindowTextW(GetDlgItem(window, ID_INFO),
                           info_visible ? TR(UI_INFO_EXPANDED) : TR(UI_INFO));
            update_info_panel(&panes[0]);
            update_info_panel(&panes[1]);
            layout_panes(window);
            return 0;
        }
        if (LOWORD(w_param) == ID_HELP) {
            show_help(window);
            return 0;
        }
        if (LOWORD(w_param) == ID_COPY_RIGHT) { transfer_between(window, 0, 1, FALSE); return 0; }
        if (LOWORD(w_param) == ID_MOVE_RIGHT) { transfer_between(window, 0, 1, TRUE); return 0; }
        if (LOWORD(w_param) == ID_DELETE_LEFT) { delete_selection(window, 0); return 0; }
        if (LOWORD(w_param) == ID_COPY_LEFT) { transfer_between(window, 1, 0, FALSE); return 0; }
        if (LOWORD(w_param) == ID_MOVE_LEFT) { transfer_between(window, 1, 0, TRUE); return 0; }
        if (LOWORD(w_param) == ID_DELETE_RIGHT) { delete_selection(window, 1); return 0; }
        if (LOWORD(w_param) == ID_SWAP_DIRS) { swap_directories(window); return 0; }
        if (LOWORD(w_param) == ID_STOP_ALL) { stop_playback(); return 0; }
        if (LOWORD(w_param) == ID_REFRESH_ALL) { refresh_all_panes(window); return 0; }
        if (LOWORD(w_param) == ID_REFRESH_ACTIVE) { refresh_active_pane(window); return 0; }
        if (LOWORD(w_param) == ID_ACCEL_COPY) {
            int source = active_pane_index(); transfer_between(window, source, 1 - source, FALSE); return 0;
        }
        if (LOWORD(w_param) == ID_ACCEL_MOVE) {
            int source = active_pane_index(); transfer_between(window, source, 1 - source, TRUE); return 0;
        }
        if (LOWORD(w_param) == ID_ACCEL_DELETE) {
            delete_selection(window, active_pane_index()); return 0;
        }
        for (index = 0; index < 2; ++index) {
            int base = index == 0 ? ID_LEFT_PATH : ID_RIGHT_PATH;
            if (LOWORD(w_param) == base - 1 && HIWORD(w_param) == CBN_SELCHANGE) {
                wchar_t root[4];
                int selected = (int)SendMessageW(panes[index].drive_combo, CB_GETCURSEL, 0, 0);
                if (selected >= 0) {
                    SendMessageW(panes[index].drive_combo, CB_GETLBTEXT, selected, (LPARAM)root);
                    fill_pane(window, &panes[index], root);
                }
                return 0;
            }
            if (LOWORD(w_param) == base + 1) {
                wchar_t *parent = (wchar_t *)malloc(PATH_CAPACITY * sizeof(*parent));
                if (parent != NULL &&
                    browser_parent_folder(panes[index].folder, parent, PATH_CAPACITY)) {
                    fill_pane(window, &panes[index], parent);
                }
                free(parent);
                return 0;
            }
            if (LOWORD(w_param) == base + 2) {
                fill_pane(window, &panes[index], panes[index].folder);
                return 0;
            }
            if (LOWORD(w_param) == base + 4) {
                browse_for_folder(window, &panes[index]);
                return 0;
            }
        }
        break;
    case WM_HSCROLL:
        if ((HWND)l_param == volume_slider) {
            app_settings.volume = (int)SendMessageW(volume_slider, TBM_GETPOS, 0, 0);
            playback_set_volume(app_settings.volume);
            settings_save(&app_settings);
            return 0;
        }
        break;
    case WM_TIMER:
        if (w_param == 1 && playing_path[0] != L'\0') {
            if (playback_is_active()) {
                playback_position = playback_position_ms();
                if (playback_total == 0) {
                    playback_total = playback_duration_ms();
                    if (playback_total > 0)
                        SendMessageW(progress_bar, PBM_SETRANGE32, 0, playback_total);
                }
                SendMessageW(progress_bar, PBM_SETPOS, playback_position, 0);
                InvalidateRect(progress_bar, NULL, FALSE);
            } else {
                if (app_settings.sequential_playback)
                    advance_playback_sequence();
                else
                    stop_playback();
            }
        }
        return 0;
    case WM_NAVIGATE_PATH:
        if ((int)w_param == ID_LEFT_PATH) navigate_edit(window, &panes[0]);
        if ((int)w_param == ID_RIGHT_PATH) navigate_edit(window, &panes[1]);
        return 0;
    case WM_NOTIFY: {
        NMHDR *header = (NMHDR *)l_param;
        if (header->code == LVN_COLUMNCLICK) {
            NMLISTVIEW *click = (NMLISTVIEW *)l_param;
            for (index = 0; index < 2; ++index) {
                if (header->hwndFrom == panes[index].list && click->iSubItem >= 0 &&
                    click->iSubItem <= 2) {
                    BrowserSortColumn column = (BrowserSortColumn)click->iSubItem;
                    if (panes[index].sort_column == column)
                        panes[index].sort_descending = !panes[index].sort_descending;
                    else {
                        panes[index].sort_column = column;
                        panes[index].sort_descending = FALSE;
                    }
                    sort_existing_pane(&panes[index]);
                    return 0;
                }
            }
        }
        if (header->code == NM_CUSTOMDRAW) {
            for (index = 0; index < 2; ++index) {
                HWND list_header = ListView_GetHeader(panes[index].list);
                if (header->hwndFrom == list_header &&
                    settings_theme_is_external(app_settings.theme))
                    return draw_themed_header(
                        (NMCUSTOMDRAW *)l_param, list_header);
                if (header->hwndFrom == panes[index].list) {
                    NMLVCUSTOMDRAW *custom = (NMLVCUSTOMDRAW *)l_param;
                    if (custom->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                    if (custom->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                        size_t item = (size_t)custom->nmcd.dwItemSpec;
                        if (settings_theme_is_external(app_settings.theme)) {
                            if ((custom->nmcd.uItemState & CDIS_SELECTED) != 0) {
                                custom->clrText = app_colors.selection_text;
                                custom->clrTextBk = app_colors.selection;
                            } else {
                                custom->clrText = app_colors.text;
                                custom->clrTextBk = app_colors.control;
                            }
                        }
                        if (item < panes[index].listing.count &&
                            panes[index].listing.entries[item].kind == BROWSER_ENTRY_DIRECTORY &&
                            app_bold_font != NULL) {
                            SelectObject(custom->nmcd.hdc, app_bold_font);
                            return CDRF_NEWFONT;
                        }
                    }
                    return CDRF_DODEFAULT;
                }
            }
        }
        if (header->code == LVN_KEYDOWN && ((NMLVKEYDOWN *)l_param)->wVKey == VK_RETURN) {
            for (index = 0; index < 2; ++index) {
                if (header->hwndFrom == panes[index].list) {
                    int item = ListView_GetNextItem(panes[index].list, -1, LVNI_FOCUSED);
                    if (item >= 0 && (size_t)item < panes[index].listing.count &&
                        panes[index].listing.entries[item].kind == BROWSER_ENTRY_AUDIO_FILE)
                        activate_item(window, &panes[index], item);
                    return 0;
                }
            }
        }
        if (header->code == NM_RCLICK) {
            for (index = 0; index < 2; ++index) {
                if (header->hwndFrom == panes[index].list) {
                    show_list_context_menu(window, &panes[index]);
                    return 0;
                }
            }
        }
        if (header->code == LVN_ENDLABELEDITW) {
            NMLVDISPINFOW *edit = (NMLVDISPINFOW *)l_param;
            if (edit->item.pszText == NULL || edit->item.pszText[0] == L'\0' ||
                wcspbrk(edit->item.pszText, L"\\/:*?\"<>|") != NULL) return FALSE;
            for (index = 0; index < 2; ++index) {
                if (header->hwndFrom == panes[index].list && edit->item.iItem >= 0 &&
                    (size_t)edit->item.iItem < panes[index].listing.count) {
                    return finish_label_edit(window, index, edit);
                }
            }
        }
        if (header->code == LVN_ITEMCHANGED) {
            NMLISTVIEW *change = (NMLISTVIEW *)l_param;
            if ((change->uChanged & LVIF_STATE) != 0) {
                for (index = 0; index < 2; ++index) {
                    if (header->hwndFrom == panes[index].list) {
                        update_info_panel(&panes[index]);
                        update_operation_state();
                        break;
                    }
                }
            }
        }
        if (header->code == NM_CLICK || header->code == NM_DBLCLK) {
            for (index = 0; index < 2; ++index) {
                if (header->hwndFrom == panes[index].list) {
                    int item = ((NMITEMACTIVATE *)l_param)->iItem;
                    if (item >= 0 && (size_t)item < panes[index].listing.count) {
                        BrowserEntryKind kind = panes[index].listing.entries[item].kind;
                        if ((header->code == NM_CLICK && kind == BROWSER_ENTRY_AUDIO_FILE &&
                             (GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
                             (GetKeyState(VK_SHIFT) & 0x8000) == 0) ||
                            (header->code == NM_DBLCLK && kind == BROWSER_ENTRY_DIRECTORY))
                            activate_item(window, &panes[index], item);
                    }
                    return 0;
                }
            }
        }
        break;
    }
    case WM_METADATA_BATCH: {
        MetadataBatch *batch = (MetadataBatch *)l_param;
        apply_metadata_batch(batch);
        metadata_batch_free(batch);
        return 0;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        SetTextColor((HDC)w_param, app_colors.text);
        SetBkColor((HDC)w_param, app_colors.control);
        return (LRESULT)app_control_brush;
    case WM_CTLCOLORSTATIC:
        for (index = 0; index < 2; ++index) {
            if ((HWND)l_param == panes[index].summary_label &&
                settings_theme_is_external(app_settings.theme)) {
                SetTextColor((HDC)w_param, app_colors.summary_text);
                SetBkColor((HDC)w_param, app_colors.summary);
                return (LRESULT)app_summary_brush;
            }
        }
        SetTextColor((HDC)w_param, app_colors.text);
        SetBkColor((HDC)w_param, app_colors.window);
        return (LRESULT)app_background_brush;
    case WM_DESTROY:
        if (metadata_loader != NULL) {
            MSG pending;
            (void)metadata_loader_shutdown(metadata_loader, 2000);
            metadata_loader = NULL;
            while (PeekMessageW(&pending, window, WM_METADATA_BATCH,
                                WM_METADATA_BATCH, PM_REMOVE))
                metadata_batch_free((MetadataBatch *)pending.lParam);
        }
        KillTimer(window, 1);
        stop_playback();
        save_window_geometry(window);
        persist_last_directories();
        browser_listing_free(&panes[0].listing);
        browser_listing_free(&panes[1].listing);
        free(panes[0].entry_index_by_id);
        free(panes[1].entry_index_by_id);
        if (app_font != NULL) DeleteObject(app_font);
        if (app_bold_font != NULL) DeleteObject(app_bold_font);
        if (app_background_brush != NULL) DeleteObject(app_background_brush);
        if (app_control_brush != NULL) DeleteObject(app_control_brush);
        if (app_summary_brush != NULL) DeleteObject(app_summary_brush);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE previous,
                    _In_ PWSTR command_line, _In_ int show_command)
{
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_BAR_CLASSES};
    WNDCLASSEXW window_class = {0};
    WNDCLASSEXW settings_class = {0};
    WNDCLASSEXW help_class = {0};
    HWND window;
    HACCEL accelerator_table;
    HANDLE single_instance;
    wchar_t settings_path[MAX_PATH];
    RECT startup_rect;
    MSG message;
    ACCEL accelerators[] = {
        {FVIRTKEY, VK_F1, ID_HELP},
        {FVIRTKEY, VK_F2, ID_ACCEL_COPY},
        {FVIRTKEY, VK_F3, ID_ACCEL_MOVE},
        {FVIRTKEY, VK_F4, ID_ACCEL_DELETE},
        {FVIRTKEY, VK_F5, ID_REFRESH_ACTIVE}
    };
    (void)previous;
    (void)command_line;
    InitCommonControlsEx(&controls);
    settings_load(&app_settings);
    single_instance = CreateMutexW(NULL, FALSE, L"Local\\AudioCommander.SingleInstance");
    if (single_instance == NULL) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, TR(UI_ALREADY_RUNNING), TR(UI_APP_TITLE),
                    MB_OK | MB_ICONINFORMATION);
        CloseHandle(single_instance);
        return 0;
    }
    if (!settings_ini_path(settings_path, ARRAYSIZE(settings_path))) {
        MessageBoxW(NULL, TR(UI_ACCEPTANCE_SAVE_FAILED), TR(UI_ERROR_TITLE),
                    MB_OK | MB_ICONERROR);
        CloseHandle(single_instance);
        return 1;
    }
    if (!settings_first_run_accepted(settings_path)) {
        if (MessageBoxW(NULL, TR(UI_FIRST_RUN_NOTICE), TR(UI_FIRST_RUN_TITLE),
                        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
            CloseHandle(single_instance);
            return 0;
        }
        if (!settings_record_first_run_acceptance(settings_path)) {
            MessageBoxW(NULL, TR(UI_ACCEPTANCE_SAVE_FAILED), TR(UI_ERROR_TITLE),
                        MB_OK | MB_ICONERROR);
            CloseHandle(single_instance);
            return 1;
        }
    }
    settings_save(&app_settings);
    startup_rect = initial_window_rect();
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    window_class.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(1));
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszClassName = APP_CLASS;
    if (!RegisterClassExW(&window_class)) {
        CloseHandle(single_instance);
        return 1;
    }
    settings_class.cbSize = sizeof(settings_class);
    settings_class.lpfnWndProc = settings_proc;
    settings_class.hInstance = instance;
    settings_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    settings_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    settings_class.lpszClassName = SETTINGS_CLASS;
    if (!RegisterClassExW(&settings_class)) {
        CloseHandle(single_instance);
        return 1;
    }
    help_class.cbSize = sizeof(help_class);
    help_class.lpfnWndProc = help_proc;
    help_class.hInstance = instance;
    help_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    help_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    help_class.lpszClassName = HELP_CLASS;
    if (!RegisterClassExW(&help_class)) {
        CloseHandle(single_instance);
        return 1;
    }
    window = CreateWindowExW(0, APP_CLASS, TR(UI_APP_TITLE),
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             startup_rect.left, startup_rect.top,
                             startup_rect.right - startup_rect.left,
                             startup_rect.bottom - startup_rect.top,
                             NULL, NULL, instance, NULL);
    if (window == NULL) {
        CloseHandle(single_instance);
        return 1;
    }
    accelerator_table = CreateAcceleratorTableW(accelerators, ARRAYSIZE(accelerators));
    ShowWindow(window, show_command);
    UpdateWindow(window);
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        if (accelerator_table == NULL || !TranslateAcceleratorW(window, accelerator_table, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (accelerator_table != NULL) DestroyAcceleratorTable(accelerator_table);
    CloseHandle(single_instance);
    return (int)message.wParam;
}
