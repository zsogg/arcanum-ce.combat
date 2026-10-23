// The readers of formats a mod ships that card H6's fuzzer did not reach
// refuse a header the file cannot back (review 2026-10-22, finding 4).
//
// Each case writes a crafted file where the reader looks for it and calls
// the public entry point. Against the previous readers these cases crashed
// or returned true for nonsense: a `.dat` whose names overrun its name
// table, a facade grid of 2^40 cells, a blocked-sector count of -1, a
// terrain a billion tiles wide. The `.art` decoder's bound (`art.c`) has no
// case here: it sits behind the art cache, which needs the video layer.
#include <stdio.h>
#include <string.h>

#include "game/sector.h"
#include "game/terrain.h"
#include "game/walkmask.h"
#include "test_harness.h"
#include "tig/art.h"
#include "tig/database.h"
#include "tig/file.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#endif

#define REPO "./arcanum_test_mod_formats"

// -- a byte builder ----------------------------------------------------------

static unsigned char bytes[4096];
static size_t bytes_len;

static void put(const void* data, size_t size)
{
    memcpy(bytes + bytes_len, data, size);
    bytes_len += size;
}

static void put_int(int value)
{
    put(&value, sizeof(value));
}

static void put_zeros(size_t count)
{
    memset(bytes + bytes_len, 0, count);
    bytes_len += count;
}

static void write_fixture(const char* path)
{
    FILE* stream = fopen(path, "wb");
    CHECK(stream != NULL);
    if (stream != NULL) {
        fwrite(bytes, 1, bytes_len, stream);
        fclose(stream);
    }
    bytes_len = 0;
}

// -- .dat --------------------------------------------------------------------

// A " TAD" archive with no payload: the payload size (the field the reader
// calls `entry_table_size`), the entry count, one entry (name length, name,
// 4 skipped bytes, flags, size, compressed size, offset), then the footer
// (fourcc, name table size, and the distance from the end back to the
// payload-size field).
// `terminated` writes the name as dbmaker does, with its trailing NUL inside
// `name_size`.
static void write_dat(int name_table_size, int name_size, bool terminated)
{
    int table_bytes;
    int fourcc;

    memcpy(&fourcc, " TAD", 4);
    table_bytes = 4 + 4 + name_size + 4 + 4 * 4;
    put_int(0); // payload size
    put_int(1); // one entry
    put_int(name_size);
    memset(bytes + bytes_len, 'n', (size_t)name_size);
    if (terminated) {
        bytes[bytes_len + (size_t)name_size - 1] = '\0';
    }
    bytes_len += (size_t)name_size;
    put_zeros(4);
    put_int(0); // flags
    put_int(0); // size
    put_int(0); // compressed size
    put_int(0); // offset
    put_int(fourcc);
    put_int(name_table_size);
    put_int(table_bytes + 12);
    write_fixture(REPO "/bounds.dat");
}

TEST(a_dat_whose_names_overrun_its_name_table_is_refused)
{
    TigDatabase* database;

    // Eight bytes of name table, a one-hundred-byte name: the previous
    // reader read the name into the eight-byte allocation.
    write_dat(8, 100, true);
    database = tig_database_open(REPO "/bounds.dat");
    CHECK(database == NULL);
    if (database != NULL) {
        tig_database_close(database);
    }
    remove(REPO "/bounds.dat");
}

TEST(a_dat_whose_name_has_no_terminator_is_refused)
{
    TigDatabase* database;

    // Found by this suite's own first fixture under ASan: the name is walked
    // as a C string, so one without a terminator reads past the table.
    write_dat(16, 5, false);
    database = tig_database_open(REPO "/bounds.dat");
    CHECK(database == NULL);
    if (database != NULL) {
        tig_database_close(database);
    }
    remove(REPO "/bounds.dat");
}

TEST(a_dat_whose_name_fits_its_name_table_opens)
{
    TigDatabase* database;

    write_dat(16, 6, true);
    database = tig_database_open(REPO "/bounds.dat");
    CHECK(database != NULL);
    if (database != NULL) {
        CHECK_INT((int)database->entries_count, 1);
        tig_database_close(database);
    }
    remove(REPO "/bounds.dat");
}

// -- facade walk masks -------------------------------------------------------

static void write_facwalk(int width, int height, int cnt)
{
    put("FacWalk V101  ", 14);
    put_int(0); // tile
    put_int(0); // outdoor
    put_int(0); // flippable
    put_int(width);
    put_int(height);
    put_int(cnt);
    write_fixture(REPO "/art/Facade/facwalk.01");
}

