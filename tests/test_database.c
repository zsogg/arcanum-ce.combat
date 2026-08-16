// Tests for `tig/database.c`, the .dat archive reader.
//
// This is the only suite whose fixtures were not written by us. Both archives
// below were produced by Troika's own `mod-info/dbmaker.exe`, the archiver
// shipped with the world editor, so what they pin is agreement with the
// format's author - not agreement with our reading of `database.c`, which is
// what every check derived from CE's own code can manage. That distinction is
// the point: the reader was ported from a binary, and a transcription cannot
// falsify itself.
//
// The wide version of the same check lives in
// `arcanum-work/scripts/dbmaker_diff.py`, which unpacks all 17 shipped archives
// with dbmaker and compares 21,639 payloads against ours. This suite is the
// part that needs neither Wine nor the game's assets, so it can run in CI:
// `arcanum-work/scripts/make_db_fixtures.py` rebuilds the two arrays below and
// records the exact dbmaker commands.
//
// What the fixtures cover, chosen because the shipped corpus does not exercise
// it: a zero-length entry (there are none in the 21,613 shipped entries), a
// stored-uncompressed entry next to a compressed one in the same archive, a
// payload larger than the 16 KB decompression buffer so the inflate loop turns
// over, and the separator-ordering hazard described at
// `finds_an_entry_when_a_sibling_directory_shares_its_prefix`.
//
// The archives are written to disk because `tig_database_open` takes a path,
// and are removed by the tests that write them.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tig/compat.h"
#include "tig/database.h"

#include "test_harness.h"

#define MIXED_PATH "./arcanum_test_fixture_mixed.dat"
#define MIXED_IGNORE_PATH "./arcanum_test_fixture_mixed.dat.ignore"
#define ORDER_PATH "./arcanum_test_fixture_order.dat"

#define BIG_SIZE 40000
#define NOISE_SIZE 256

#include "test_database_fixtures.inc"

static bool write_fixture(const char* path, const unsigned char* data, size_t size)
{
    FILE* stream = fopen(path, "wb");

    if (stream == NULL) {
        return false;
    }

    fwrite(data, 1, size, stream);
    fclose(stream);

    return true;
}

static bool write_text_fixture(const char* path, const char* contents)
{
    FILE* stream = fopen(path, "wb");

    if (stream == NULL) {
        return false;
    }

    fwrite(contents, 1, strlen(contents), stream);
    fclose(stream);

    return true;
}

// `bin/big.bin` is 40000 bytes of this ramp: deterministic, and with a period
// of 256 it compresses to 487 bytes, which keeps the fixture small while still
// inflating to more than the 16 KB decompression buffer.
static unsigned char big_byte(int index)
{
    return (unsigned char)((index * 7 + 3) & 0xFF);
}

// `bin/noise.bin` is 256 bytes of xorshift32 from seed 0x1234567. It is
// incompressible, which is why dbmaker stored it plain rather than deflated -
// the same reason 65 of the shipped entries are plain.
static void fill_noise(unsigned char* buffer, int count)
{
    unsigned int state = 0x1234567;
    int index;

    for (index = 0; index < count; index++) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        buffer[index] = (unsigned char)(state & 0xFF);
    }
}

static TigDatabase* open_mixed(void)
{
    if (!write_fixture(MIXED_PATH, FIXTURE_MIXED, sizeof(FIXTURE_MIXED))) {
        return NULL;
    }

    return tig_database_open(MIXED_PATH);
}

static void close_and_remove(TigDatabase* database, const char* path)
{
    if (database != NULL) {
        tig_database_close(database);
    }
    remove(path);
}

