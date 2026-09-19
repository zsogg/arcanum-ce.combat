// Tests for `game/mes.c`.
//
// The first suite over game code rather than tig, and `mes.c` is the natural
// bridge: it is the only game module that links on its own (it includes
// `game/mes.h` and `tig/tig.h` and nothing else), and it is the real caller of
// `tig_bsearch`, whose insertion-point-on-miss contract the tig suite pins in
// isolation. Here it is exercised through `mes_find_next`, which is what
// actually depends on it.
//
// Every string the game shows a player comes out of a .mes file - dialogue,
// item names, skill descriptions, UI labels - so the parser's edge cases are
// player-visible by definition. One of them is a known live data bug: asking
// for a number that is not in the file yields the string "Error! Missing line
// N in <path>" rather than a failure, and the game prints it. That mechanism is
// tested below.
//
// Fixtures are written to disk because the parser reads a stream, but they are
// created and removed by the tests themselves, so the suite still needs no game
// assets. Paths start with "./" deliberately: `tig_file_fopen` only falls
// through to a plain `fopen` when the path starts with '.', '/', '\\' or has
// ':' second, and otherwise looks the name up in the (empty, in a test) archive
// repository list.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/mes.h"

#include "test_harness.h"

#define FIXTURE_PATH "./arcanum_test_fixture.mes"
#define FIXTURE_PATH_B "./arcanum_test_fixture_b.mes"

static bool write_fixture(const char* path, const char* contents)
{
    FILE* stream = fopen(path, "wb");

    if (stream == NULL) {
        return false;
    }

    fwrite(contents, 1, strlen(contents), stream);
    fclose(stream);

    return true;
}

static void remove_fixture(const char* path)
{
    remove(path);
}

// The shape of a real .mes file: braced number, braced text, comments between.
static const char* const SAMPLE = "// a comment line\n"
                                  "{100}{first}\n"
                                  "{300}{third}\n"
                                  "{200}{second}\n"
                                  "\n"
                                  "{400}{fourth}\n";

TEST(load_reads_every_entry)
{
    mes_file_handle_t handle = MES_FILE_HANDLE_INVALID;

    CHECK(write_fixture(FIXTURE_PATH, SAMPLE));
    CHECK(mes_load(FIXTURE_PATH, &handle));
    CHECK(handle != MES_FILE_HANDLE_INVALID);

    CHECK_INT(mes_entries_count(handle), 4);

    mes_unload(handle);
    remove_fixture(FIXTURE_PATH);
}

TEST(entries_are_sorted_by_number_not_file_order)
{
    // The file lists 100, 300, 200, 400. Every lookup is a binary search, so
    // the load has to sort - if it ever stopped, lookups would silently miss.
    mes_file_handle_t handle = MES_FILE_HANDLE_INVALID;
    MesFileEntry entry;

    CHECK(write_fixture(FIXTURE_PATH, SAMPLE));
    CHECK(mes_load(FIXTURE_PATH, &handle));

    CHECK(mes_get_entry(handle, 0, &entry));
    CHECK_INT(entry.num, 100);
    CHECK(mes_get_entry(handle, 1, &entry));
    CHECK_INT(entry.num, 200);
    CHECK(mes_get_entry(handle, 2, &entry));
    CHECK_INT(entry.num, 300);
    CHECK(mes_get_entry(handle, 3, &entry));
    CHECK_INT(entry.num, 400);
    CHECK_STR(entry.str, "fourth");

    mes_unload(handle);
    remove_fixture(FIXTURE_PATH);
}

TEST(get_entry_rejects_out_of_range_indices)
{
    mes_file_handle_t handle = MES_FILE_HANDLE_INVALID;
    MesFileEntry entry;

    CHECK(write_fixture(FIXTURE_PATH, SAMPLE));
    CHECK(mes_load(FIXTURE_PATH, &handle));

    CHECK(!mes_get_entry(handle, -1, &entry));
    CHECK(!mes_get_entry(handle, 4, &entry));
    CHECK(mes_get_entry(handle, 3, &entry));

    mes_unload(handle);
    remove_fixture(FIXTURE_PATH);
}

