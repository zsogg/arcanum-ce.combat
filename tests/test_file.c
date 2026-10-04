// Tests for `tig/file.c`'s repository layer - which copy of a path the game
// gets when more than one place has it.
//
// This is the mechanism every mod depends on and the one nothing tested. It is
// also not the rule it looks like: `tig_file_open_internal_native` does not walk
// the list taking the first hit. It finds the first *archive* holding the path,
// then lets any *directory* repository ahead of that archive supply the bytes
// instead, and only falls back to scanning directories when no archive has it
// at all. The two rules happen to agree on every input - which is worth knowing
// rather than assuming, and is why these cases check the answers rather than the
// code path.
//
// The archives are dbmaker's, from `arcanum-work/scripts/make_db_fixtures.py
// --repos`. `base.dat` and `mod.dat` both hold `rules/hello.txt` with different
// text and one unique file each, so the bytes that come back say which
// repository won.
//
// The wide version of this check is `arcanum-work/scripts/overlay_check.py`,
// which resolves all 101,643 paths of a real install through the same code and
// compares against `dat_extract.py --overlay`. This suite is the part that runs
// with no assets, and it covers the cases the install does not contain - an
// `.ignore` sidecar, and a directory repository shadowing an archive.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Card R2's two F2 reproductions drive a write to failure with
// RLIMIT_FSIZE, in a child so neither the limit nor the SIGXFSZ
// disposition escapes into the rest of the suite.
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include "tig/file.h"

#include "test_harness.h"

#define BASE_PATH "./arcanum_test_repo_base.dat"
#define BASE_IGNORE_PATH "./arcanum_test_repo_base.dat.ignore"
#define MOD_PATH "./arcanum_test_repo_mod.dat"
#define MOD_IGNORE_PATH "./arcanum_test_repo_mod.dat.ignore"
#define DIR_PATH "./arcanum_test_repo_dir"
#define DIR_RULES DIR_PATH "/rules"
#define DIR_HELLO DIR_RULES "/hello.txt"

// Everything any test here creates, files before the directories holding them.
// Explicit rather than a recursive delete through `tig_file_empty_directory`,
// which is built on the function half this suite is testing - cleanup that
// depends on the code under test fails quietly and takes the next test with it.
static const char* const FIXTURE_PATHS[] = {
    BASE_PATH,
    BASE_IGNORE_PATH,
    MOD_PATH,
    MOD_IGNORE_PATH,
    DIR_RULES "/hello.txt",
    DIR_RULES "/dir-only.txt",
    DIR_RULES "/apple.txt",
    DIR_RULES "/Mango.txt",
    DIR_RULES "/Zebra.txt",
    DIR_PATH "/arcanum1.dat",
    DIR_PATH "/Arcanum4.dat",
    DIR_PATH "/ArcanumXbar.dat",
    DIR_PATH "/ArcanumZfoo.dat",
    DIR_RULES,
    DIR_PATH,
};

#include "test_file_fixtures.inc"

static bool write_bytes(const char* path, const unsigned char* data, size_t size)
{
    FILE* stream = fopen(path, "wb");

    if (stream == NULL) {
        return false;
    }

    fwrite(data, 1, size, stream);
    fclose(stream);

    return true;
}

static bool write_text(const char* path, const char* text)
{
    return write_bytes(path, (const unsigned char*)text, strlen(text));
}

// Reads a path through the repository layer. Returns the bytes as a string, or
// NULL when nothing in the mounted repositories has it.
static char* read_path(const char* path, char* buffer, size_t size)
{
    TigFile* stream = tig_file_fopen(path, "rb");
    size_t got;

    if (stream == NULL) {
        return NULL;
    }

    got = tig_file_fread(buffer, 1, size - 1, stream);
    buffer[got] = '\0';
    tig_file_fclose(stream);

    return buffer;
}

static void write_fixtures(void)
{
    write_bytes(BASE_PATH, FIXTURE_BASE, sizeof(FIXTURE_BASE));
    write_bytes(MOD_PATH, FIXTURE_MOD, sizeof(FIXTURE_MOD));
}

static void remove_fixtures(void)
{
    // Named rather than `tig_file_repository_remove_all`, which would also
    // unmount whatever another suite had mounted - the repository list is
    // process-wide state.
    size_t index;

    tig_file_repository_remove(DIR_PATH);
    tig_file_repository_remove(MOD_PATH);
    tig_file_repository_remove(BASE_PATH);

    for (index = 0; index < sizeof(FIXTURE_PATHS) / sizeof(FIXTURE_PATHS[0]); index++) {
        remove(FIXTURE_PATHS[index]);
    }
}

// The listing's names, in order, comma-separated - so an assertion about
// ordering reads as one string and a failure prints the whole list rather than
// the first index that differs.
static char* list_names(const TigFileList* list, char* buffer, size_t size)
{
    unsigned int index;

    buffer[0] = '\0';
    for (index = 0; index < list->count; index++) {
        if (index != 0) {
            strncat(buffer, ",", size - strlen(buffer) - 1);
        }
        strncat(buffer, list->entries[index].path, size - strlen(buffer) - 1);
    }

    return buffer;
}

static const TigFileInfo* list_find(const TigFileList* list, const char* name)
{
    unsigned int index;

    for (index = 0; index < list->count; index++) {
        if (strcmp(list->entries[index].path, name) == 0) {
            return &(list->entries[index]);
        }
    }

    return NULL;
}

static void make_dir_repo(void)
{
    tig_file_mkdir(DIR_PATH);
    tig_file_mkdir(DIR_RULES);
}

