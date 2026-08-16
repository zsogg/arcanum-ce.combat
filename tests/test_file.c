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

void test_file_suite(void)
{
    RUN(the_archive_mounted_last_wins);
    RUN(reversing_the_mount_order_reverses_the_answer);
    RUN(a_directory_mounted_after_an_archive_beats_it);
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
}