TEST(search_finds_a_number_and_reports_a_miss)
{
    mes_file_handle_t handle = MES_FILE_HANDLE_INVALID;
    MesFileEntry entry;

    CHECK(write_fixture(FIXTURE_PATH, SAMPLE));
    CHECK(mes_load(FIXTURE_PATH, &handle));

    entry.num = 200;
    CHECK(mes_search(handle, &entry));
    CHECK_STR(entry.str, "second");

    entry.num = 250;
    CHECK(!mes_search(handle, &entry));

    mes_unload(handle);
    remove_fixture(FIXTURE_PATH);
}

TEST(get_msg_returns_an_error_string_instead_of_failing)
{
    // The mechanism behind the known live data bug: `mes_get_msg` has no way to
    // report a miss, so it hands back "Error! Missing line N in <path>" and the
    // caller prints it. Thanking a female technologist can show exactly this
    // for line 1155, in vanilla and UAP alike. Pinned so the behaviour is not
    // "tidied" into something that hides the missing line.
    mes_file_handle_t handle = MES_FILE_HANDLE_INVALID;
    MesFileEntry entry;

    CHECK(write_fixture(FIXTURE_PATH, SAMPLE));
    CHECK(mes_load(FIXTURE_PATH, &handle));

    entry.num = 100;
    mes_get_msg(handle, &entry);
    CHECK_STR(entry.str, "first");

    entry.num = 999;
    entry.str = NULL;
    mes_get_msg(handle, &entry);
    CHECK(entry.str != NULL);
    CHECK(strstr(entry.str, "Error! Missing line 999") != NULL);
    // The number stays as asked, so a caller cannot tell from `num` that the
    // lookup failed - only by reading the string it is about to display.
    CHECK_INT(entry.num, 999);

    mes_unload(handle);
    remove_fixture(FIXTURE_PATH);
}

TEST(find_first_and_find_next_walk_every_entry_in_order)
{
    mes_file_handle_t handle = MES_FILE_HANDLE_INVALID;
    MesFileEntry entry;
    int seen = 0;
    int last = -1;
    bool ordered = true;

    CHECK(write_fixture(FIXTURE_PATH, SAMPLE));
    CHECK(mes_load(FIXTURE_PATH, &handle));

    if (mes_find_first(handle, &entry)) {
        do {
            if (entry.num <= last) {
                ordered = false;
            }
            last = entry.num;
            seen++;
        } while (mes_find_next(handle, &entry));
    }

    CHECK_INT(seen, 4);
    CHECK(ordered);
    // Iteration terminates rather than running off the end - the range check in
    // `mes_find_next` is what catches `tig_bsearch` returning one past the end.
    CHECK_INT(last, 400);

    mes_unload(handle);
    remove_fixture(FIXTURE_PATH);
}

TEST(find_next_from_an_absent_number_lands_on_the_next_one_up)
{
    // This is `tig_bsearch`'s insertion-point contract reaching its real
    // caller: 250 is not in the file, so the search returns where it would go
    // and `mes_find_next` does *not* step past it, yielding 300.
    mes_file_handle_t handle = MES_FILE_HANDLE_INVALID;
    MesFileEntry entry;

    CHECK(write_fixture(FIXTURE_PATH, SAMPLE));
    CHECK(mes_load(FIXTURE_PATH, &handle));

    entry.num = 250;
    CHECK(mes_find_next(handle, &entry));
    CHECK_INT(entry.num, 300);
    CHECK_STR(entry.str, "third");

    // From a present number it does step past.
    entry.num = 300;
    CHECK(mes_find_next(handle, &entry));
    CHECK_INT(entry.num, 400);

    // And past the last entry there is nothing.
    entry.num = 400;
    CHECK(!mes_find_next(handle, &entry));

    entry.num = 5000;
    CHECK(!mes_find_next(handle, &entry));

    mes_unload(handle);
    remove_fixture(FIXTURE_PATH);
}

