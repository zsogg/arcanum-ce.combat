# Unit tests for CE's pure-logic code.
#
# Off by default so the normal build is untouched; enable with
# -DARCANUM_BUILD_TESTS=ON.
#
# Links the real `tig` rather than stubbing its dependencies. `str_parse.c`
# needs exactly one external symbol, `tig_debug_printf`, and taking it from the
# library keeps the code under test identical to the shipped code - a stub is
# another thing that can drift.

add_executable(arcanum-tests
    "test_harness.c"
    "test_harness.h"
    "test_str_parse.c"
    "test_rect.c"
    "test_idxtable.c"
    "test_color.c"
    "test_bsearch.c"
    "test_palette.c"
    "test_mes.c"
    "tests_main.c"

    # The first suite to cover game code rather than tig. `mes.c` is the only
    # game module that can be linked on its own - it includes `game/mes.h` and
    # `tig/tig.h` and nothing else - so it comes in as a source rather than
    # needing the game split into a library.
    "${CMAKE_SOURCE_DIR}/src/game/mes.c"
)

target_include_directories(arcanum-tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(arcanum-tests PRIVATE
    tig
    SDL3::SDL3
)

add_test(NAME arcanum-tests COMMAND arcanum-tests)
