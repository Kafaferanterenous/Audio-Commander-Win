#ifndef AUDIOCOMMANDER_PLAYLIST_H
#define AUDIOCOMMANDER_PLAYLIST_H

#include <stdbool.h>
#include <stddef.h>
#include <wchar.h>

#include "browser.h"

typedef struct Playlist {
    wchar_t **paths;
    size_t count;
    size_t next_index;
} Playlist;

bool playlist_build_after(Playlist *playlist, const wchar_t *folder,
                          const BrowserListing *listing, size_t current_index);
const wchar_t *playlist_next(Playlist *playlist);
void playlist_clear(Playlist *playlist);

#endif
