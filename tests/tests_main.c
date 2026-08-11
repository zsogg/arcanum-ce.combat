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

int main(void)
{
    test_str_parse_suite();
    test_rect_suite();

    return test_report();
}