// Mounting prepends, so the archive added last is searched first. This is the
// whole basis of the load order: `arcanum1.dat` through `ArcanumZ*.dat` are
// mounted in sorted order precisely so that the name decides who wins.
TEST(the_archive_mounted_last_wins)
{
    char buffer[64];

    write_fixtures();
    CHECK(tig_file_repository_add(BASE_PATH));
    CHECK(tig_file_repository_add(MOD_PATH));

    CHECK_STR(read_path("rules\\hello.txt", buffer, sizeof(buffer)),
        "from the mod archive");

    // Neither archive hides what only the other has.
    CHECK_STR(read_path("rules\\base-only.txt", buffer, sizeof(buffer)),
        "only in base");
    CHECK_STR(read_path("rules\\mod-only.txt", buffer, sizeof(buffer)),
        "only in mod");

    remove_fixtures();
}

// The same two archives the other way round, because a test that only ever
// mounts them in one order cannot tell "the mod won" from "the second file in
// the list won for some other reason".
TEST(reversing_the_mount_order_reverses_the_answer)
{
    char buffer[64];

    write_fixtures();
    CHECK(tig_file_repository_add(MOD_PATH));
    CHECK(tig_file_repository_add(BASE_PATH));

    CHECK_STR(read_path("rules\\hello.txt", buffer, sizeof(buffer)),
        "from the base archive");

    remove_fixtures();
}

// A directory mounted after an archive beats it - this is what `data/` is, and
// what a module's own folder is. It is also the branch of
// `tig_file_open_internal_native` that looks *backwards* from the archive it
// found, rather than simply taking the first hit.
TEST(a_directory_mounted_after_an_archive_beats_it)
{
    char buffer[64];

    write_fixtures();
    tig_file_mkdir(DIR_PATH);
    tig_file_mkdir(DIR_RULES);
    CHECK(write_text(DIR_HELLO, "from the directory"));

    CHECK(tig_file_repository_add(BASE_PATH));
    CHECK(tig_file_repository_add(MOD_PATH));
    CHECK(tig_file_repository_add(DIR_PATH));

    CHECK_STR(read_path("rules\\hello.txt", buffer, sizeof(buffer)),
        "from the directory");

    // ... and it does not hide the archives' other files.
    CHECK_STR(read_path("rules\\mod-only.txt", buffer, sizeof(buffer)),
        "only in mod");

    remove_fixtures();
}

// Porting bug #51: naming a directory opens nothing, as `fopen` refused one
// for the original on Windows. Three sound ids resolve to the bare `sound\`
// directory; on Linux it opened, its length read -1 and the file cache
// corrupted the heap.
TEST(a_directory_is_not_a_file)
{
    TigFile* stream;
    char buffer[64];

    write_fixtures();
    tig_file_mkdir(DIR_PATH);
    tig_file_mkdir(DIR_RULES);
    CHECK(write_text(DIR_HELLO, "from the directory"));
    CHECK(tig_file_repository_add(DIR_PATH));

    CHECK(tig_file_fopen("rules\\", "rb") == NULL);
    CHECK(tig_file_fopen("rules", "rb") == NULL);
    stream = tig_file_fopen(DIR_RULES, "rb");
    CHECK(stream == NULL);
    if (stream != NULL) {
        tig_file_fclose(stream);
    }
    // The file inside it still opens.
    CHECK_STR(read_path("rules\\hello.txt", buffer, sizeof(buffer)), "from the directory");

    remove_fixtures();
}

// The other way round: a directory mounted *before* an archive loses to it,
// which is why a loose file in the game root does not override an archive but
// the same file under `data/` does.
TEST(a_directory_mounted_before_an_archive_loses_to_it)
{
    char buffer[64];

    write_fixtures();
    tig_file_mkdir(DIR_PATH);
    tig_file_mkdir(DIR_RULES);
    CHECK(write_text(DIR_HELLO, "from the directory"));

    CHECK(tig_file_repository_add(DIR_PATH));
    CHECK(tig_file_repository_add(MOD_PATH));

    CHECK_STR(read_path("rules\\hello.txt", buffer, sizeof(buffer)),
        "from the mod archive");

    remove_fixtures();
}

// An `<archive>.ignore` sidecar hides entries inside that archive, so the
// lookup carries on to the next repository. This is how a mod removes a
// base-game file without rebuilding the archive it lives in - and with the mod
// on top, hiding its own entry is what exposes the base copy again.
TEST(an_ignore_sidecar_falls_through_to_the_next_repository)
{
    char buffer[64];

    write_fixtures();
    CHECK(write_text(MOD_IGNORE_PATH, "rules\\hello.txt\n"));

    CHECK(tig_file_repository_add(BASE_PATH));
    CHECK(tig_file_repository_add(MOD_PATH));

    CHECK_STR(read_path("rules\\hello.txt", buffer, sizeof(buffer)),
        "from the base archive");

    // Only the named entry is hidden.
    CHECK_STR(read_path("rules\\mod-only.txt", buffer, sizeof(buffer)),
        "only in mod");

    remove_fixtures();
}

