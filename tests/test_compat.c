// Tests for the pure string/path half of `tig/compat.c`.
//
// These are CE-original porting shims, not transcribed Troika code, but they
// still contain exactly the kind of off-by-one and separator logic a unit test
// is for. `compat_resolve_path` is deliberately not tested here: it reads the
// real file system and depends on case-insensitive directory enumeration.

#include <string.h>

#include "tig/compat.h"

#include "test_harness.h"

TEST(windows_separators_convert_to_native_separators)
{
    char path[] = "art\\Facade\\facwalk.00";

    compat_windows_path_to_native(path);
#if defined(_WIN32)
    CHECK_STR(path, "art\\Facade\\facwalk.00");
#else
    CHECK_STR(path, "art/Facade/facwalk.00");
#endif
}

TEST(append_path_adds_exactly_one_separator)
{
    char path[64];

    strcpy(path, "art");
    compat_append_path(path, sizeof(path), "Facade");
    CHECK_STR(path, "art" PATH_SEPARATOR_STR "Facade");

    compat_append_path(path, sizeof(path), "facwalk.00");
    CHECK_STR(path, "art" PATH_SEPARATOR_STR "Facade" PATH_SEPARATOR_STR "facwalk.00");
}

TEST(append_path_refuses_to_overflow_the_buffer)
{
    char path[8];

    strcpy(path, "art");
    compat_append_path(path, sizeof(path), "Facade");

    // The original contents survive a refused append.
    CHECK_STR(path, "art");
}

TEST(join_path_ex_builds_the_expected_path_and_stops_at_a_full_buffer)
{
    char path[32];

    compat_join_path(path, sizeof(path), "art", "Facade", "facwalk.00");
    CHECK_STR(path, "art" PATH_SEPARATOR_STR "Facade" PATH_SEPARATOR_STR "facwalk.00");

    compat_join_path(path, sizeof(path), "a", "b", "c", "d", "e", "f", "g");
    CHECK(strlen(path) < sizeof(path));
}

TEST(splitpath_and_makepath_are_inverses_for_a_simple_posix_path)
{
#ifndef _WIN32
    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    char fname[COMPAT_MAX_FNAME];
    char ext[COMPAT_MAX_EXT];
    char rebuilt[COMPAT_MAX_DIR + COMPAT_MAX_FNAME + COMPAT_MAX_EXT + 4];

    compat_splitpath("/usr/local/bin/tool.exe", drive, dir, fname, ext);
    CHECK_STR(drive, "");
    CHECK_STR(dir, "/usr/local/bin/");
    CHECK_STR(fname, "tool");
    CHECK_STR(ext, ".exe");

    compat_makepath(rebuilt, drive, dir, fname, ext);
    CHECK_STR(rebuilt, "/usr/local/bin/tool.exe");
#endif
}

void test_compat_suite(void)
{
    RUN(windows_separators_convert_to_native_separators);
    RUN(append_path_adds_exactly_one_separator);
    RUN(append_path_refuses_to_overflow_the_buffer);
    RUN(join_path_ex_builds_the_expected_path_and_stops_at_a_full_buffer);
    RUN(splitpath_and_makepath_are_inverses_for_a_simple_posix_path);
}