// dbmaker writes one entry per file plus one per directory, in its own order.
// Nothing here should surprise anyone; it is the baseline the rest builds on.
TEST(indexes_every_entry_dbmaker_wrote)
{
    TigDatabase* database = open_mixed();
    TigDatabaseEntry* entry = NULL;

    CHECK(database != NULL);
    if (database == NULL) {
        return;
    }

    CHECK_INT(database->entries_count, 6);

    CHECK(tig_database_get_entry(database, "bin\\big.bin", &entry));
    CHECK_INT(entry->size, BIG_SIZE);
    CHECK_INT(entry->flags & TIG_DATABASE_ENTRY_COMPRESSED, TIG_DATABASE_ENTRY_COMPRESSED);
    CHECK(entry->compressed_size < entry->size);

    CHECK(tig_database_get_entry(database, "bin\\noise.bin", &entry));
    CHECK_INT(entry->size, NOISE_SIZE);
    CHECK_INT(entry->flags & TIG_DATABASE_ENTRY_PLAIN, TIG_DATABASE_ENTRY_PLAIN);
    CHECK_INT(entry->compressed_size, NOISE_SIZE);

    CHECK(tig_database_get_entry(database, "text\\empty.txt", &entry));
    CHECK_INT(entry->size, 0);

    // The directory entries are indexed too, and carry the directory flag.
    CHECK(tig_database_get_entry(database, "bin", &entry));
    CHECK_INT(entry->flags & TIG_DATABASE_ENTRY_DIRECTORY, TIG_DATABASE_ENTRY_DIRECTORY);

    close_and_remove(database, MIXED_PATH);
}

// Lookups take a Windows path and are case-insensitive, because `open` folds
// both: separators to the platform's and the whole name to lower case.
TEST(looks_up_entries_by_windows_path_and_any_case)
{
    TigDatabase* database = open_mixed();
    TigDatabaseEntry* entry = NULL;

    CHECK(database != NULL);
    if (database == NULL) {
        return;
    }

    CHECK(tig_database_get_entry(database, "BIN\\BIG.BIN", &entry));
    CHECK(tig_database_get_entry(database, "Bin\\Big.Bin", &entry));
    CHECK(!tig_database_get_entry(database, "bin\\missing.bin", &entry));
    CHECK(entry == NULL);

    close_and_remove(database, MIXED_PATH);
}

// The compressed path, end to end: 40000 bytes through a 16 KB buffer, so the
// inflate loop refills at least twice.
TEST(reads_a_compressed_entry_whole)
{
    TigDatabase* database = open_mixed();
    TigDatabaseFileHandle* stream;
    unsigned char* buffer;
    int index;
    int mismatches = 0;

    CHECK(database != NULL);
    if (database == NULL) {
        return;
    }

    stream = tig_database_fopen(database, "bin\\big.bin", "rb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        CHECK_INT(tig_database_filelength(stream), BIG_SIZE);

        buffer = (unsigned char*)malloc(BIG_SIZE);
        CHECK_INT(tig_database_fread(buffer, 1, BIG_SIZE, stream), BIG_SIZE);
        CHECK_INT(tig_database_ftell(stream), BIG_SIZE);

        for (index = 0; index < BIG_SIZE; index++) {
            if (buffer[index] != big_byte(index)) {
                mismatches++;
            }
        }
        CHECK_INT(mismatches, 0);

        free(buffer);
        tig_database_fclose(stream);
    }

    close_and_remove(database, MIXED_PATH);
}

// The plain path. dbmaker chose it for this entry, so the bytes on disk are the
// payload and nothing inflates.
TEST(reads_a_plain_entry_whole)
{
    TigDatabase* database = open_mixed();
    TigDatabaseFileHandle* stream;
    unsigned char actual[NOISE_SIZE];
    unsigned char expected[NOISE_SIZE];

    CHECK(database != NULL);
    if (database == NULL) {
        return;
    }

    fill_noise(expected, NOISE_SIZE);

    stream = tig_database_fopen(database, "bin\\noise.bin", "rb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        CHECK_INT(tig_database_filelength(stream), NOISE_SIZE);
        CHECK_INT(tig_database_fread(actual, 1, NOISE_SIZE, stream), NOISE_SIZE);
        CHECK_INT(memcmp(actual, expected, NOISE_SIZE), 0);
        tig_database_fclose(stream);
    }

    close_and_remove(database, MIXED_PATH);
}

// A read that runs past the end returns what was there and stops. `fread`
// reports whole elements, so asking for two 24-byte records out of a 24-byte
// entry yields one.
TEST(clamps_a_read_at_the_end_of_an_entry)
{
    TigDatabase* database = open_mixed();
    TigDatabaseFileHandle* stream;
    char buffer[64];

    CHECK(database != NULL);
    if (database == NULL) {
        return;
    }

    stream = tig_database_fopen(database, "text\\hello.txt", "rb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        memset(buffer, 0, sizeof(buffer));
        CHECK_INT(tig_database_fread(buffer, 24, 2, stream), 1);
        CHECK_STR(buffer, "line one\r\nline two\r\ntail");
        CHECK(tig_database_feof(stream) != 0);
        tig_database_fclose(stream);
    }

    close_and_remove(database, MIXED_PATH);
}

