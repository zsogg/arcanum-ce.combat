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
#define MOD_PATH "./arcanum_test_repo_mod.dat"
#define MOD_IGNORE_PATH "./arcanum_test_repo_mod.dat.ignore"
#define DIR_PATH "./arcanum_test_repo_dir"
#define DIR_RULES DIR_PATH "/rules"
#define DIR_HELLO DIR_RULES "/hello.txt"

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
    tig_file_repository_remove(DIR_PATH);
    tig_file_repository_remove(MOD_PATH);
    tig_file_repository_remove(BASE_PATH);
    remove(BASE_PATH);
    remove(MOD_PATH);
    remove(MOD_IGNORE_PATH);
    remove(DIR_HELLO);
    remove(DIR_RULES);
    remove(DIR_PATH);
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

void test_file_suite(void)
{
    RUN(the_archive_mounted_last_wins);
    RUN(reversing_the_mount_order_reverses_the_answer);
    RUN(a_directory_mounted_after_an_archive_beats_it);
    RUN(a_directory_mounted_before_an_archive_loses_to_it);
    RUN(an_ignore_sidecar_falls_through_to_the_next_repository);
    RUN(exists_agrees_with_fopen);
}