TEST(count_in_range_needs_its_start_number_to_exist)
{
    // A sharp edge worth pinning. The count is inclusive of both ends *when the
    // start number is present*, but the implementation locates `start` with an
    // exact-match `bsearch` and returns 0 when it is absent - even though
    // entries plainly fall inside the range. Callers passing a round number
    // like 100 get a count only because that entry happens to exist.
    mes_file_handle_t handle = MES_FILE_HANDLE_INVALID;

    CHECK(write_fixture(FIXTURE_PATH, SAMPLE));
    CHECK(mes_load(FIXTURE_PATH, &handle));

    CHECK_INT(mes_entries_count_in_range(handle, 100, 400), 4);
    CHECK_INT(mes_entries_count_in_range(handle, 200, 300), 2);
    CHECK_INT(mes_entries_count_in_range(handle, 100, 100), 1);

    // 150 is absent, so this reports 0 despite 200, 300 and 400 being in range.
    CHECK_INT(mes_entries_count_in_range(handle, 150, 400), 0);

    mes_unload(handle);
    remove_fixture(FIXTURE_PATH);
}

TEST(loading_the_same_path_twice_shares_one_file)
{
    // Refcounted by path: the second load returns the same handle, and the
    // entries stay alive until the second unload.
    mes_file_handle_t first = MES_FILE_HANDLE_INVALID;
    mes_file_handle_t second = MES_FILE_HANDLE_INVALID;
    MesFileEntry entry;

    CHECK(write_fixture(FIXTURE_PATH, SAMPLE));
    CHECK(mes_load(FIXTURE_PATH, &first));
    CHECK(mes_load(FIXTURE_PATH, &second));

    CHECK_INT(first, second);
    CHECK_INT(mes_entries_count(first), 4);

    mes_unload(second);

    // Still loaded after one unload.
    entry.num = 100;
    CHECK(mes_search(first, &entry));
    CHECK_STR(entry.str, "first");

    mes_unload(first);
    remove_fixture(FIXTURE_PATH);
}

TEST(parses_multi_line_text_and_skips_comments)
{
    // Newlines inside braces are part of the message, and anything outside
    // braces is skipped, so a comment between entries must not become text.
    mes_file_handle_t handle = MES_FILE_HANDLE_INVALID;
    MesFileEntry entry;

    CHECK(write_fixture(FIXTURE_PATH,
        "// leading comment\n"
        "{1}{line one\nline two}\n"
        "// another comment\n"
        "{2}{plain}\n"));
    CHECK(mes_load(FIXTURE_PATH, &handle));

    CHECK_INT(mes_entries_count(handle), 2);

    entry.num = 1;
    CHECK(mes_search(handle, &entry));
    CHECK_STR(entry.str, "line one\nline two");

    entry.num = 2;
    CHECK(mes_search(handle, &entry));
    CHECK_STR(entry.str, "plain");

    mes_unload(handle);
    remove_fixture(FIXTURE_PATH);
}

TEST(a_file_with_no_entries_fails_to_load)
{
    // Surprising, and worth pinning: a file that parses to zero entries is
    // reported as a *failed* load, handle set to invalid - so a comment-only
    // .mes is indistinguishable from one that is not there at all. Every
    // accessor indexes `mes_files[handle]` without validating, so a caller that
    // ignores the return value reads `mes_files[-1]`. Written the other way
    // round first, and ASan caught exactly that.
    mes_file_handle_t handle = 12345;

    CHECK(write_fixture(FIXTURE_PATH, "// nothing but a comment\n"));
    CHECK(!mes_load(FIXTURE_PATH, &handle));
    CHECK_INT(handle, MES_FILE_HANDLE_INVALID);

    remove_fixture(FIXTURE_PATH);
}

TEST(a_missing_file_fails_to_load)
{
    mes_file_handle_t handle = MES_FILE_HANDLE_INVALID;

    remove_fixture("./arcanum_test_no_such_file.mes");
    CHECK(!mes_load("./arcanum_test_no_such_file.mes", &handle));
}