// The one case where they really do disagree, and it is deliberate: an entry
// read out of an archive **stops looking for an override**.
//
// `tig_database_open` marks every entry 0x100 ("not yet resolved"). Opening one
// makes `tig_file_open_internal_native` scan the directory repositories ahead of
// its archive; if none has the file it clears both 0x100 and 0x200, and the test
// at the top of that scan - `if ((flags & (0x100 | 0x200)) != 0 ...)` - then
// skips it for good. So a directory copy that appears *after* the first read is
// never picked up, while `tig_file_exists`, which walks the list every time,
// reports it immediately.
//
// Faithful: the original tests the same two bits, `test ah, 0x3` at 0x530D0B.
//
// It matters for the game because `data/` is mounted at startup, before
// anything is read - but it means a file dropped in mid-session does not take
// effect, and that the memo lives in the loaded index rather than on disk:
// remounting the archive clears it.
TEST(an_entry_already_read_from_an_archive_ignores_a_later_override)
{
    TigFileInfo info;
    char buffer[64];

    write_fixtures();
    CHECK(tig_file_repository_add(BASE_PATH));
    CHECK_STR(read_path("rules\\hello.txt", buffer, sizeof(buffer)),
        "from the base archive");

    tig_file_mkdir(DIR_PATH);
    tig_file_mkdir(DIR_RULES);
    CHECK(write_text(DIR_HELLO, "from the directory"));
    CHECK(tig_file_repository_add(DIR_PATH));

    // The list says the directory has it...
    CHECK(tig_file_exists("rules\\hello.txt", &info));
    CHECK_INT(info.size, strlen("from the directory"));

    // ... and the read does not agree, because this entry is already resolved.
    CHECK_STR(read_path("rules\\hello.txt", buffer, sizeof(buffer)),
        "from the base archive");

    // Remounting rebuilds the index, and with it the flags. Both repositories
    // have to be re-added, in that order: `add` prepends, so re-adding the
    // archive by itself would put it ahead of the directory, and then the
    // directory would lose on order rather than on the memo - which is what
    // this test would otherwise be quietly measuring.
    tig_file_repository_remove(BASE_PATH);
    tig_file_repository_remove(DIR_PATH);
    CHECK(tig_file_repository_add(BASE_PATH));
    CHECK(tig_file_repository_add(DIR_PATH));
    CHECK_STR(read_path("rules\\hello.txt", buffer, sizeof(buffer)),
        "from the directory");

    remove_fixtures();
}

// `tig_file_exists` walks the list itself rather than sharing a code path with
// `fopen`, so the two can disagree in principle. Pinned because a file the game
// checks for and then cannot open, or the reverse, is a nasty class of bug.
TEST(exists_agrees_with_fopen)
{
    TigFileInfo info;
    char buffer[64];

    write_fixtures();
    CHECK(tig_file_repository_add(BASE_PATH));
    CHECK(tig_file_repository_add(MOD_PATH));

    CHECK(tig_file_exists("rules\\hello.txt", &info));
    CHECK_INT(info.size, strlen("from the mod archive"));
    CHECK(read_path("rules\\hello.txt", buffer, sizeof(buffer)) != NULL);

    CHECK(tig_file_exists("rules\\base-only.txt", &info));
    CHECK(read_path("rules\\base-only.txt", buffer, sizeof(buffer)) != NULL);

    CHECK(!tig_file_exists("rules\\nothing.txt", &info));
    CHECK(read_path("rules\\nothing.txt", buffer, sizeof(buffer)) == NULL);

    remove_fixtures();
}

// A directory listing is the union of every repository's, not the winner's
// alone: a mod that adds one map does not hide the base game's. Names are
// basenames on both sides - `tig_file_list_create_native` splits an archive
// entry's path down to fname+ext, and the directory side returns what the glob
// found - which is what makes the two mergeable at all.
TEST(a_listing_merges_every_repository)
{
    TigFileList list;
    char buffer[256];

    write_fixtures();
    make_dir_repo();
    CHECK(write_text(DIR_RULES "/dir-only.txt", "only in the directory"));

    CHECK(tig_file_repository_add(BASE_PATH));
    CHECK(tig_file_repository_add(MOD_PATH));
    CHECK(tig_file_repository_add(DIR_PATH));

    tig_file_list_create(&list, "rules\\*.*");
    CHECK_INT(list.count, 4);
    CHECK_STR(list_names(&list, buffer, sizeof(buffer)),
        "base-only.txt,dir-only.txt,hello.txt,mod-only.txt");
    tig_file_list_destroy(&list);

    remove_fixtures();
}

// A name held by two repositories appears once, and the entry kept is the
// winner's - `tig_file_list_add` returns on an exact match, and the walk runs
// highest priority first, so the first one added is the one a read would get.
// The two archives' `hello.txt` differ in length, which is what makes this
// observable.
TEST(a_shared_name_appears_once_and_describes_the_copy_that_wins)
{
    TigFileList list;
    const TigFileInfo* info;

    write_fixtures();
    CHECK(tig_file_repository_add(BASE_PATH));
    CHECK(tig_file_repository_add(MOD_PATH));

    tig_file_list_create(&list, "rules\\*.*");
    CHECK_INT(list.count, 3);
    info = list_find(&list, "hello.txt");
    CHECK(info != NULL);
    if (info != NULL) {
        CHECK_INT(info->size, strlen("from the mod archive"));
    }
    tig_file_list_destroy(&list);

    tig_file_repository_remove(MOD_PATH);
    tig_file_repository_remove(BASE_PATH);
    CHECK(tig_file_repository_add(MOD_PATH));
    CHECK(tig_file_repository_add(BASE_PATH));

    tig_file_list_create(&list, "rules\\*.*");
    CHECK_INT(list.count, 3);
    info = list_find(&list, "hello.txt");
    CHECK(info != NULL);
    if (info != NULL) {
        CHECK_INT(info->size, strlen("from the base archive"));
    }
    tig_file_list_destroy(&list);

    remove_fixtures();
}

