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
    "test_art_id.c"
    "test_color.c"
    "test_bsearch.c"
    "test_palette.c"
    "test_mes.c"
    "test_database.c"
    "test_random.c"
    "test_stat.c"
    "test_skill.c"
    "test_location.c"
    "test_obj_harness.c"
    "test_obj_file.c"
    "test_highres_config.c"
    "test_settings.c"
    "test_obj_id.c"
    "test_reaction.c"
    "test_compat.c"
    "test_magitech.c"
    "test_proto.c"
    "test_fatigue.c"
    "test_determinism.c"
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


# The CE half of the archive differential. `arcanum-work/scripts/dbmaker_diff.py`
# compares its output against Troika's own `dbmaker.exe`, which is the only
# reader of this format that did not come from us. Not registered with
# `add_test`: it needs the game's archives.
add_executable(arcanum-dat-dump
    "dat_dump.c"
)

target_link_libraries(arcanum-dat-dump PRIVATE
    tig
    SDL3::SDL3
)

# Resolves paths through the real repository layer, for
# `arcanum-work/scripts/overlay_check.py`. Same reason as `arcanum-dat-dump`:
# the Python model of which archive answers a lookup has never been checked
# against the code it models. Needs the game's archives, so not a test.
add_executable(arcanum-file-probe
    "file_probe.c"
)

target_include_directories(arcanum-file-probe PRIVATE
    ${ZLIB_INCLUDE_DIRS}
)

target_link_libraries(arcanum-file-probe PRIVATE
    tig
    SDL3::SDL3
    ${ZLIB_LIBRARIES}
)
