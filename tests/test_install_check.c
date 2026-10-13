// Release 1, R5 (2026-10-13): `arcanum-ce -check-install` - the pure part.
// The report and the fix are exercised on a prepared game folder.

#include "game/install_check.h"

#include "test_harness.h"

TEST(a_main_archive_is_arcanum_star_dat_in_any_case)
{
    CHECK(install_check_is_main_archive("arcanum1.dat"));
    CHECK(install_check_is_main_archive("Arcanum5.dat"));
    CHECK(install_check_is_main_archive("ArcanumXRaces.dat"));
    CHECK(install_check_is_main_archive("ARCANUMZHIGHRES.DAT"));
    CHECK(!install_check_is_main_archive("tig.dat"));
    CHECK(!install_check_is_main_archive("Arcanum.exe"));
    // The game's own pattern, `arcanum*.dat`, matches this in the game folder
    // too (the module archive lives in `modules/`, where nothing globs it).
    CHECK(install_check_is_main_archive("arcanum.dat"));
    CHECK(!install_check_is_main_archive("arcanum1.dat.bak"));
    CHECK(!install_check_is_main_archive(NULL));
}

void test_install_check_suite(void)
{
    RUN(a_main_archive_is_arcanum_star_dat_in_any_case);
}