// A zero-length entry. None ship - all 21,613 shipped entries have a payload -
// so this path had never been exercised by anything.
TEST(reads_a_zero_length_entry)
{
    TigDatabase* database = open_mixed();
    TigDatabaseFileHandle* stream;
    char buffer[8];

    CHECK(database != NULL);
    if (database == NULL) {
        return;
    }

    stream = tig_database_fopen(database, "text\\empty.txt", "rb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        CHECK_INT(tig_database_filelength(stream), 0);
        CHECK_INT(tig_database_fread(buffer, 1, sizeof(buffer), stream), 0);
        CHECK(tig_database_feof(stream) != 0);

        // The original reads through `ReadFile`, which succeeds when asked for
        // zero bytes, so nothing is in error here. See the NOTE on
        // `tig_database_fread_internal`.
        CHECK_INT(tig_database_ferror(stream), 0);

        // ... and because it is not in error, the handle is still usable.
        CHECK_INT(tig_database_fseek(stream, 0, SEEK_SET), 0);

        tig_database_fclose(stream);
    }

    close_and_remove(database, MIXED_PATH);
}

// Seeking backwards in a compressed entry cannot rewind the inflate state, so
// the reader restarts the stream and skips forward. That is the expensive path
// and the one most likely to be wrong; the ramp makes any drift visible.
TEST(seeks_within_a_compressed_entry)
{
    TigDatabase* database = open_mixed();
    TigDatabaseFileHandle* stream;
    unsigned char buffer[4];

    CHECK(database != NULL);
    if (database == NULL) {
        return;
    }

    stream = tig_database_fopen(database, "bin\\big.bin", "rb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        // Forward, past the 16 KB decompression buffer, so the skip loop runs
        // more than once.
        CHECK_INT(tig_database_fseek(stream, 39000, SEEK_SET), 0);
        CHECK_INT(tig_database_ftell(stream), 39000);
        CHECK_INT(tig_database_fread(buffer, 1, 4, stream), 4);
        CHECK_INT(buffer[0], big_byte(39000));
        CHECK_INT(buffer[3], big_byte(39003));

        // Backwards, which restarts inflation from the entry's first byte.
        CHECK_INT(tig_database_fseek(stream, 5, SEEK_SET), 0);
        CHECK_INT(tig_database_fread(buffer, 1, 4, stream), 4);
        CHECK_INT(buffer[0], big_byte(5));
        CHECK_INT(buffer[3], big_byte(8));

        // Relative and from-the-end origins address the same bytes.
        CHECK_INT(tig_database_fseek(stream, 10, SEEK_CUR), 0);
        CHECK_INT(tig_database_ftell(stream), 19);
        CHECK_INT(tig_database_fseek(stream, -4, SEEK_END), 0);
        CHECK_INT(tig_database_fread(buffer, 1, 4, stream), 4);
        CHECK_INT(buffer[3], big_byte(BIG_SIZE - 1));

        // Seeking to the very end is allowed (CE's own FIX, marked as such in
        // `tig_database_fseek`); seeking past it is an error and latches.
        CHECK_INT(tig_database_fseek(stream, 0, SEEK_END), 0);
        CHECK_INT(tig_database_fseek(stream, BIG_SIZE + 1, SEEK_SET), 1);
        CHECK(tig_database_ferror(stream) != 0);

        tig_database_fclose(stream);
    }

    close_and_remove(database, MIXED_PATH);
}

// A plain entry seeks by moving the underlying file position, with no state to
// rebuild - the cheap path, checked so a change to the shared code cannot break
// one branch without the other noticing.
TEST(seeks_within_a_plain_entry)
{
    TigDatabase* database = open_mixed();
    TigDatabaseFileHandle* stream;
    unsigned char expected[NOISE_SIZE];
    unsigned char buffer[4];

    CHECK(database != NULL);
    if (database == NULL) {
        return;
    }

    fill_noise(expected, NOISE_SIZE);

    stream = tig_database_fopen(database, "bin\\noise.bin", "rb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        CHECK_INT(tig_database_fseek(stream, 200, SEEK_SET), 0);
        CHECK_INT(tig_database_fread(buffer, 1, 4, stream), 4);
        CHECK_INT(memcmp(buffer, &expected[200], 4), 0);

        CHECK_INT(tig_database_fseek(stream, 0, SEEK_SET), 0);
        CHECK_INT(tig_database_fread(buffer, 1, 4, stream), 4);
        CHECK_INT(memcmp(buffer, expected, 4), 0);

        tig_database_fclose(stream);
    }

    close_and_remove(database, MIXED_PATH);
}

