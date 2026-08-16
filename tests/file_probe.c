// Resolves game paths through CE's real file layer and reports what it read.
//
// The archive differential (`dbmaker_diff.py`) proves CE reads any one archive
// correctly. It says nothing about *which* archive the game reads a path from,
// which is a separate mechanism: repositories are mounted in a particular order
// and `tig_file_open_internal_native` picks among them. Everything downstream of
// `dat_extract.py --overlay` - the resolved .mes views, `verify_messages.py` -
// depends on a Python model of that mechanism that has never been checked
// against the thing it models.
//
//     arcanum-file-probe <repo-list> <path-list>
//
// `<repo-list>` is one repository per line **in mount order**, lowest priority
// first, exactly as the game adds them. Take it from the game's own log rather
// than from a model of it: a run prints `TIG File: Added path "..."` and
// `TIG File: Added database "..."` in order, and `scripts/overlay_check.py`
// parses those lines.
//
// `<path-list>` is one game-relative path per line. For each, this prints
//
//     <path>\t<size>\t<crc32>
//
// or `<path>\tMISSING`, having read the bytes through `tig_file_fread` - so the
// answer is the payload the game would get, whichever repository supplied it.
// crc32 rather than a stronger digest because both sides need it fast: zlib is
// already linked here, and Python's `zlib.crc32` is the same code.
//
// Run it with the game folder as the working directory; repository paths are
// relative to it, as they are for the game.
//
// Reads with "rb" and never calls `tig_file_extract*`, the API that would write
// a cache copy - but it is not quite side-effect free, and the exception is
// worth knowing before pointing it at a real install. **Mounting a directory
// repository deletes that directory's `TIGCache`**: `tig_file_repository_add`
// (file.c:630) runs `tig_file_rmdir_recursively_native(<repo>/TIGCache)` on
// every directory it adds. That is the game's own startup behaviour, not
// something this adds - launching the game clears `data/TIGCache` the same way -
// so what is lost is a cache the next run would rebuild. Nothing else is
// written.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include "tig/file.h"

#define CHUNK (1 << 16)

static int read_lines(const char* path, char*** out)
{
    FILE* stream;
    char** lines = NULL;
    int count = 0;
    int capacity = 0;
    char buffer[TIG_MAX_PATH];

    stream = fopen(path, "rb");
    if (stream == NULL) {
        fprintf(stderr, "cannot read %s\n", path);
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), stream) != NULL) {
        size_t length = strlen(buffer);

        while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')) {
            buffer[--length] = '\0';
        }

        if (length == 0) {
            continue;
        }

        if (count == capacity) {
            capacity = capacity != 0 ? capacity * 2 : 64;
            lines = (char**)realloc(lines, sizeof(*lines) * capacity);
        }

        lines[count] = (char*)malloc(length + 1);
        memcpy(lines[count], buffer, length + 1);
        count++;
    }

    fclose(stream);
    *out = lines;
    return count;
}

int main(int argc, char** argv)
{
    char** repos;
    char** paths;
    int repos_count;
    int paths_count;
    int index;
    int missing = 0;
    unsigned char* buffer;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <repo-list> <path-list>\n", argv[0]);
        return 2;
    }

    repos_count = read_lines(argv[1], &repos);
    paths_count = read_lines(argv[2], &paths);
    if (repos_count < 0 || paths_count < 0) {
        return 1;
    }

    // In mount order: each `add` prepends, so the last one added is searched
    // first. Feeding the list in the game's own order is the whole point.
    for (index = 0; index < repos_count; index++) {
        if (!tig_file_repository_add(repos[index])) {
            fprintf(stderr, "cannot mount %s\n", repos[index]);
            return 1;
        }
    }

    buffer = (unsigned char*)malloc(CHUNK);

    for (index = 0; index < paths_count; index++) {
        TigFile* stream = tig_file_fopen(paths[index], "rb");
        uLong crc = crc32(0L, Z_NULL, 0);
        long total = 0;

        if (stream == NULL) {
            printf("%s\tMISSING\n", paths[index]);
            missing++;
            continue;
        }

        for (;;) {
            size_t got = tig_file_fread(buffer, 1, CHUNK, stream);
            if (got == 0) {
                break;
            }
            crc = crc32(crc, buffer, (uInt)got);
            total += (long)got;
        }

        tig_file_fclose(stream);
        printf("%s\t%ld\t%08lx\n", paths[index], total, (unsigned long)crc);
    }

    free(buffer);
    fprintf(stderr, "%d paths, %d missing\n", paths_count, missing);

    return 0;
}
