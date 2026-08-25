// Tests for `tig/find_file.c` - the directory-enumeration compat layer.
//
// 121 lines, no suite, and the module where 2026-08-25's one real defect
// actually lived: `tig_find_first_file` dropped SDL's glob result on every
// zero-match glob. It was found by ASan from a `file.c` test, not by anything
// aimed here, which is the argument for this file existing.
//
// **There is no original to be faithful to.** The original calls Win32
// `FindFirstFile`/`FindNextFile`/`FindClose`; this is CE's own re-implementation
// over `SDL_GlobDirectory`, so the question these cases ask is not "does it
// match 0x5XXXXX" but "does it honour the contract its callers rely on". Those
// callers are `tig_file_exists_native` (file.c:1129) and
// `tig_file_list_create_native` (file.c:1013), and the idiom they use is
// pinned below: first, then next until false, then close - with close called
// even when first failed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tig/file.h"
#include "tig/find_file.h"

#include "test_harness.h"

#define FF_DIR "./arcanum_test_findfile"
#define FF_SUB FF_DIR "/sub"

// Files before the directories holding them.
static const char* const FF_PATHS[] = {
    FF_SUB "/nested.txt",
    FF_DIR "/alpha.txt",
    FF_DIR "/Beta.TXT",
    FF_DIR "/gamma.dat",
    FF_SUB,
    FF_DIR,
};

static void ff_remove(void)
{
    size_t index;

    for (index = 0; index < sizeof(FF_PATHS) / sizeof(FF_PATHS[0]); index++) {
        remove(FF_PATHS[index]);
    }
}

static void ff_write(const char* path, const char* text)
{
    FILE* stream = fopen(path, "wb");

    if (stream != NULL) {
        fwrite(text, 1, strlen(text), stream);
        fclose(stream);
    }
}

static void ff_fixture(void)
{
    ff_remove();
    tig_file_mkdir(FF_DIR);
    tig_file_mkdir(FF_SUB);
    ff_write(FF_DIR "/alpha.txt", "alpha");
    ff_write(FF_DIR "/Beta.TXT", "beta!!");
    ff_write(FF_DIR "/gamma.dat", "gam");
    ff_write(FF_SUB "/nested.txt", "nested");
}

// Walk a pattern the way `tig_file_list_create_native` does and return the
// names, comma-separated and sorted, so an assertion does not depend on the
// order the filesystem hands entries back.
static int ff_compare(const void* a, const void* b)
{
    return strcmp((const char*)a, (const char*)b);
}

#define FF_MAX_NAMES 16

static int ff_walk(const char* pattern, char* out, size_t size)
{
    TigFindFileData ffd;
    char names[FF_MAX_NAMES][TIG_MAX_PATH];
    int count = 0;
    int index;

    out[0] = '\0';
    if (tig_find_first_file(pattern, &ffd)) {
        do {
            if (count < FF_MAX_NAMES) {
                strcpy(names[count], ffd.name);
            }
            count++;
            // Bounded on purpose. An iterator that never advances its cursor
            // returns the same entry forever, and an unbounded `do/while` here
            // would hang the whole binary instead of failing one case - a test
            // that cannot fail is worse than no test. Overrunning returns -1,
            // which every caller asserts against.
            if (count > FF_MAX_NAMES) {
                break;
            }
        } while (tig_find_next_file(&ffd));
    }
    tig_find_close(&ffd);

    if (count > FF_MAX_NAMES) {
        return -1;
    }

    qsort(names, (size_t)count, sizeof(names[0]), ff_compare);
    for (index = 0; index < count; index++) {
        if (index != 0) {
            strncat(out, ",", size - strlen(out) - 1);
        }
        strncat(out, names[index], size - strlen(out) - 1);
    }

    return count;
}

// The regression test for the defect this module is here because of. The
// structural half is checkable in a plain build: a failed find must leave no
// handle behind, so nothing is left for `tig_find_close` to free. The leak
// half needs the ASan preset - `LSAN_OPTIONS=suppressions=tests/lsan.supp
// ./out/build/linux-x64-asan/tests/arcanum-tests` - which is exactly how the
// original defect surfaced.
TEST(a_pattern_that_matches_nothing_fails_and_keeps_no_handle)
{
    TigFindFileData ffd;

    ff_fixture();

    memset(&ffd, 0xAB, sizeof(ffd));
    CHECK(!tig_find_first_file(FF_DIR "/nothing-here-*", &ffd));
    CHECK_INT(ffd.handle, 0);
    CHECK_STR(ffd.name, "");

    ff_remove();
}

// The caller idiom at file.c:926 and file.c:1023 - close is called whether or
// not the find succeeded, so it has to tolerate a struct that never held a
// handle. It reports false; it must not crash or free anything.
TEST(closing_after_a_failed_find_is_harmless)
{
    TigFindFileData ffd;

    ff_fixture();

    CHECK(!tig_find_first_file(FF_DIR "/nothing-here-*", &ffd));
    CHECK(!tig_find_close(&ffd));
    CHECK(!tig_find_close(&ffd));

    ff_remove();
}

// The whole point of the module: every matching entry, once each. `.` and `..`
// are not among them - `SDL_GlobDirectory` does not report them, which is why
// the `strcmp(path, ".")` guards in `file.c`'s archive walker are dead code
// under CE and live code under the original's `FindFirstFile`.
TEST(every_matching_entry_is_returned_exactly_once)
{
    char names[512];

    ff_fixture();

    CHECK_INT(ff_walk(FF_DIR "/*", names, sizeof(names)), 4);
    CHECK_STR(names, "Beta.TXT,alpha.txt,gamma.dat,sub");

    ff_remove();
}

