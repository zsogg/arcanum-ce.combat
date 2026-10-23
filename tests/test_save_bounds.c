// The save-stream loaders refuse counts, indexes and lengths the arrays they
// fill cannot hold (review 2026-10-22, findings 2 and 3).
//
// Every case feeds a crafted stream to the real loader through the public
// entry point and asserts two things: the loader answers false, and the
// stream position shows it stopped AT the refused field rather than reading
// through it. The second assertion is what makes each case red against the
// previous code, which either returned true (a negative extent that skipped
// past the array), read on into memory it did not own (a goal index of 40,
// a time-event type of -5, a 64 KB archive name), or both.
//
// Nothing here is a save the game can write: each array is the same size in
// the original, so a count past it never comes out of `*_save`. The save
// format is Compatible (FIDELITY-CONTRACT.md): CE reads everything the
// original writes, and refusing what it cannot have written changes nothing
// for a real save.
#include <stdio.h>
#include <string.h>

#include "game/anim.h"
#include "game/anim_private.h"
#include "game/gamelib.h"
#include "game/magictech.h"
#include "game/sector.h"
#include "game/timeevent.h"
#include "test_harness.h"
#include "tig/file.h"
#include "tig/video.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#endif

#define REPO "./arcanum_test_save_bounds"
#define FIXTURE REPO "/stream.bin"

// -- a byte builder ----------------------------------------------------------

static unsigned char bytes[70000];
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