TEST(a_facade_grid_the_product_cannot_hold_is_refused)
{
    tig_art_id_t* ids = NULL;
    int width = 0;
    int height = 0;

    // 2^20 by 2^20: the product wrapped to zero, the allocation was empty,
    // and the previous reader then wrote 2^40 invalid ids into it.
    write_facwalk(1 << 20, 1 << 20, 0);
    CHECK(!walkmask_load(1, &ids, &width, &height));
    CHECK(ids == NULL);
}

TEST(a_small_facade_still_loads)
{
    tig_art_id_t* ids = NULL;
    int width = 0;
    int height = 0;

    write_facwalk(2, 2, 0);
    CHECK(walkmask_load(1, &ids, &width, &height));
    CHECK_INT(width, 2);
    CHECK_INT(height, 2);
    if (ids != NULL) {
        FREE(ids);
    }
}

// -- blocked sectors ---------------------------------------------------------

TEST(a_negative_blocked_sector_count_is_refused)
{
    // -1 sized the allocation at eight times -1; the read into it followed.
    put_int(-1);
    put_zeros(8);
    write_fixture(REPO "/cur/map.sbf");
    CHECK(!sector_block_load("base", "cur"));
}

TEST(a_blocked_sector_count_past_the_file_is_refused)
{
    put_int(100);
    put_zeros(8);
    write_fixture(REPO "/cur/map.sbf");
    CHECK(!sector_block_load("base", "cur"));
}

TEST(a_blocked_sector_list_the_file_holds_loads)
{
    put_int(1);
    put_zeros(8);
    write_fixture(REPO "/cur/map.sbf");
    CHECK(sector_block_load("base", "cur"));
}

// -- terrain -----------------------------------------------------------------

static void write_terrain(long long width, long long height)
{
    float version = 1.2f;
    unsigned int flags = 0x1; // compressed rows: the row cache is sized by width

    put(&version, sizeof(version));
    put(&flags, sizeof(flags));
    put(&width, sizeof(width));
    put(&height, sizeof(height));
    put_int(0); // base terrain type
    put_int(0); // padding
    put_zeros(16); // "tile data"
    write_fixture(REPO "/base/terrain.tdf");
}

TEST(a_terrain_a_billion_tiles_wide_is_refused)
{
    // The previous reader allocated four two-gigabyte row buffers and
    // returned true.
    write_terrain(1LL << 30, 1);
    CHECK(!terrain_open("base", "base"));
    terrain_map_close();
}

TEST(a_terrain_of_negative_height_is_refused)
{
    write_terrain(64, -1);
    CHECK(!terrain_open("base", "base"));
    terrain_map_close();
}

TEST(a_plausible_terrain_header_still_opens)
{
    write_terrain(64, 64);
    CHECK(terrain_open("base", "base"));
    terrain_map_close();
}

// -- suite -------------------------------------------------------------------

static void erase_fixture_dir(void)
{
    remove(REPO "/bounds.dat");
    remove(REPO "/art/Facade/facwalk.01");
    remove(REPO "/cur/map.sbf");
    remove(REPO "/base/terrain.tdf");
    tig_file_rmdir(REPO "/art/Facade");
    tig_file_rmdir(REPO "/art");
    tig_file_rmdir(REPO "/cur");
    tig_file_rmdir(REPO "/base");
    tig_file_rmdir(REPO);
}

void test_mod_format_bounds_suite(void)
{
    erase_fixture_dir();
    mkdir(REPO, 0700);
    mkdir(REPO "/art", 0700);
    mkdir(REPO "/art/Facade", 0700);
    mkdir(REPO "/cur", 0700);
    mkdir(REPO "/base", 0700);
    CHECK(tig_file_repository_add(REPO));

    RUN(a_dat_whose_names_overrun_its_name_table_is_refused);
    RUN(a_dat_whose_name_has_no_terminator_is_refused);
    RUN(a_dat_whose_name_fits_its_name_table_opens);
    RUN(a_facade_grid_the_product_cannot_hold_is_refused);
    RUN(a_small_facade_still_loads);
    RUN(a_negative_blocked_sector_count_is_refused);
    RUN(a_blocked_sector_count_past_the_file_is_refused);
    RUN(a_blocked_sector_list_the_file_holds_loads);
    RUN(a_terrain_a_billion_tiles_wide_is_refused);
    RUN(a_terrain_of_negative_height_is_refused);
    RUN(a_plausible_terrain_header_still_opens);

    CHECK(tig_file_repository_remove(REPO));
    erase_fixture_dir();
}