// Matching is case-insensitive (`SDL_GLOB_CASEINSENSITIVE`), which is what
// lets the game ask for `art\\tile\\FOO.ART` on a case-sensitive filesystem.
// Both spellings of the extension must find both files.
TEST(matching_ignores_case)
{
    char names[512];

    ff_fixture();

    CHECK_INT(ff_walk(FF_DIR "/*.txt", names, sizeof(names)), 2);
    CHECK_STR(names, "Beta.TXT,alpha.txt");

    CHECK_INT(ff_walk(FF_DIR "/*.TXT", names, sizeof(names)), 2);
    CHECK_STR(names, "Beta.TXT,alpha.txt");

    CHECK_INT(ff_walk(FF_DIR "/BETA.txt", names, sizeof(names)), 1);
    CHECK_STR(names, "Beta.TXT");

    ff_remove();
}

// The name reported is the entry's own, not the path it was found under - the
// callers join it back onto their own directory, so a full path here would
// double it.
TEST(the_name_is_the_entry_not_the_path)
{
    TigFindFileData ffd;

    ff_fixture();

    CHECK(tig_find_first_file(FF_DIR "/gamma.dat", &ffd));
    CHECK_STR(ffd.name, "gamma.dat");
    CHECK(tig_find_close(&ffd));

    ff_remove();
}

// `path_info` is what `tig_file_list_create_native` turns into attributes and
// sizes, so a wrong type here becomes a file the game thinks is a directory.
TEST(path_info_describes_the_entry)
{
    TigFindFileData ffd;

    ff_fixture();

    CHECK(tig_find_first_file(FF_DIR "/Beta.TXT", &ffd));
    CHECK_INT(ffd.path_info.type, SDL_PATHTYPE_FILE);
    CHECK_INT(ffd.path_info.size, 6);
    CHECK(tig_find_close(&ffd));

    CHECK(tig_find_first_file(FF_DIR "/sub", &ffd));
    CHECK_INT(ffd.path_info.type, SDL_PATHTYPE_DIRECTORY);
    CHECK(tig_find_close(&ffd));

    ff_remove();
}

// Iteration ends, and stays ended - a caller that keeps asking gets false
// rather than the first entry again.
TEST(iteration_ends_and_stays_ended)
{
    TigFindFileData ffd;
    int guard;

    ff_fixture();

    CHECK(tig_find_first_file(FF_DIR "/*.dat", &ffd));
    CHECK_STR(ffd.name, "gamma.dat");
    CHECK(!tig_find_next_file(&ffd));
    for (guard = 0; guard < 3; guard++) {
        CHECK(!tig_find_next_file(&ffd));
    }
    CHECK(tig_find_close(&ffd));

    ff_remove();
}

// Close zeroes the struct, so the handle cannot be used twice and a second
// close is a no-op rather than a double free.
TEST(close_clears_the_handle)
{
    TigFindFileData ffd;

    ff_fixture();

    CHECK(tig_find_first_file(FF_DIR "/alpha.txt", &ffd));
    CHECK(ffd.handle != 0);
    CHECK(tig_find_close(&ffd));
    CHECK_INT(ffd.handle, 0);
    CHECK_STR(ffd.name, "");
    CHECK(!tig_find_close(&ffd));
    CHECK(!tig_find_next_file(&ffd));

    ff_remove();
}

// A pattern with no directory separator is looked up in the working
// directory, not rejected - the `dir[0] = '.'` branch. `tig_file_exists`
// reaches it for any bare filename.
TEST(a_bare_pattern_searches_the_working_directory)
{
    TigFindFileData ffd;

    ff_fixture();
    ff_write("./arcanum_test_findfile_bare.txt", "bare");

    CHECK(tig_find_first_file("arcanum_test_findfile_bare.txt", &ffd));
    CHECK_STR(ffd.name, "arcanum_test_findfile_bare.txt");
    CHECK(tig_find_close(&ffd));

    remove("./arcanum_test_findfile_bare.txt");
    ff_remove();
}

// The cursor must advance. This is first in the suite and bounded on purpose:
// `tig_file_list_create_native` walks this iterator with an unbounded
// `do/while` (file.c:1013), so a cursor that stops advancing hangs the engine -
// and hung this binary - rather than returning a wrong answer. A named failure
// here beats a timeout there.
TEST(the_cursor_advances_so_a_walk_terminates)
{
    TigFindFileData ffd;
    char first[TIG_MAX_PATH];
    int steps = 0;

    ff_fixture();

    CHECK(tig_find_first_file(FF_DIR "/*", &ffd));
    strcpy(first, ffd.name);

    while (tig_find_next_file(&ffd) && steps < FF_MAX_NAMES) {
        CHECK(strcmp(ffd.name, first) != 0);
        steps++;
    }
    // Four entries, so three steps after the first. More than that means the
    // walk did not terminate on its own.
    CHECK_INT(steps, 3);
    CHECK(tig_find_close(&ffd));

    ff_remove();
}

void test_find_file_suite(void)
{
    RUN(the_cursor_advances_so_a_walk_terminates);
    RUN(a_pattern_that_matches_nothing_fails_and_keeps_no_handle);
    RUN(closing_after_a_failed_find_is_harmless);
    RUN(every_matching_entry_is_returned_exactly_once);
    RUN(matching_ignores_case);
    RUN(the_name_is_the_entry_not_the_path);
    RUN(path_info_describes_the_entry);
    RUN(iteration_ends_and_stays_ended);
    RUN(close_clears_the_handle);
    RUN(a_bare_pattern_searches_the_working_directory);
}