// Sorted case-insensitively, which is not the same as sorted: 'Z' (0x5A) sorts
// before 'a' (0x61) by byte and after it here.
//
// Faithful, and worth attesting because so much rests on it: the original's
// `tig_file_list_add` at 0x530FD0 calls 0x58ADB0 for the comparison, and that
// routine folds case inline (`sub al, 0x41 / cmp al, 0x1a / sbb cl, cl /
// and cl, 0x20`), so it is `stricmp` and not `strcmp`. A case-*sensitive* sort
// would put every `Arcanum*.dat` ahead of `arcanum1.dat`, which would invert
// the load order and make the base archives outrank the community add-ons.
TEST(a_listing_is_sorted_case_insensitively)
{
    TigFileList list;
    char buffer[256];

    make_dir_repo();
    CHECK(write_text(DIR_RULES "/Zebra.txt", "z"));
    CHECK(write_text(DIR_RULES "/apple.txt", "a"));
    CHECK(write_text(DIR_RULES "/Mango.txt", "m"));

    CHECK(tig_file_repository_add(DIR_PATH));

    tig_file_list_create(&list, "rules\\*.*");
    CHECK_STR(list_names(&list, buffer, sizeof(buffer)),
        "apple.txt,Mango.txt,Zebra.txt");
    tig_file_list_destroy(&list);

    remove_fixtures();
}

// Why that ordering matters, and the reason this function is worth testing at
// all: `gamelib_load_data` mounts `arcanum*.dat` in exactly this order, and
// mounting prepends, so **the last name in the listing ends up highest
// priority**. That is the whole mechanism behind naming community add-ons
// `ArcanumX…` and `ArcanumZ…` - the sort here is what makes the name the
// priority.
TEST(the_glob_that_decides_the_mount_order)
{
    TigFileList list;
    char buffer[256];

    make_dir_repo();
    CHECK(write_text(DIR_PATH "/ArcanumZfoo.dat", ""));
    CHECK(write_text(DIR_PATH "/arcanum1.dat", ""));
    CHECK(write_text(DIR_PATH "/ArcanumXbar.dat", ""));
    CHECK(write_text(DIR_PATH "/Arcanum4.dat", ""));

    CHECK(tig_file_repository_add(DIR_PATH));

    tig_file_list_create(&list, "arcanum*.dat");
    CHECK_STR(list_names(&list, buffer, sizeof(buffer)),
        "arcanum1.dat,Arcanum4.dat,ArcanumXbar.dat,ArcanumZfoo.dat");
    tig_file_list_destroy(&list);

    remove_fixtures();
}

// One level, and directories come back flagged. On the archive side that is
// `tig_database_find_next_entry` requiring the entry's path to have as many
// separators as the pattern, so `*.*` at the top yields the `rules` directory
// entry and none of what is inside it.
//
// A pattern ending in `*.*` is rewritten to `*` first (0x52F760 compares the
// last three bytes against the literal at 0x5C22A0 and stores a terminator at
// len-2, which CE matches). Without that, a directory entry - which has no
// extension - would not match the pattern that is supposed to list it.
TEST(a_listing_covers_one_level_and_marks_directories)
{
    TigFileList list;
    const TigFileInfo* info;

    write_fixtures();
    CHECK(tig_file_repository_add(BASE_PATH));

    tig_file_list_create(&list, "*.*");
    CHECK_INT(list.count, 1);
    info = list_find(&list, "rules");
    CHECK(info != NULL);
    if (info != NULL) {
        CHECK_INT(info->attributes & TIG_FILE_ATTRIBUTE_SUBDIR, TIG_FILE_ATTRIBUTE_SUBDIR);
    }
    CHECK(list_find(&list, "hello.txt") == NULL);
    tig_file_list_destroy(&list);

    remove_fixtures();
}

// An entry hidden by an `.ignore` sidecar is left out of the listing too, not
// just out of `fopen` - otherwise a mod could hide a file from being read and
// still have the game enumerate it.
TEST(an_ignored_entry_is_left_out_of_the_listing)
{
    TigFileList list;
    char buffer[256];

    write_fixtures();
    CHECK(write_text(BASE_IGNORE_PATH, "rules\\hello.txt\n"));
    CHECK(write_text(MOD_IGNORE_PATH, "rules\\hello.txt\n"));

    CHECK(tig_file_repository_add(BASE_PATH));
    CHECK(tig_file_repository_add(MOD_PATH));

    tig_file_list_create(&list, "rules\\*.*");
    CHECK_STR(list_names(&list, buffer, sizeof(buffer)),
        "base-only.txt,mod-only.txt");
    tig_file_list_destroy(&list);

    remove_fixtures();
}

// ---------------------------------------------------------------------------
// The archive container - `tig_file_archive` / `tig_file_unarchive`.
//
// Everything above is the repository layer: which copy of a path wins. This is
// the other thing `file.c` does, and it is the outermost layer of the save
// format - `gamelib.c` archives `Save\Current` into a `.tfai`/`.tfaf` pair on
// save and unarchives it back on load. Nothing had ever executed it.
//
// The format cases parse the index with plain stdio rather than calling back
// into tig, so what checks the writer is an independent reader of the bytes and
// not the writer agreeing with itself. The round-trip case is the weaker of the
// two on its own: a writer and reader that share a misunderstanding round-trip
// perfectly, which is why the record stream is asserted directly as well.

#define ARC_SRC "./arcanum_test_archive_src"
#define ARC_SUB ARC_SRC "/sub"
#define ARC_DEEP ARC_SUB "/deeper"
#define ARC_DST "./arcanum_test_archive_dst"
#define ARC_OUT "./arcanum_test_archive_out"
#define ARC_INDEX ARC_OUT ".tfai"
#define ARC_DATA ARC_OUT ".tfaf"
#define ARC_EMPTY "./arcanum_test_archive_empty"
// A mounted plain repository, so a *relative* destination can be archived
// the way `gamelib_save` spells one.
#define ARC_REPO "./arcanum_test_archive_repo"
#define ARC_REPO_INDEX ARC_REPO "/out.tfai"
#define ARC_REPO_DATA ARC_REPO "/out.tfaf"
#define ARC_REPO_TMP_INDEX ARC_REPO "/out.tfai.tmp"
#define ARC_REPO_TMP_DATA ARC_REPO "/out.tfaf.tmp"
#define ARC_REPO_SUB ARC_REPO "/sub"
#define ARC_REPO_SUB_INDEX ARC_REPO_SUB "/out.tfai"
#define ARC_REPO_SUB_TMP ARC_REPO_SUB "/out.tfai.tmp"
// Created as `Case`, asked for as `case`.
#define ARC_REPO_CASE ARC_REPO "/Case"
#define ARC_REPO_CASE_INDEX ARC_REPO_CASE "/out.tfai"
#define ARC_REPO_CASE_TMP ARC_REPO_CASE "/out.tfai.tmp"