// Text mode collapses CRLF to a single newline on the way out, which is what
// every .mes and .dlg reader in the game depends on.
TEST(collapses_crlf_in_text_mode)
{
    TigDatabase* database = open_mixed();
    TigDatabaseFileHandle* stream;
    char line[32];

    CHECK(database != NULL);
    if (database == NULL) {
        return;
    }

    stream = tig_database_fopen(database, "text\\hello.txt", "rt");
    CHECK(stream != NULL);
    if (stream != NULL) {
        CHECK(tig_database_fgets(line, sizeof(line), stream) != NULL);
        CHECK_STR(line, "line one\n");
        CHECK(tig_database_fgets(line, sizeof(line), stream) != NULL);
        CHECK_STR(line, "line two\n");

        // The last line has no terminator; `fgets` returns what is left.
        CHECK(tig_database_fgets(line, sizeof(line), stream) != NULL);
        CHECK_STR(line, "tail");

        tig_database_fclose(stream);
    }

    // The same bytes in binary mode keep their carriage returns.
    stream = tig_database_fopen(database, "text\\hello.txt", "rb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        CHECK_INT(tig_database_fgetc(stream), 'l');
        CHECK_INT(tig_database_fseek(stream, 8, SEEK_SET), 0);
        CHECK_INT(tig_database_fgetc(stream), '\r');
        CHECK_INT(tig_database_fgetc(stream), '\n');
        tig_database_fclose(stream);
    }

    close_and_remove(database, MIXED_PATH);
}

// `ungetc` pushes a byte back for the next read to hand out, and the position
// moves with it.
TEST(ungetc_hands_the_byte_to_the_next_read)
{
    TigDatabase* database = open_mixed();
    TigDatabaseFileHandle* stream;
    char buffer[8];

    CHECK(database != NULL);
    if (database == NULL) {
        return;
    }

    stream = tig_database_fopen(database, "text\\hello.txt", "rb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        CHECK_INT(tig_database_fgetc(stream), 'l');
        CHECK_INT(tig_database_ungetc('X', stream), 'X');
        CHECK_INT(tig_database_ftell(stream), 0);
        CHECK_INT(tig_database_fgetc(stream), 'X');
        CHECK_INT(tig_database_fgetc(stream), 'i');

        // Only one byte deep: a second `ungetc` without a read in between is
        // refused rather than stacked.
        CHECK_INT(tig_database_ungetc('Y', stream), 'Y');
        CHECK_INT(tig_database_ungetc('Z', stream), -1);

        memset(buffer, 0, sizeof(buffer));
        CHECK_INT(tig_database_fread(buffer, 1, 3, stream), 3);
        CHECK_INT(buffer[0], 'Y');
        CHECK_INT(buffer[1], 'n');
        CHECK_INT(buffer[2], 'e');

        tig_database_fclose(stream);
    }

    close_and_remove(database, MIXED_PATH);
}

// Directory enumeration. The pattern is matched against whole entry paths and
// the depth has to agree, so `bin\*.*` lists what is in `bin` and nothing
// underneath it.
TEST(enumerates_entries_matching_a_pattern)
{
    TigDatabase* database = open_mixed();
    TigDatabaseFindFileData ffd;
    int found = 0;
    bool saw_big = false;
    bool saw_noise = false;

    CHECK(database != NULL);
    if (database == NULL) {
        return;
    }

    if (tig_database_find_first_entry(database, "bin\\*.*", &ffd)) {
        do {
            found++;
            if (strcmp(ffd.name, "bin" PATH_SEPARATOR_STR "big.bin") == 0) {
                saw_big = true;
                CHECK_INT(ffd.size, BIG_SIZE);
                CHECK(!ffd.is_directory);
            }
            if (strcmp(ffd.name, "bin" PATH_SEPARATOR_STR "noise.bin") == 0) {
                saw_noise = true;
            }
        } while (tig_database_find_next_entry(&ffd));
        tig_database_find_close(&ffd);
    }

    CHECK_INT(found, 2);
    CHECK(saw_big);
    CHECK(saw_noise);

    close_and_remove(database, MIXED_PATH);
}

