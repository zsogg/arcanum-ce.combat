# Unit tests for CE's pure-logic code.
#
# Off by default so the normal build is untouched; enable with
# -DARCANUM_BUILD_TESTS=ON.
#
# Links the real `tig` and the real game rather than stubbing their
# dependencies, so the code under test is byte-for-byte the shipped code - a
# stub is another thing that can drift.
#
# `arcanum-game` is every game source except `src/main.c` (see the top-level
# CMakeLists). Because it is a STATIC library the linker takes only the objects
# a suite actually reaches, so covering one module does not pull the whole game
# into the test binary. That is what makes game-side suites cheap to add: write
# `test_foo.c`, call its runner from `tests_main.c`, and the module comes in.

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
    "test_random.c"
    "test_stat.c"
    "test_skill.c"
    "test_location.c"
    "test_obj_harness.c"
    "test_obj_file.c"
    "obj_harness.c"
    "obj_harness.h"
    "tests_main.c"
)

target_include_directories(arcanum-tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(arcanum-tests PRIVATE
    arcanum-game
    tig
    SDL3::SDL3
)

add_test(NAME arcanum-tests COMMAND arcanum-tests)

# The CE half of the emulation differential. Driven by
# `arcanum-work/harness/emudiff`, which runs the same functions out of the
# original Arcanum.exe under emulation and compares. Not registered with
# `add_test`: the driver needs the original binary, which is not part of this
# repository.
add_executable(arcanum-emudiff-oracle
    "emudiff_oracle.c"
)

target_link_libraries(arcanum-emudiff-oracle PRIVATE
    arcanum-game
    tig
    SDL3::SDL3
)