// `inner.bin` carries an embedded NUL and a 0xFF so a copy that goes through a
// string function, or a stream opened in text mode, cannot pass.
static const unsigned char ARC_INNER[] = { 0x00, 0x01, 0xFF, 0x7F, 0x00, 0x41 };

// Files before the directories holding them, both trees, deepest first.
static const char* const ARCHIVE_PATHS[] = {
    ARC_INDEX,
    ARC_DATA,
    ARC_DEEP "/leaf.txt",
    ARC_SUB "/inner.bin",
    ARC_SRC "/top.txt",
    ARC_DEEP,
    ARC_SUB,
    ARC_SRC,
    ARC_DST "/sub/deeper/leaf.txt",
    ARC_DST "/sub/inner.bin",
    ARC_DST "/top.txt",
    ARC_DST "/sub/deeper",
    ARC_DST "/sub",
    ARC_DST,
    ARC_EMPTY,
};

static void archive_fixture_remove(void)
{
    size_t index;

    for (index = 0; index < sizeof(ARCHIVE_PATHS) / sizeof(ARCHIVE_PATHS[0]); index++) {
        remove(ARCHIVE_PATHS[index]);
    }
}

// `tig_file_mkdir` is setup, not the code under test - the cases here are about
// `tig_file_archive`/`_unarchive`, and the existing repository cases already
// build their fixtures the same way.
static void archive_fixture_write(void)
{
    archive_fixture_remove();

    tig_file_mkdir(ARC_SRC);
    tig_file_mkdir(ARC_SUB);
    tig_file_mkdir(ARC_DEEP);

    write_text(ARC_SRC "/top.txt", "top level");
    write_bytes(ARC_SUB "/inner.bin", ARC_INNER, sizeof(ARC_INNER));
    write_text(ARC_DEEP "/leaf.txt", "leaf");
}

static int archive_entry_compare(const void* lhs, const void* rhs)
{
    return strcmp(*(const char* const*)lhs, *(const char* const*)rhs);
}

// Walk the pair as the format defines it and rebuild "path=contents" for every
// file record, sorted so the answer does not depend on readdir order. Returns
// the number of file records, or -1 if the stream is malformed or did not end
// with the tag-3 record.
static int archive_read_back(char* out, size_t size)
{
    FILE* index_stream;
    FILE* data_stream;
    char dir[512];
    char name[512];
    char entries[8][1024];
    char* sorted[8];
    unsigned char payload[512];
    int tag;
    int length;
    int count = 0;
    int ended = 0;
    int result;
    int i;

    out[0] = '\0';
    dir[0] = '\0';

    index_stream = fopen(ARC_INDEX, "rb");
    if (index_stream == NULL) {
        return -1;
    }

    data_stream = fopen(ARC_DATA, "rb");
    if (data_stream == NULL) {
        fclose(index_stream);
        return -1;
    }

    while (fread(&tag, sizeof(tag), 1, index_stream) == 1) {
        if (tag == 3) {
            ended = 1;
            break;
        }

        if (tag == 2) {
            char* pch = strrchr(dir, '/');
            if (pch == NULL) {
                dir[0] = '\0';
            } else {
                *pch = '\0';
            }
            continue;
        }

        if (tag != 0 && tag != 1) {
            count = -1;
            break;
        }

        if (fread(&length, sizeof(length), 1, index_stream) != 1
            || length < 0
            || (size_t)length >= sizeof(name)
            || fread(name, (size_t)length, 1, index_stream) != 1) {
            count = -1;
            break;
        }
        name[length] = '\0';

        if (tag == 1) {
            if (dir[0] != '\0') {
                strncat(dir, "/", sizeof(dir) - strlen(dir) - 1);
            }
            strncat(dir, name, sizeof(dir) - strlen(dir) - 1);
            continue;
        }

        if (fread(&length, sizeof(length), 1, index_stream) != 1
            || length < 0
            || (size_t)length > sizeof(payload)
            || count >= (int)(sizeof(entries) / sizeof(entries[0]))) {
            count = -1;
            break;
        }

        if (length != 0 && fread(payload, (size_t)length, 1, data_stream) != 1) {
            count = -1;
            break;
        }

        snprintf(entries[count], sizeof(entries[count]), "%s%s%s=",
            dir, dir[0] != '\0' ? "/" : "", name);
        for (i = 0; i < length; i++) {
            char byte[8];
            snprintf(byte, sizeof(byte), "%02X", payload[i]);
            strncat(entries[count], byte,
                sizeof(entries[count]) - strlen(entries[count]) - 1);
        }
        count++;
    }

    fclose(data_stream);
    fclose(index_stream);

    if (count < 0 || !ended) {
        return -1;
    }

    for (i = 0; i < count; i++) {
        sorted[i] = entries[i];
    }
    qsort(sorted, (size_t)count, sizeof(sorted[0]), archive_entry_compare);

    for (i = 0; i < count; i++) {
        if (i != 0) {
            strncat(out, ",", size - strlen(out) - 1);
        }
        strncat(out, sorted[i], size - strlen(out) - 1);
    }

    result = count;
    return result;
}

