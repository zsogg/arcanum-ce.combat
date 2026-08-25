#include "tig/find_file.h"

#include <stdio.h>
#include <string.h>

#include "tig/compat.h"
#include "tig/memory.h"

typedef struct TigFindFileDataPriv {
    char dir[TIG_MAX_PATH];
    char** entries;
    int count;
    int index;
} TigFindFileDataPriv;

bool tig_find_first_file(const char* pattern, TigFindFileData* find_file_data)
{
    char native_pattern[TIG_MAX_PATH];
    char dir[TIG_MAX_PATH];
    char fname[TIG_MAX_PATH];
    char* pch;
    int count;
    char** entries;
    TigFindFileDataPriv* priv;

    find_file_data->name[0] = '\0';
    find_file_data->handle = 0;

    strcpy(native_pattern, pattern);
    compat_windows_path_to_native(native_pattern);

    pch = strrchr(native_pattern, PATH_SEPARATOR);
    if (pch != NULL) {
        strncpy(dir, native_pattern, pch - native_pattern);
        dir[pch - native_pattern] = '\0';

        compat_windows_path_to_native(dir);
        compat_resolve_path(dir);

        strcpy(fname, pch + 1);
    } else {
        dir[0] = '.';
        dir[1] = '\0';

        strcpy(fname, native_pattern);
    }

    entries = SDL_GlobDirectory(dir, fname, SDL_GLOB_CASEINSENSITIVE, &count);
    if (count == 0) {
        // FIX: Memory leak. A glob that matches nothing still hands back an
        // allocated array - one slot holding the NULL terminator - and SDL
        // documents the result as the caller's to free. Dropping it leaked 8
        // bytes on every miss, and the callers are `tig_file_exists_native`
        // and `tig_file_list_create_native`, so misses are routine.
        //
        // CE-only, with nothing to be faithful to: the original reaches this
        // through Win32 `FindFirstFile`, which reports failure with
        // INVALID_HANDLE_VALUE and allocates nothing. Found 2026-08-25 by
        // running the new archive suite under the ASan preset (roadmap P5.4).
        SDL_free(entries);
        return false;
    }

    priv = (TigFindFileDataPriv*)MALLOC(sizeof(*priv));
    strcpy(priv->dir, dir);
    priv->entries = entries;
    priv->count = count;
    priv->index = 0;
    find_file_data->handle = (intptr_t)priv;

    if (!tig_find_next_file(find_file_data)) {
        SDL_free(entries);
        FREE(priv);
        find_file_data->handle = 0;
        return false;
    }

    return true;
}

bool tig_find_next_file(TigFindFileData* find_file_data)
{
    TigFindFileDataPriv* priv;
    char path[TIG_MAX_PATH];

    priv = (TigFindFileDataPriv*)find_file_data->handle;
    if (priv == NULL) {
        return false;
    }

    if (priv->index >= priv->count) {
        return false;
    }

    compat_join_path(path, sizeof(path), priv->dir, priv->entries[priv->index]);
    if (!SDL_GetPathInfo(path, &(find_file_data->path_info))) {
        return false;
    }

    strcpy(find_file_data->name, priv->entries[priv->index]);
    priv->index++;

    return true;
}

bool tig_find_close(TigFindFileData* find_file_data)
{
    TigFindFileDataPriv* priv;

    priv = (TigFindFileDataPriv*)find_file_data->handle;
    if (priv == NULL) {
        return false;
    }

    SDL_free(priv->entries);
    FREE(priv);

    memset(find_file_data, 0, sizeof(*find_file_data));

    return true;
}
