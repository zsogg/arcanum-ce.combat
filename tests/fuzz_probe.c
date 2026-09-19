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
//     arcanum-fuzz-probe {mes|dlg|scr|jmp|mob|pro} <path>
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

#include "game/dialog.h"
#include "game/jumppoint.h"
#include "game/mes.h"
#include "game/obj.h"
#include "game/obj_file.h"
#include "game/script.h"

#include "obj_harness.h"

// **A reader that refuses everything proves nothing**, so each probe below is
// required to reach real parsing on a valid shipped file before its refusals
// count as evidence of safety (card H6, work item 1). `--check` in
// `fuzz_readers.py` is what asserts that, per format, against a seed from the
// shipped corpus.
//
// The initialisation each reader needs is the substance of that item, and it
// differs per format:
//
//   mes  nothing but the file repository.
//   dlg  the same; `dialog_load` reads a text file and asks `timeevent` only
//        for a cache timestamp.
//   jmp  the same; `jumppoint_open` takes explicit paths and does not go
//        through `jumppoint_init`, which wants a whole GameInitInfo.
//   pro  the object system, which `tests/obj_harness.c` already stands up
//        free-standing - `obj_read` is what `proto_init` calls per `.pro`.
//   mob  the object system too; `objf_solitary_read` is the map loader's
//        per-object path.
//   scr  nothing but the file repository. `script_file_load_hdr` and
//        `script_file_load_code` are the two functions that turn the bytes
//        into a `ScriptFile`. Both were already defined without `static`;
//        only their forward declarations made them internal, and card H6
//        moved those into `script.h`.
//        Going through `script_load_hdr` instead would add the script *name*
//        table and reach only the 8-byte header - and the header is not where
//        the interesting lengths are.

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

static int probe_scr(const char* path)
{
    TigFile* stream;
    ScriptHeader hdr;
    ScriptFile file;

    stream = tig_file_fopen(path, "rb");
    if (stream == NULL) {
        printf("refused\n");
        return 0;
    }

    if (!script_file_load_hdr(stream, &hdr)) {
        tig_file_fclose(stream);
        printf("refused\n");
        return 0;
    }

    // Zeroed rather than built by `script_file_create`, which has internal
    // linkage. `script_file_load_code` allocates `entries` itself, so a zeroed
    // struct is the same starting state its real caller hands it.
    memset(&file, 0, sizeof(file));
    if (!script_file_load_code(stream, &file)) {
        tig_file_fclose(stream);
        FREE(file.entries);
        printf("refused\n");
        return 0;
    }
    tig_file_fclose(stream);

    printf("ok %d\n", file.num_entries);
    FREE(file.entries);
    return 0;
}

static int probe_dlg(const char* path)
{
    int dlg = -1;

    if (!dialog_load(path, &dlg)) {
        printf("refused\n");
        return 0;
    }

    // Unlike `mes`, there is no second walk to do: `dialog_load_internal`
    // parses every entry up front, and `dialog_load` returns false when it
    // produced none. So a success here already means the parser built at
    // least one entry out of the bytes - which is the reach this probe has to
    // demonstrate before its refusals mean anything - and everything the
    // engine exports for reading an entry back wants a live NPC and PC
    // object, which is a different harness than this one.
    dialog_unload(dlg);
    printf("ok\n");
    return 0;
}

static int probe_jmp(const char* path)
{
    char* dup;
    char* dir;

    // `jumppoint_open` builds `<dir>\\map.jmp` itself, so it wants the
    // directory rather than the file. It tries the save path first and falls
    // back to the base path; both are the same directory here.
    dup = strdup(path);
    dir = dirname(dup);

    if (!jumppoint_open(dir, dir)) {
        free(dup);
        printf("refused\n");
        return 0;
    }

    jumppoint_map_close();
    free(dup);
    printf("ok\n");
    return 0;
}

static int probe_pro(const char* path)
{
    TigFile* stream;
    int64_t obj = OBJ_HANDLE_NULL;

    if (!obj_harness_init()) {
        fprintf(stderr, "obj system refused to initialise\n");
        return 3;
    }

    stream = tig_file_fopen(path, "rb");
    if (stream == NULL) {
        printf("refused\n");
        return 0;
    }

    if (!obj_read(stream, &obj)) {
        tig_file_fclose(stream);
        printf("refused\n");
        return 0;
    }
    tig_file_fclose(stream);

    // Touch the object the way `proto_init` does: a field read is where a
    // bogus field table or string offset is actually dereferenced, and a
    // `.pro` that parsed is not yet a `.pro` that can be used.
    {
        volatile int type = obj_field_int32_get(obj, OBJ_F_TYPE);
        (void)type;
    }

    printf("ok\n");
    return 0;
}

static int probe_mob(const char* path)
{
    int64_t handle = OBJ_HANDLE_NULL;

    if (!obj_harness_init()) {
        fprintf(stderr, "obj system refused to initialise\n");
        return 3;
    }

    // The map loader's per-object path. It takes the game-relative path of
    // the `.mob` itself, unlike `jumppoint_open`.
    if (!objf_solitary_read(&handle, path)) {
        printf("refused\n");
        return 0;
    }

    {
        volatile int type = obj_field_int32_get(handle, OBJ_F_TYPE);
        (void)type;
    }

    printf("ok\n");
    return 0;
}

int main(int argc, char** argv)
{
    char* dup;
    char* dir;
    char* base;
    int rc;

    if (argc != 3) {
        fprintf(stderr, "usage: %s {mes|dlg|scr|jmp|mob|pro} <path>\n", argv[0]);
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
    } else if (strcmp(argv[1], "dlg") == 0) {
        rc = probe_dlg(base);
    } else if (strcmp(argv[1], "jmp") == 0) {
        rc = probe_jmp(argv[2]);
    } else if (strcmp(argv[1], "mob") == 0) {
        rc = probe_mob(base);
    } else if (strcmp(argv[1], "pro") == 0) {
        rc = probe_pro(base);
    } else if (strcmp(argv[1], "scr") == 0) {
        rc = probe_scr(base);
    } else {
        fprintf(stderr, "unknown format: %s\n", argv[1]);
        rc = 2;
    }

    free(dup);
    return rc;
}