static long archive_file_size(const char* path)
{
    FILE* stream = fopen(path, "rb");
    long size;

    if (stream == NULL) {
        return -1;
    }

    fseek(stream, 0, SEEK_END);
    size = ftell(stream);
    fclose(stream);

    return size;
}

// The property the save path actually depends on: what went in comes back out,
// nested directories and all, byte for byte.
TEST(an_archived_tree_comes_back_byte_for_byte)
{
    char buffer[64];
    unsigned char raw[16];
    FILE* stream;

    archive_fixture_write();

    CHECK(tig_file_archive(ARC_OUT, ARC_SRC));
    CHECK(tig_file_unarchive(ARC_OUT, ARC_DST));

    CHECK_STR(read_path(ARC_DST "/top.txt", buffer, sizeof(buffer)), "top level");
    CHECK_STR(read_path(ARC_DST "/sub/deeper/leaf.txt", buffer, sizeof(buffer)), "leaf");

    // The binary one by hand - `read_path` stops at the embedded NUL.
    stream = fopen(ARC_DST "/sub/inner.bin", "rb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        size_t got = fread(raw, 1, sizeof(raw), stream);
        fclose(stream);
        CHECK_INT(got, sizeof(ARC_INNER));
        CHECK_INT(memcmp(raw, ARC_INNER, sizeof(ARC_INNER)), 0);
    }

    archive_fixture_remove();
}

// The index is a record stream of int32 tags: 0 file, 1 push dir, 2 pop dir,
// 3 end. Names are stored with a length and no terminator; payloads are
// concatenated into `.tfaf` in index order and carry no lengths of their own.
TEST(the_index_is_the_record_stream_the_format_says)
{
    char names[1024];
    int count;

    archive_fixture_write();
    CHECK(tig_file_archive(ARC_OUT, ARC_SRC));

    count = archive_read_back(names, sizeof(names));
    CHECK_INT(count, 3);
    CHECK_STR(names,
        "sub/deeper/leaf.txt=6C656166,"
        "sub/inner.bin=0001FF7F0041,"
        "top.txt=746F70206C6576656C");

    // `.tfaf` is exactly the payloads and nothing else: 9 + 6 + 4.
    CHECK_INT(archive_file_size(ARC_DATA), 19);

    archive_fixture_remove();
}

// An empty tree still produces a well-formed archive: the end record alone, and
// a data file with nothing in it.
TEST(an_empty_directory_archives_to_a_bare_end_record)
{
    int tag = -1;
    FILE* stream;

    archive_fixture_remove();
    tig_file_mkdir(ARC_EMPTY);

    CHECK(tig_file_archive(ARC_OUT, ARC_EMPTY));
    CHECK_INT(archive_file_size(ARC_INDEX), 4);
    CHECK_INT(archive_file_size(ARC_DATA), 0);

    stream = fopen(ARC_INDEX, "rb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        CHECK_INT(fread(&tag, sizeof(tag), 1, stream), 1);
        fclose(stream);
    }
    CHECK_INT(tag, 3);

    archive_fixture_remove();
}

// `tig_file_is_directory` guards the entry, and it runs before either output
// file is opened - a refused archive leaves nothing on disk to mistake for one.
TEST(archiving_refuses_a_source_that_is_not_a_directory)
{
    archive_fixture_write();

    CHECK(!tig_file_archive(ARC_OUT, ARC_SRC "/top.txt"));
    CHECK_INT(archive_file_size(ARC_INDEX), -1);
    CHECK_INT(archive_file_size(ARC_DATA), -1);

    CHECK(!tig_file_archive(ARC_OUT, "./arcanum_test_archive_nothing_here"));
    CHECK_INT(archive_file_size(ARC_INDEX), -1);

    archive_fixture_remove();
}

// The reader fails closed on an index that stops before the end record, which
// is what a truncated save looks like. It reports failure rather than silently
// restoring a partial tree - though the files it already wrote do stay, which
// is the original's behaviour too and is why `gamelib` treats a failed load as
// fatal rather than resumable.
TEST(an_index_truncated_before_the_end_record_fails)
{
    long full;
    unsigned char bytes[512];
    FILE* stream;
    size_t got;

    archive_fixture_write();
    CHECK(tig_file_archive(ARC_OUT, ARC_SRC));

    full = archive_file_size(ARC_INDEX);
    CHECK(full > 4);

    stream = fopen(ARC_INDEX, "rb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        got = fread(bytes, 1, sizeof(bytes), stream);
        fclose(stream);

        // Drop the trailing end record, leaving whole records before it.
        CHECK(write_bytes(ARC_INDEX, bytes, got - 4));
        CHECK(!tig_file_unarchive(ARC_OUT, ARC_DST));
    }

    archive_fixture_remove();
}

// --- card R2 / review finding F2 -----------------------------------------
//
// `tig_file_archive` is how every save is written: `gamelib_save` archives
// `Save\Current` onto `save\<name>`. It writes the destination **in place** -
// both halves opened "wb" over the previous archive - so a failure part way
// through does not fail cleanly, it destroys the save that was already there.
//
// Two distinct failures, and the second is the one that loses everything.
// Both tests below assert the property a player depends on: a save that fails
// to be replaced leaves the previous one loadable. Both are expected to FAIL
// until the commit protocol lands.

// Is the archive at ARC_OUT still a complete, readable archive? The
// destination tree is cleared first so a stale unpack cannot pass for a
// successful one.
static const char* const ARCHIVE_DST_PATHS[] = {
    ARC_DST "/sub/deeper/leaf.txt",
    ARC_DST "/sub/inner.bin",
    ARC_DST "/top.txt",
    ARC_DST "/sub/deeper",
    ARC_DST "/sub",
    ARC_DST,
};

