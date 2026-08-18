// Entry point for the unit tests.
//
// These cover CE's pure-logic code only: no assets, no display, no network, so
// the binary runs anywhere and needs no game install. That is the whole point -
// the existing smoke gate proves the tree builds and reaches `player_create`,
// but it exercises nothing that a rename actually touches.
//
//     cmake --preset linux-x64-debug -DARCANUM_BUILD_TESTS=ON
//     cmake --build --preset linux-x64-debug --target arcanum-tests
//     ./out/build/linux-x64-debug/tests/arcanum-tests
//
// Exit status is 0 when every check passes, 1 otherwise, so it drops straight
// into `verify-build.sh`.

#include "test_harness.h"

void test_str_parse_suite(void);
void test_rect_suite(void);
void test_idxtable_suite(void);
void test_color_suite(void);
void test_bsearch_suite(void);
void test_palette_suite(void);
void test_mes_suite(void);
void test_random_suite(void);
void test_stat_suite(void);
void test_skill_suite(void);
void test_location_suite(void);
void test_obj_harness_suite(void);
void test_obj_file_suite(void);
void test_highres_config_suite(void);
void test_settings_suite(void);
void test_obj_id_suite(void);
void test_reaction_suite(void);
void test_compat_suite(void);
void test_magitech_suite(void);
void test_proto_suite(void);

int main(void)
{
    test_str_parse_suite();
    test_rect_suite();
    test_idxtable_suite();
    test_color_suite();
    test_bsearch_suite();
    test_palette_suite();
    test_mes_suite();
    test_random_suite();
    test_stat_suite();
    test_skill_suite();
    test_location_suite();
    test_obj_harness_suite();
    test_obj_file_suite();
    test_highres_config_suite();
    test_settings_suite();
    test_obj_id_suite();
    test_reaction_suite();
    test_compat_suite();
    test_magitech_suite();
    test_proto_suite();

    return test_report();
}