static void put_fill(unsigned char value, size_t count)
{
    memset(bytes + bytes_len, value, count);
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

// Runs `load` over the fixture and reports its answer and where it stopped.
static bool load_fixture(bool (*load)(GameLoadInfo*), int* position)
{
    GameLoadInfo load_info;
    TigFile* stream;
    bool ok;

    stream = tig_file_fopen(FIXTURE, "rb");
    CHECK(stream != NULL);
    if (stream == NULL) {
        return false;
    }
    memset(&load_info, 0, sizeof(load_info));
    load_info.stream = stream;
    ok = load(&load_info);
    *position = tig_file_ftell(stream);
    tig_file_fclose(stream);
    return ok;
}

// -- anim --------------------------------------------------------------------

// `anim_load` ends by walking every slot and asking the object pool about
// the handle of each active one. Earlier suites leave handles in the static
// table after tearing their pool down, so each case starts from an empty
// table: nothing is active, nothing is asked.
static void clear_anim_table(void)
{
    memset(anim_run_info, 0, sizeof(anim_run_info));
}

// `anim_load` reads eighteen 4-byte globals before its count.
#define ANIM_HEADER_SIZE (18 * 4)

TEST(an_anim_count_past_the_array_is_refused_before_anything_is_read)
{
    int position = 0;

    clear_anim_table();
    put_zeros(ANIM_HEADER_SIZE);
    put_int(ANIM_RUN_INFO_COUNT + 1);
    put_int(1);
    write_fixture(FIXTURE);

    CHECK(!load_fixture(anim_load, &position));
    CHECK_INT(position, ANIM_HEADER_SIZE + 4);
}

TEST(an_anim_extent_that_skips_past_the_array_is_refused)
{
    int position = 0;

    clear_anim_table();
    // The previous loader accepted this: `idx += 1000` ended the loop and it
    // returned true for a stream describing slots that do not exist.
    put_zeros(ANIM_HEADER_SIZE);
    put_int(ANIM_RUN_INFO_COUNT);
    put_int(-1000);
    write_fixture(FIXTURE);

    CHECK(!load_fixture(anim_load, &position));
    CHECK_INT(position, ANIM_HEADER_SIZE + 8);
}

TEST(an_empty_anim_block_still_loads)
{
    int position = 0;

    clear_anim_table();
    put_zeros(ANIM_HEADER_SIZE);
    put_int(0);
    write_fixture(FIXTURE);

    CHECK(load_fixture(anim_load, &position));
    CHECK_INT(position, ANIM_HEADER_SIZE + 4);
}

// One run record up to and including its goal index: six ints, a safe object
// handle (ObjectID, location, map), the extra target tile, the index.
#define ANIM_RECORD_TO_GOAL (6 * 4 + 24 + 8 + 4 + 8 + 4)

TEST(a_goal_index_past_the_stack_is_refused_at_the_index)
{
    int position = 0;

    clear_anim_table();
    put_zeros(ANIM_HEADER_SIZE);
    put_int(1);
    put_int(1);
    put_zeros(ANIM_RECORD_TO_GOAL - 4);
    put_int(40);
    // Bytes the previous loader read as thirty-two more goals, past the
    // eight-entry stack and into the rest of the record and its neighbours.
    put_zeros(4096);
    write_fixture(FIXTURE);

    CHECK(!load_fixture(anim_load, &position));
    CHECK_INT(position, ANIM_HEADER_SIZE + 8 + ANIM_RECORD_TO_GOAL);
}

// -- magictech ---------------------------------------------------------------

TEST(a_magictech_extent_that_skips_past_the_array_is_refused)
{
    int position = 0;

    put_int(0); // active run count
    put_int(512);
    put_int(-1000);
    write_fixture(FIXTURE);

    CHECK(!load_fixture(magictech_post_load, &position));
    CHECK_INT(position, 12);
}

TEST(a_magictech_count_past_the_array_is_refused_before_anything_is_read)
{
    int position = 0;

    put_int(0);
    put_int(513);
    put_int(1);
    write_fixture(FIXTURE);

    CHECK(!load_fixture(magictech_post_load, &position));
    CHECK_INT(position, 8);
}

// -- time events -------------------------------------------------------------

// `timeevent_load` reads three clocks, then a count per time type.
#define TIMEEVENT_HEADER_SIZE (3 * (int)sizeof(DateTime))

TEST(a_negative_time_event_type_is_refused_at_the_type)
{
    int position = 0;

    put_zeros(TIMEEVENT_HEADER_SIZE);
    put_int(1); // one event in the first time type
    put_zeros(sizeof(DateTime));
    put_int(-5);
    // Bytes the previous loader read as parameters chosen by an out-of-range
    // table entry.
    put_zeros(256);
    write_fixture(FIXTURE);

    CHECK(!load_fixture(timeevent_load, &position));
    CHECK_INT(position, TIMEEVENT_HEADER_SIZE + 4 + (int)sizeof(DateTime) + 4);
}

TEST(a_time_event_type_at_the_count_is_refused_at_the_type)
{
    int position = 0;

    put_zeros(TIMEEVENT_HEADER_SIZE);
    put_int(1);
    put_zeros(sizeof(DateTime));
    put_int(TIMEEVENT_TYPE_COUNT);
    put_zeros(256);
    write_fixture(FIXTURE);

    CHECK(!load_fixture(timeevent_load, &position));
    CHECK_INT(position, TIMEEVENT_HEADER_SIZE + 4 + (int)sizeof(DateTime) + 4);
}

// -- the save list's .gsi ----------------------------------------------------

// `gamelib_saveinfo_load` loads the slot's thumbnail first, through the
// renderer; the suite has none, so the load is answered here.
static bool fake_thumbnail;

extern int __real_tig_video_buffer_load_from_bmp(const char* filename, TigVideoBuffer** video_buffer_ptr, unsigned int flags);

int __wrap_tig_video_buffer_load_from_bmp(const char* filename, TigVideoBuffer** video_buffer_ptr, unsigned int flags)
{
    if (fake_thumbnail) {
        *video_buffer_ptr = NULL;
        return TIG_OK;
    }
    return __real_tig_video_buffer_load_from_bmp(filename, video_buffer_ptr, flags);
}

static void put_string(const char* str)
{
    put_int((int)strlen(str));
    put(str, strlen(str));
}

// A .gsi for Slot0001 whose two unbounded strings are `pc_size` and
// `description_size` bytes long on paper.
static void write_gsi(int pc_size, const char* pc_name, int description_size, const char* description)
{
    put_int(1); // version
    put_string("Arcanum");
    put_int(pc_size);
    put(pc_name, strlen(pc_name));
    put_int(0); // map
    put_zeros(sizeof(DateTime));
    put_int(0); // portrait
    put_int(1); // level
    put_zeros(8); // location
    put_int(0); // story state
    put_int(description_size);
    put(description, strlen(description));
    write_fixture(REPO "/Save/Slot0001.gsi");
}

TEST(a_well_formed_gsi_loads)
{
    GameSaveInfo info;

    memset(&info, 0, sizeof(info));
    write_gsi(6, "Player", 4, "desc");
    fake_thumbnail = true;
    CHECK(gamelib_saveinfo_load("Slot0001", &info));
    fake_thumbnail = false;
    CHECK_STR(info.module_name, "Arcanum");
    CHECK_STR(info.pc_name, "Player");
    CHECK_STR(info.description, "desc");
}

TEST(a_gsi_with_an_oversized_player_name_is_refused)
{
    GameSaveInfo info;

    memset(&info, 0, sizeof(info));
    // 4000 bytes on paper, 4000 bytes on disk: the previous reader took them
    // all into pc_name[24].
    put_int(1);
    put_string("Arcanum");
    put_int(4000);
    put_fill('A', 4000);
    write_fixture(REPO "/Save/Slot0001.gsi");
    fake_thumbnail = true;
    CHECK(!gamelib_saveinfo_load("Slot0001", &info));
    fake_thumbnail = false;
}

TEST(a_gsi_with_an_oversized_description_is_refused)
{
    GameSaveInfo info;

    memset(&info, 0, sizeof(info));
    write_gsi(6, "Player", 300, "");
    put_fill('D', 300);
    bytes_len = 0;
    // Rewrite with the 300 bytes present on disk.
    put_int(1);
    put_string("Arcanum");
    put_string("Player");
    put_int(0);
    put_zeros(sizeof(DateTime));
    put_int(0);
    put_int(1);
    put_zeros(8);
    put_int(0);
    put_int(300);
    put_fill('D', 300);
    write_fixture(REPO "/Save/Slot0001.gsi");
    fake_thumbnail = true;
    CHECK(!gamelib_saveinfo_load("Slot0001", &info));
    fake_thumbnail = false;
}

// -- the archive index -------------------------------------------------------

TEST(an_archive_entry_name_longer_than_a_path_is_refused)
{
    // The previous reader read all 64 KB of it onto a 260-byte stack buffer.
    put_int(0); // a file record
    put_int(0x10000);
    put_fill('N', 0x10000);
    write_fixture(REPO "/Save/Bad.tfai");
    write_fixture(REPO "/Save/Bad.tfaf");

    CHECK(!tig_file_unarchive("save\\Bad", "unpacked"));
}

TEST(an_archive_directory_name_longer_than_a_path_is_refused)
{
    put_int(1); // a directory record
    put_int(-1);
    write_fixture(REPO "/Save/Bad.tfai");
    write_fixture(REPO "/Save/Bad.tfaf");

    CHECK(!tig_file_unarchive("save\\Bad", "unpacked"));
}

// -- suite -------------------------------------------------------------------

static void erase_fixture_dir(void)
{
    remove(FIXTURE);
    remove(REPO "/Save/Slot0001.gsi");
    remove(REPO "/Save/Bad.tfai");
    remove(REPO "/Save/Bad.tfaf");
    tig_file_rmdir(REPO "/unpacked");
    tig_file_rmdir(REPO "/Save");
    tig_file_rmdir(REPO);
}

void test_save_bounds_suite(void)
{
    erase_fixture_dir();
    mkdir(REPO, 0700);
    mkdir(REPO "/Save", 0700);
    mkdir(REPO "/unpacked", 0700);
    CHECK(tig_file_repository_add(REPO));

    RUN(an_anim_count_past_the_array_is_refused_before_anything_is_read);
    RUN(an_anim_extent_that_skips_past_the_array_is_refused);
    RUN(an_empty_anim_block_still_loads);
    RUN(a_goal_index_past_the_stack_is_refused_at_the_index);
    RUN(a_magictech_extent_that_skips_past_the_array_is_refused);
    RUN(a_magictech_count_past_the_array_is_refused_before_anything_is_read);
    RUN(a_negative_time_event_type_is_refused_at_the_type);
    RUN(a_time_event_type_at_the_count_is_refused_at_the_type);
    RUN(a_well_formed_gsi_loads);
    RUN(a_gsi_with_an_oversized_player_name_is_refused);
    RUN(a_gsi_with_an_oversized_description_is_refused);
    RUN(an_archive_entry_name_longer_than_a_path_is_refused);
    RUN(an_archive_directory_name_longer_than_a_path_is_refused);

    CHECK(tig_file_repository_remove(REPO));
    erase_fixture_dir();
}