static int archive_still_loads(void)
{
    size_t index;

    for (index = 0; index < sizeof(ARCHIVE_DST_PATHS) / sizeof(ARCHIVE_DST_PATHS[0]); index++) {
        remove(ARCHIVE_DST_PATHS[index]);
    }

    return tig_file_unarchive(ARC_OUT, ARC_DST) ? 1 : 0;
}

// Failure 1, and what it exercises changed when staging landed.
//
// Before staging this was the review's case: `.tfai` was opened "wb" first, so
// by the time the `.tfaf` open failed the previous index was already gone.
// Now both opens go to `.tmp` names and succeed, and making `.tfaf` a
// directory instead fails the **rename** that would commit the payload - which
// is the fault-injection point for the commit stage. Either way the property
// asserted is the same one, and it is the one that matters: the previous
// archive is still there and still loads.
TEST(a_failed_replacement_leaves_the_previous_archive_loadable)
{
    long index_before;

    archive_fixture_write();
    CHECK(tig_file_archive(ARC_OUT, ARC_SRC));
    CHECK(archive_still_loads());
    index_before = archive_file_size(ARC_INDEX);
    CHECK(index_before > 4);

    // Make the payload open fail, leaving the index open to succeed: a
    // directory cannot be opened "wb".
    CHECK_INT(remove(ARC_DATA), 0);
    tig_file_mkdir(ARC_DATA);

    // The replacement must fail - it does today, and that part is correct.
    CHECK(!tig_file_archive(ARC_OUT, ARC_SRC));

    // ...and the previous index must have survived it untouched. Note what
    // this case can and cannot assert: forcing the failure means replacing
    // `.tfaf` with a directory, so the *pair* cannot load afterwards - the
    // fixture destroyed the payload, not the code. The loadable-previous-
    // archive property is asserted by the flush-failure case below, where the
    // previous pair is left intact. Asserting it here failed for the fixture's
    // own reason and was removed rather than worked around.
    CHECK_INT(archive_file_size(ARC_INDEX), index_before);

    // Cleanup stage: a failed archive leaves no staging behind for the next
    // run to trip over.
    CHECK_INT(archive_file_size(ARC_INDEX ".tmp"), -1);
    CHECK_INT(archive_file_size(ARC_DATA ".tmp"), -1);

    remove(ARC_DATA);
    archive_fixture_remove();
}

// Failure 2: a write that fails mid-archive is REPORTED AS A SUCCESSFUL SAVE.
//
// This is not the case the review reproduced, and it is worse than the one it
// described. The writes are buffered, so `tig_file_fwrite` and
// `tig_file_fputs` return success into the FILE* buffer and the worker sees
// nothing wrong. The failure only materialises when the buffer is flushed at
// `tig_file_fclose` - and `tig_file_archive_native` **ignores both fclose
// return values**. So `success` is still true, the `!success` cleanup never
// runs, and the function returns true.
//
// `gamelib_save` takes that true and reports "Save Complete." The player is
// told the save worked. On disk the previous archive has been overwritten
// with a truncated fragment, and it no longer loads.
//
// Measured 2026-09-08: over a valid 97-byte index, this leaves 12 bytes and
// returns true.
//
// RLIMIT_FSIZE is how the write is made to fail without a full filesystem:
// past the limit the flush fails with EFBIG. SIGXFSZ would otherwise kill the
// process, so it is ignored for the duration. The child keeps both the limit
// and the signal disposition away from the rest of the suite, and carries
// what `tig_file_archive` claimed out through its exit status.
TEST(a_failure_during_the_write_is_not_reported_as_a_successful_save)
{
    struct rlimit saved;
    long index_before;
    pid_t child;
    int status = 0;
    int claimed_success;

    archive_fixture_write();
    CHECK(tig_file_archive(ARC_OUT, ARC_SRC));
    index_before = archive_file_size(ARC_INDEX);
    CHECK(index_before > 4);

    fflush(NULL);
    child = fork();
    if (child == 0) {
        struct rlimit small;

        signal(SIGXFSZ, SIG_IGN);
        getrlimit(RLIMIT_FSIZE, &saved);
        // Enough for the first records, not for the whole index.
        small.rlim_cur = 12;
        small.rlim_max = saved.rlim_max;
        setrlimit(RLIMIT_FSIZE, &small);

        _exit(tig_file_archive(ARC_OUT, ARC_SRC) ? 7 : 0);
    }

    CHECK(child > 0);
    if (child > 0) {
        CHECK(waitpid(child, &status, 0) == child);
        // The child must have RETURNED from tig_file_archive, not died in it.
        // If SIGXFSZ killed it, the damage below would be this test's doing
        // rather than the code's, and the case would prove nothing.
        CHECK(WIFEXITED(status));
        CHECK_INT(WIFSIGNALED(status) ? WTERMSIG(status) : 0, 0);
    }

    claimed_success = (WIFEXITED(status) && WEXITSTATUS(status) == 7) ? 1 : 0;

    // The sharp one: a save that did not happen must not report that it did.
    CHECK_INT(claimed_success, 0);

    // And the previous archive must have survived.
    CHECK_INT(archive_file_size(ARC_INDEX), index_before);
    CHECK(archive_still_loads());

    // Cleanup stage again, this time after a failure detected at the flush.
    CHECK_INT(archive_file_size(ARC_INDEX ".tmp"), -1);
    CHECK_INT(archive_file_size(ARC_DATA ".tmp"), -1);

    archive_fixture_remove();
}

