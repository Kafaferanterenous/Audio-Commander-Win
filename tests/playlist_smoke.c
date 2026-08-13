#include "playlist.h"

#include <stdio.h>
#include <wchar.h>

int wmain(void)
{
    BrowserEntry entries[] = {
        {L"Folder", BROWSER_ENTRY_DIRECTORY, 0, 0},
        {L"first.wav", BROWSER_ENTRY_AUDIO_FILE, 1, 1},
        {L"second.mp3", BROWSER_ENTRY_AUDIO_FILE, 1, 1},
        {L"third.flac", BROWSER_ENTRY_AUDIO_FILE, 1, 1}
    };
    BrowserListing listing = {entries, 4};
    Playlist playlist = {0};
    const wchar_t *path;
    if (!playlist_build_after(&playlist, L"C:\\Music", &listing, 1)) return 1;
    path = playlist_next(&playlist);
    if (path == NULL || wcscmp(path, L"C:\\Music\\second.mp3") != 0) return 2;
    path = playlist_next(&playlist);
    if (path == NULL || wcscmp(path, L"C:\\Music\\third.flac") != 0) return 3;
    if (playlist_next(&playlist) != NULL) return 4;
    playlist_clear(&playlist);
    if (playlist.paths != NULL || playlist.count != 0 || playlist.next_index != 0) return 5;
    if (!playlist_build_after(&playlist, L"C:\\Music\x005c", &listing, 3) ||
        playlist_next(&playlist) != NULL) return 6;
    if (playlist_build_after(&playlist, NULL, &listing, 1) ||
        playlist.paths != NULL || playlist.count != 0) return 7;
    playlist_clear(&playlist);
    wprintf(L"sequential playlist snapshot ordering and cleanup passed\n");
    return 0;
}
