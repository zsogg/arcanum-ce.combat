// Runs CE's own data-file readers over one file and exits.
//
// A mod produces files the engine has never seen. The readers were written
// against Troika's well-formed data, and one of them has already been caught
// overflowing on input alone: porting bug #11, where `parse_field`'s bound let
// `pos` reach MAX_STRING, and any .mes message of MAX_STRING characters or
// more wrote past the end of a caller's buffer. That was found by reading. It
// is the kind of thing a fuzzer finds by breakfast.
//
// `FIDELITY-CONTRACT.md` judges Free code on *must not crash, hang or
// corrupt*, and a reader that segfaults on a modder's typo fails that bar
// whatever the original does. This is the harness that asks.
//
//     arcanum-fuzz-probe mes <path>
//
// Exit 0 means the reader survived -- **not** that the file was valid.
// Refusing a malformed file is a correct outcome and must not be reported as
// a failure; the only failures here are a crash, a hang, or a sanitizer
// report, which is why this is run under the linux-x64-asan preset.
//
// Deliberately one file per process: a fuzzer wants the crash attributed to
// an input, and a long-lived process would attribute it to whichever input
// happened to be last.
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tig/tig.h>

#include "game/mes.h"

static int probe_mes(const char* path)
{
    mes_file_handle_t handle;
    MesFileEntry entry;
    int count;
    int index;

    if (!mes_load(path, &handle)) {
        // A refusal is a pass. The reader looked at the bytes and declined,
        // which is exactly what it should do with a broken file.
        printf("refused\n");
        return 0;
    }

    // Loading is not the whole reader. The entries have to be walked, because
    // that is where a caller's buffer gets written into -- porting bug #11
    // was in `parse_field`, reached through the walk rather than the open.
    count = mes_entries_count(handle);
    for (index = 0; index < count; index++) {
        if (mes_get_entry(handle, index, &entry)) {
            // Touch the string the way a caller would.
            if (entry.str != NULL) {
                volatile size_t len = strlen(entry.str);
                (void)len;
            }
        }
    }

    if (mes_find_first(handle, &entry)) {
        while (mes_find_next(handle, &entry)) {
            if (entry.str != NULL) {
                volatile size_t len = strlen(entry.str);
                (void)len;
            }
        }
    }

    mes_unload(handle);
    printf("ok %d\n", count);
    return 0;
}

int main(int argc, char** argv)
{
    char* dup;
    char* dir;
    char* base;
    int rc;

    if (argc != 3) {
        fprintf(stderr, "usage: %s mes <path>\n", argv[0]);
        return 2;
    }

    // No `tig_init`: the file layer stands up on its own, which is what
    // `file_probe.c` relies on too. The reader opens by game-relative path,
    // so mount the file's own directory and hand it the basename.
    //
    // NOTE: `tig_file_repository_add` deletes that directory's `TIGCache`
    // (see tests/file_probe.c). Point this at a scratch corpus, never at the
    // install.
    dup = strdup(argv[2]);
    dir = dirname(dup);
    if (!tig_file_repository_add(dir)) {
        fprintf(stderr, "cannot mount %s\n", dir);
        free(dup);
        return 3;
    }

    base = strrchr(argv[2], '/');
    base = (base != NULL) ? base + 1 : argv[2];

    if (strcmp(argv[1], "mes") == 0) {
        rc = probe_mes(base);
    } else {
        fprintf(stderr, "unknown format: %s\n", argv[1]);
        rc = 2;
    }

    free(dup);
    return rc;
}