// The game's path shape, which the cases above do NOT exercise.
//
// `gamelib_save` archives to `save\<name>` - a *repository-relative* path. The
// cases above all use "./arcanum_test_archive_out", and a leading '.' takes an
// entirely different branch in `tig_file_remove_native`, `_rename_native` and
// `_fopen_native`: it goes straight to SDL, while a relative name walks the
// mounted repositories instead.
//
// That difference is not academic. Card R2's first attempt at staging passed
// every case above and then produced no archive at all in the real game,
// leaving `SlotAuto.tfai.tmp` and `SlotAuto.tfaf.tmp` behind - caught by the
// save matrix, not by this suite. This case is that gap closed.
TEST(archiving_to_a_repository_relative_path_lands_on_the_real_name)
{
    char dir_out[TIG_MAX_PATH];
    char dir_index[TIG_MAX_PATH];
    char dir_tmp[TIG_MAX_PATH];

    archive_fixture_write();
    remove(ARC_REPO_INDEX);
    remove(ARC_REPO_DATA);
    remove(ARC_REPO_TMP_INDEX);
    remove(ARC_REPO_TMP_DATA);
    tig_file_mkdir(ARC_REPO);

    CHECK(tig_file_repository_add(ARC_REPO));

    // Relative, exactly as `gamelib_save` spells it - including the
    // subdirectory, because `save\\SlotAuto` has one and a bare name does not.
    tig_file_mkdir(ARC_REPO_SUB);
    CHECK(tig_file_archive("sub\\out", ARC_SRC));

    CHECK(archive_file_size(ARC_REPO_SUB_INDEX) > 4);
    CHECK_INT(archive_file_size(ARC_REPO_SUB_TMP), -1);

    CHECK(tig_file_archive("out", ARC_SRC));

    // The archive must exist under its real name...
    CHECK(archive_file_size(ARC_REPO_INDEX) > 4);
    CHECK(archive_file_size(ARC_REPO_DATA) > 0);

    // ...and no staging file may survive a success.
    CHECK_INT(archive_file_size(ARC_REPO_TMP_INDEX), -1);
    CHECK_INT(archive_file_size(ARC_REPO_TMP_DATA), -1);

    // The case-mismatch shape, which is what the game actually has: the
    // directory on disk is `Save`, `gamelib_save` asks for `save\\SlotAuto`,
    // and only the *open* path calls `compat_resolve_path` after joining the
    // repository root. `tig_file_rename_native` and `tig_file_remove_native`
    // do not, so on a case-sensitive filesystem they are handed a name that
    // does not exist and report failure.
    tig_file_mkdir(ARC_REPO_CASE);
    CHECK(tig_file_archive("case\\out", ARC_SRC));
    CHECK(archive_file_size(ARC_REPO_CASE_INDEX) > 4);
    CHECK_INT(archive_file_size(ARC_REPO_CASE_TMP), -1);

    (void)dir_out;
    (void)dir_index;
    (void)dir_tmp;

    remove(ARC_REPO_CASE_INDEX);
    remove(ARC_REPO_CASE_TMP);
    remove(ARC_REPO_CASE "/out.tfaf");
    remove(ARC_REPO_CASE "/out.tfaf.tmp");
    remove(ARC_REPO_CASE);
    remove(ARC_REPO_SUB_INDEX);
    remove(ARC_REPO_SUB_TMP);
    remove(ARC_REPO_SUB "/out.tfaf");
    remove(ARC_REPO_SUB "/out.tfaf.tmp");
    remove(ARC_REPO_SUB);
    remove(ARC_REPO_INDEX);
    remove(ARC_REPO_DATA);
    remove(ARC_REPO_TMP_INDEX);
    remove(ARC_REPO_TMP_DATA);
    remove(ARC_REPO);
    archive_fixture_remove();
}

// A missing archive is a failure, not an empty success.
TEST(unarchiving_an_absent_archive_fails)
{
    archive_fixture_remove();

    CHECK(!tig_file_unarchive(ARC_OUT, ARC_DST));

    // The index exists but the data file does not - the second open fails.
    CHECK(write_text(ARC_INDEX, "xxxx"));
    CHECK(!tig_file_unarchive(ARC_OUT, ARC_DST));

    archive_fixture_remove();
}

void test_file_suite(void)
{
    RUN(the_archive_mounted_last_wins);
    RUN(reversing_the_mount_order_reverses_the_answer);
    RUN(a_directory_mounted_after_an_archive_beats_it);
    RUN(a_directory_is_not_a_file);
    RUN(a_directory_mounted_before_an_archive_loses_to_it);
    RUN(an_ignore_sidecar_falls_through_to_the_next_repository);
    RUN(an_entry_already_read_from_an_archive_ignores_a_later_override);
    RUN(exists_agrees_with_fopen);
    RUN(a_listing_merges_every_repository);
    RUN(a_shared_name_appears_once_and_describes_the_copy_that_wins);
    RUN(a_listing_is_sorted_case_insensitively);
    RUN(the_glob_that_decides_the_mount_order);
    RUN(a_listing_covers_one_level_and_marks_directories);
    RUN(an_ignored_entry_is_left_out_of_the_listing);
    RUN(an_archived_tree_comes_back_byte_for_byte);
    RUN(the_index_is_the_record_stream_the_format_says);
    RUN(an_empty_directory_archives_to_a_bare_end_record);
    RUN(archiving_refuses_a_source_that_is_not_a_directory);
    RUN(an_index_truncated_before_the_end_record_fails);
    RUN(archiving_to_a_repository_relative_path_lands_on_the_real_name);
    RUN(a_failed_replacement_leaves_the_previous_archive_loadable);
    RUN(a_failure_during_the_write_is_not_reported_as_a_successful_save);
    RUN(unarchiving_an_absent_archive_fails);
}
