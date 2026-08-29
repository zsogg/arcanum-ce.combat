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
    "test_file.c"
    "test_sound.c"
    "test_find_file.c"
    "test_memory.c"
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

# `static` hides several of `art.c`'s most testable functions from the oracle -
# `tig_art_blt_flags_convert`, `tig_art_build_path`,
# `tig_art_header_num_rotations` are pure, are in the blit path, and would not
# link. That was written up as needing "test seams in the engine source" and
# carried as an open decision; it does not.
#
# `static` restricts symbol *binding*, and binding is editable after
# compilation. This takes the object file the real build already produced,
# globalizes exactly the symbols named below, and swaps it into a private copy
# of the archive. **`first_party/tig/src/art.c` is not modified**, and the
# shipped `tig` target is untouched - only this test-only archive differs.
#
# Two things to know before adding a symbol here:
#
#  * `static` also licenses inlining. Under an optimising preset a globalized
#    symbol can exist while the real call sites use an inlined copy, so the
#    oracle would test a function nobody calls. The `VERIFY` step below fails
#    the build if a requested symbol is absent, but it cannot detect that
#    case - prefer the debug preset, and treat a case here as evidence about
#    *this* object file.
#  * Order matters at link time. The globalized object is listed *before*
#    `tig`, so the linker satisfies `art.c`'s symbols from it and never pulls
#    the archive member, which would otherwise duplicate every non-static
#    definition in the file.
set(EMUDIFF_ART_STATICS
    tig_art_blt_flags_convert
    tig_art_build_path
    tig_art_header_num_rotations
    tig_art_cache_entry_blit
)

# Static *data* the oracle has to reach as well - `tig_art_cache_entry_blit`
# reads the cache array rather than taking it as an argument, so the fixture
# has nowhere else to go. Listed apart from the functions because the verify
# step has to look for a different nm class: a function is `T`, a pointer in
# .bss is `B`.
set(EMUDIFF_ART_DATA
    tig_art_cache_entries
)
set(EMUDIFF_ART_OBJ "${CMAKE_CURRENT_BINARY_DIR}/art_globalized.o")
set(_globalize_args "")
foreach(sym ${EMUDIFF_ART_STATICS} ${EMUDIFF_ART_DATA})
    list(APPEND _globalize_args "--globalize-symbol=${sym}")
endforeach()

# Created at configure time: WORKING_DIRECTORY below applies to the *first*
# command too, so it cannot be created by one of them.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/artobj")

add_custom_command(
    OUTPUT "${EMUDIFF_ART_OBJ}"
    COMMAND "${CMAKE_AR}" x "$<TARGET_FILE:tig>" art.c.o
    COMMAND "${CMAKE_OBJCOPY}" ${_globalize_args} art.c.o "${EMUDIFF_ART_OBJ}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f art.c.o
    # VERIFY: every requested symbol must actually be global in the result.
    # Without this, a typo or a renamed function silently produces an object
    # that links but omits the case, and the oracle would report "not
    # registered" - which reads like a missing wrapper, not a missing symbol.
    COMMAND "${CMAKE_COMMAND}"
        "-DOBJ=${EMUDIFF_ART_OBJ}"
        "-DSYMS=${EMUDIFF_ART_STATICS}"
        "-DDATA_SYMS=${EMUDIFF_ART_DATA}"
        "-DNM=${CMAKE_NM}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_globalized.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/artobj"
    DEPENDS tig
    COMMENT "emudiff: globalizing art.c statics (no source change)"
    VERBATIM
)
add_custom_target(arcanum-emudiff-art-obj DEPENDS "${EMUDIFF_ART_OBJ}")

target_link_libraries(arcanum-emudiff-oracle PRIVATE
    "${EMUDIFF_ART_OBJ}"
    arcanum-game
    tig
    SDL3::SDL3
)
add_dependencies(arcanum-emudiff-oracle arcanum-emudiff-art-obj)


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