// A `<archive>.dat.ignore` sidecar next to the archive marks entries as if they
// were not there. It is a plain text file of patterns, one per line, and it is
// how a mod hides a file the base game ships without rebuilding the archive.
TEST(an_ignore_sidecar_hides_matching_entries)
{
    TigDatabase* database;
    TigDatabaseEntry* entry = NULL;

    CHECK(write_fixture(MIXED_PATH, FIXTURE_MIXED, sizeof(FIXTURE_MIXED)));
    CHECK(write_text_fixture(MIXED_IGNORE_PATH, "bin\\*.bin\n"));

    database = tig_database_open(MIXED_PATH);
    CHECK(database != NULL);
    if (database != NULL) {
        CHECK(!tig_database_get_entry(database, "bin\\big.bin", &entry));
        CHECK(!tig_database_get_entry(database, "bin\\noise.bin", &entry));
        CHECK(tig_database_get_entry(database, "text\\hello.txt", &entry));
        CHECK(tig_database_fopen(database, "bin\\big.bin", "rb") == NULL);
    }

    close_and_remove(database, MIXED_PATH);
    remove(MIXED_IGNORE_PATH);
}

// Porting bug #13, and the reason this fixture exists.
//
// dbmaker sorts the entry table by the path as *it* writes it, with backslash
// separators, and both searches over that table - the `bsearch` in
// `tig_database_get_entry` and the hand-rolled one in
// `tig_database_find_prepare` - need the array in the order their comparator
// expects. On Windows that holds. On every other platform `tig_database_open`
// rewrites the separators to '/' first, and '/' (0x2F) does not sort where '\'
// (0x5C) did, so any archive holding a directory whose name is a prefix of a
// sibling's comes out unsorted and the searches walk past entries that exist.
//
// `order.dat` is four entries - `art`, `art2`, `art2\b.txt`, `art\a.txt` - and
// that is the whole shape: '2' sorts before '\' and after '/'. None of the 17
// shipped archives contain the pattern, which is why nothing caught it, but a
// mod built with Troika's own tool can.
TEST(finds_an_entry_when_a_sibling_directory_shares_its_prefix)
{
    TigDatabase* database;
    TigDatabaseEntry* entry = NULL;
    TigDatabaseFindFileData ffd;
    int found = 0;

    CHECK(write_fixture(ORDER_PATH, FIXTURE_ORDER, sizeof(FIXTURE_ORDER)));

    database = tig_database_open(ORDER_PATH);
    CHECK(database != NULL);
    if (database == NULL) {
        remove(ORDER_PATH);
        return;
    }

    CHECK_INT(database->entries_count, 4);

    CHECK(tig_database_get_entry(database, "art2\\b.txt", &entry));
    CHECK(tig_database_get_entry(database, "art\\a.txt", &entry));

    // The same ordering feeds the pattern search, which starts from a binary
    // search on the fixed prefix of the pattern.
    if (tig_database_find_first_entry(database, "art\\*.*", &ffd)) {
        do {
            found++;
        } while (tig_database_find_next_entry(&ffd));
        tig_database_find_close(&ffd);
    }
    CHECK_INT(found, 1);

    close_and_remove(database, ORDER_PATH);
}

void test_database_suite(void)
{
    RUN(indexes_every_entry_dbmaker_wrote);
    RUN(looks_up_entries_by_windows_path_and_any_case);
    RUN(reads_a_compressed_entry_whole);
    RUN(reads_a_plain_entry_whole);
    RUN(clamps_a_read_at_the_end_of_an_entry);
    RUN(reads_a_zero_length_entry);
    RUN(seeks_within_a_compressed_entry);
    RUN(seeks_within_a_plain_entry);
    RUN(collapses_crlf_in_text_mode);
    RUN(ungetc_hands_the_byte_to_the_next_read);
    RUN(enumerates_entries_matching_a_pattern);
    RUN(an_ignore_sidecar_hides_matching_entries);
    RUN(finds_an_entry_when_a_sibling_directory_shares_its_prefix);
}