TEST(merge_combines_two_files_and_re_sorts)
{
    // `mes_merge` reallocates the destination's string arena and then fixes up
    // every `str` pointer by the amount the arena moved. If that arithmetic is
    // wrong the strings read as garbage rather than crashing, so the check is
    // on the text, not just the numbers.
    mes_file_handle_t dst = MES_FILE_HANDLE_INVALID;
    mes_file_handle_t src = MES_FILE_HANDLE_INVALID;
    MesFileEntry entry;

    CHECK(write_fixture(FIXTURE_PATH, "{10}{ten}\n{30}{thirty}\n"));
    CHECK(write_fixture(FIXTURE_PATH_B, "{20}{twenty}\n{40}{forty}\n"));

    CHECK(mes_load(FIXTURE_PATH, &dst));
    CHECK(mes_load(FIXTURE_PATH_B, &src));

    mes_merge(dst, src);

    CHECK_INT(mes_entries_count(dst), 4);

    entry.num = 10;
    CHECK(mes_search(dst, &entry));
    CHECK_STR(entry.str, "ten");

    entry.num = 20;
    CHECK(mes_search(dst, &entry));
    CHECK_STR(entry.str, "twenty");

    entry.num = 30;
    CHECK(mes_search(dst, &entry));
    CHECK_STR(entry.str, "thirty");

    entry.num = 40;
    CHECK(mes_search(dst, &entry));
    CHECK_STR(entry.str, "forty");

    // Sorted after the merge, since every lookup is a binary search.
    CHECK(mes_get_entry(dst, 0, &entry));
    CHECK_INT(entry.num, 10);
    CHECK(mes_get_entry(dst, 3, &entry));
    CHECK_INT(entry.num, 40);

    mes_unload(src);
    mes_unload(dst);
    remove_fixture(FIXTURE_PATH);
    remove_fixture(FIXTURE_PATH_B);
}

TEST(an_over_long_message_is_truncated_without_overflowing)
{
    // Regression test for porting bug #11.
    //
    // `parse_field` stores a character only while `pos < MAX_STRING - 1`, so
    // the terminator it writes afterwards lands on the last byte of the buffer
    // at worst. CE originally bounded at `pos < MAX_STRING`, which let `pos`
    // reach 2000 and wrote `buffer[2000]` on a `char buffer[2000]` - one byte
    // past the end of a stack array, and then `strcpy`'d 2001 bytes into
    // another 2000-byte one. The exe bounds at 1999 (`cmp esi, 0x7cf` at
    // 0x4D4BF2), so this was CE's own off-by-one, not Troika's.
    //
    // Reachable from data alone: any .mes message of 2000 characters or more.
    // The shipped corpus does not quite get there - the longest braced field in
    // all 731 vanilla and UAP .mes files is 1996 characters, in gamebook.mes -
    // so this was four characters of headroom away from being live, and any mod
    // or localisation with a longer entry crosses it. Under the ASan preset
    // this test fails loudly if the bound regresses; without it the corruption
    // is silent.
    mes_file_handle_t handle = MES_FILE_HANDLE_INVALID;
    MesFileEntry entry;
    char* contents;
    const int long_len = 2500;
    int i;

    contents = (char*)malloc(long_len + 64);
    CHECK(contents != NULL);
    if (contents == NULL) {
        return;
    }

    strcpy(contents, "{1}{");
    for (i = 0; i < long_len; i++) {
        contents[4 + i] = 'x';
    }
    strcpy(contents + 4 + long_len, "}\n");

    CHECK(write_fixture(FIXTURE_PATH, contents));
    CHECK(mes_load(FIXTURE_PATH, &handle));

    entry.num = 1;
    CHECK(mes_search(handle, &entry));
    // Truncated to fit, and still a valid C string.
    CHECK_INT((int)strlen(entry.str), MAX_STRING - 1);

    mes_unload(handle);
    remove_fixture(FIXTURE_PATH);
    free(contents);
}

void test_mes_suite(void)
{
    RUN(load_reads_every_entry);
    RUN(entries_are_sorted_by_number_not_file_order);
    RUN(get_entry_rejects_out_of_range_indices);
    RUN(search_finds_a_number_and_reports_a_miss);
    RUN(get_msg_returns_an_error_string_instead_of_failing);
    RUN(find_first_and_find_next_walk_every_entry_in_order);
    RUN(find_next_from_an_absent_number_lands_on_the_next_one_up);
    RUN(count_in_range_needs_its_start_number_to_exist);
    RUN(loading_the_same_path_twice_shares_one_file);
    RUN(parses_multi_line_text_and_skips_comments);
    RUN(a_file_with_no_entries_fails_to_load);
    RUN(a_missing_file_fails_to_load);
    RUN(merge_combines_two_files_and_re_sorts);
    RUN(an_over_long_message_is_truncated_without_overflowing);
}
