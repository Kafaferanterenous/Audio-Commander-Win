#include "playlist.h"

#include <stdlib.h>
#include <string.h>

void playlist_clear(Playlist *playlist)
{
    size_t index;
    if (playlist == NULL) return;
    for (index = 0; index < playlist->count; ++index) free(playlist->paths[index]);
    free(playlist->paths);
    memset(playlist, 0, sizeof(*playlist));
}

bool playlist_build_after(Playlist *playlist, const wchar_t *folder,
                          const BrowserListing *listing, size_t current_index)
{
    size_t index;
    size_t count = 0;
    if (playlist == NULL) return false;
    playlist_clear(playlist);
    if (folder == NULL || folder[0] == L'\0' || listing == NULL ||
        current_index >= listing->count)
        return false;
    for (index = current_index + 1; index < listing->count; ++index)
        if (listing->entries[index].kind == BROWSER_ENTRY_AUDIO_FILE) ++count;
    if (count == 0) return true;
    playlist->paths = (wchar_t **)calloc(count, sizeof(*playlist->paths));
    if (playlist->paths == NULL) return false;
    for (index = current_index + 1; index < listing->count; ++index) {
        const BrowserEntry *entry = &listing->entries[index];
        size_t folder_length;
        size_t name_length;
        size_t path_count;
        wchar_t *path;
        if (entry->kind != BROWSER_ENTRY_AUDIO_FILE) continue;
        folder_length = wcslen(folder);
        name_length = wcslen(entry->name);
        path_count = folder_length + name_length + 2;
        path = (wchar_t *)malloc(path_count * sizeof(*path));
        if (path == NULL || swprintf_s(path, path_count, L"%ls%ls%ls", folder,
            folder[folder_length - 1] == L'\\' ? L"" : L"\x005c", entry->name) < 0) {
            free(path);
            playlist_clear(playlist);
            return false;
        }
        playlist->paths[playlist->count++] = path;
    }
    return true;
}

const wchar_t *playlist_next(Playlist *playlist)
{
    if (playlist == NULL || playlist->next_index >= playlist->count) return NULL;
    return playlist->paths[playlist->next_index++];
}
