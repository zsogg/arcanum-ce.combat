// Unpacks a .dat archive through CE's own reader.
//
// The point is differential, not utility. `arcanum-work/scripts/dbmaker_diff.py`
// compares the shipped archives against Troika's `dbmaker.exe`, but it reads
// them with `dat_extract.py`, which was transcribed from `database.c` - so on
// its own it can only prove that our Python agrees with our C. Pointing the same
// comparison at this binary replaces the transcription with the shipped reader,
// which is the thing anyone actually cares about being correct.
//
//     arcanum-dat-dump <archive> <outdir>
//
// Writes every non-directory entry to <outdir>/<entry path> and prints one
// summary line. Not registered as a test: it needs the game's archives, which
// are not part of this repository.
//
// Reads through `tig_database_fread` rather than by pulling the payload apart
// itself, so the decompression loop, the plain path and the end-of-entry clamp
// are all on the path being checked.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "tig/compat.h"
#include "tig/database.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#define MKDIR(path) mkdir(path, 0755)
#endif

// Creates every directory along `path` except the last component.
static bool make_parents(char* path)
{
    char* pch;

    for (pch = path; *pch != '\0'; pch++) {
        if (*pch == PATH_SEPARATOR && pch != path) {
            *pch = '\0';
            if (MKDIR(path) != 0 && errno != EEXIST) {
                return false;
            }
            *pch = PATH_SEPARATOR;
        }
    }

    return true;
}

static bool dump_entry(TigDatabase* database, TigDatabaseEntry* entry, const char* outdir)
{
    char path[2048];
    TigDatabaseFileHandle* stream;
    unsigned char* buffer;
    size_t read;
    FILE* dest;

    snprintf(path, sizeof(path), "%s%c%s", outdir, PATH_SEPARATOR, entry->path);
    if (!make_parents(path)) {
        fprintf(stderr, "cannot create directories for %s\n", path);
        return false;
    }

    if ((entry->flags & TIG_DATABASE_ENTRY_DIRECTORY) != 0) {
        if (MKDIR(path) != 0 && errno != EEXIST) {
            fprintf(stderr, "cannot create %s\n", path);
            return false;
        }
        return true;
    }

    stream = tig_database_fopen_entry(database, entry, "rb");
    if (stream == NULL) {
        fprintf(stderr, "cannot open entry %s\n", entry->path);
        return false;
    }

    buffer = (unsigned char*)malloc(entry->size != 0 ? entry->size : 1);
    if (buffer == NULL) {
        tig_database_fclose(stream);
        return false;
    }

    read = entry->size != 0
        ? tig_database_fread(buffer, 1, entry->size, stream)
        : 0;
    if (read != entry->size) {
        fprintf(stderr, "short read on %s: %u of %u\n",
            entry->path, (unsigned int)read, entry->size);
        free(buffer);
        tig_database_fclose(stream);
        return false;
    }

    tig_database_fclose(stream);

    dest = fopen(path, "wb");
    if (dest == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        free(buffer);
        return false;
    }

    if (entry->size != 0) {
        fwrite(buffer, 1, entry->size, dest);
    }

    fclose(dest);
    free(buffer);

    return true;
}

int main(int argc, char** argv)
{
    TigDatabase* database;
    unsigned int index;
    int files = 0;
    int directories = 0;
    int failed = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <archive> <outdir>\n", argv[0]);
        return 2;
    }

    database = tig_database_open(argv[1]);
    if (database == NULL) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }

    if (MKDIR(argv[2]) != 0 && errno != EEXIST) {
        fprintf(stderr, "cannot create %s\n", argv[2]);
        return 1;
    }

    for (index = 0; index < database->entries_count; index++) {
        TigDatabaseEntry* entry = &(database->entries[index]);

        if (!dump_entry(database, entry, argv[2])) {
            failed++;
            continue;
        }

        if ((entry->flags & TIG_DATABASE_ENTRY_DIRECTORY) != 0) {
            directories++;
        } else {
            files++;
        }
    }

    tig_database_close(database);

    printf("%s: %d files, %d directories, %d failed\n",
        argv[1], files, directories, failed);

    return failed != 0 ? 1 : 0;
}
