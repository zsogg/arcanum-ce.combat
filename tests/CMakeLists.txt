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
    "test_file_cache.c"
    "test_message.c"
    "test_timer.c"
    "test_bmp.c"
    "test_core.c"
    "test_debug.c"
    "test_draw.c"
    "test_kb.c"
    "test_guid.c"
    "test_dialog_shutdown.c"
    "test_dialog_options.c"
    "test_eye_candy_sound.c"
    "test_random.c"
    "test_stat.c"
    "test_skill.c"
    "test_location.c"
    "test_obj_harness.c"
    "test_obj_file.c"
    "test_highres_config.c"
    "test_settings.c"
    "test_feature.c"
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
    # The object fixture. Most of `src/game` takes an `int64_t obj`, which put
    # it outside a differential whose model was "pass these words, read EAX";
    # `obj_harness` builds real prototype objects with no map, sector or art
    # cache under them, so CE's side of such a case can be the engine's own
    # object rather than a mock. See `o_critter_is_monstrous`.
    "obj_harness.c"
    "obj_harness.h"
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

# The same objcopy trick again, for `combat.c`, so the differential can reach
# the two functions in it that an object-field fixture can drive.
#
# `combat.c` was the emptiest module in the suite -- one case for 89
# definitions -- because 41 of those take an object handle and the differential
# had no way to pass one. The 2026-08-31 object fixture removed that, and
# `scripts/obj_case_candidates.py` then found exactly three combat functions
# whose call closure reaches nothing but the field accessors. Two of the three
# are `static`, so without this they stay unreachable however drivable they are.
#
# `combat.c` is not modified. This takes the object file the real build already
# produced and globalizes three names in a private copy, exactly as for `art.c`,
# `dialog.c` and `anim.c` above.
set(EMUDIFF_COMBAT_STATICS
    combat_weapon_loudness
    combat_turn_based_critter_is_dontdraw
)
set(EMUDIFF_COMBAT_OBJ "${CMAKE_CURRENT_BINARY_DIR}/combat_globalized.o")
set(_combat_globalize_args "")
foreach(sym ${EMUDIFF_COMBAT_STATICS})
    list(APPEND _combat_globalize_args "--globalize-symbol=${sym}")
endforeach()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/combatobj")

add_custom_command(
    OUTPUT "${EMUDIFF_COMBAT_OBJ}"
    COMMAND "${CMAKE_AR}" x "$<TARGET_FILE:arcanum-game>" combat.c.o
    COMMAND "${CMAKE_OBJCOPY}" ${_combat_globalize_args} combat.c.o "${EMUDIFF_COMBAT_OBJ}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f combat.c.o
    COMMAND "${CMAKE_COMMAND}"
        "-DOBJ=${EMUDIFF_COMBAT_OBJ}"
        "-DSYMS=${EMUDIFF_COMBAT_STATICS}"
        "-DDATA_SYMS="
        "-DNM=${CMAKE_NM}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_globalized.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/combatobj"
    DEPENDS arcanum-game
    COMMENT "emudiff: globalizing combat.c statics (no source change)"
    VERBATIM
)
add_custom_target(arcanum-emudiff-combat-obj DEPENDS "${EMUDIFF_COMBAT_OBJ}")

# The same objcopy trick again, for `object.c` and `magictech.c` (card V3).
#
# One static function in each is a candidate the object-field fixture can
# drive, and only one: `object_hp_from_pts` is the tier-2 half of the hit-point
# trio, and `magictech_disallowed_flags_check` is the widest thing in
# `magictech.c` that reaches nothing but the field accessors. Neither source
# file is modified.
set(EMUDIFF_OBJECT_STATICS
    object_hp_from_pts
)
set(EMUDIFF_OBJECT_OBJ "${CMAKE_CURRENT_BINARY_DIR}/object_globalized.o")
set(_object_globalize_args "")
foreach(sym ${EMUDIFF_OBJECT_STATICS})
    list(APPEND _object_globalize_args "--globalize-symbol=${sym}")
endforeach()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/objectobj")

add_custom_command(
    OUTPUT "${EMUDIFF_OBJECT_OBJ}"
    COMMAND "${CMAKE_AR}" x "$<TARGET_FILE:arcanum-game>" object.c.o
    COMMAND "${CMAKE_OBJCOPY}" ${_object_globalize_args} object.c.o "${EMUDIFF_OBJECT_OBJ}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f object.c.o
    COMMAND "${CMAKE_COMMAND}"
        "-DOBJ=${EMUDIFF_OBJECT_OBJ}"
        "-DSYMS=${EMUDIFF_OBJECT_STATICS}"
        "-DDATA_SYMS="
        "-DNM=${CMAKE_NM}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_globalized.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/objectobj"
    DEPENDS arcanum-game
    COMMENT "emudiff: globalizing object.c statics (no source change)"
    VERBATIM
)
add_custom_target(arcanum-emudiff-object-obj DEPENDS "${EMUDIFF_OBJECT_OBJ}")

set(EMUDIFF_MAGICTECH_STATICS
    magictech_disallowed_flags_check
)
set(EMUDIFF_MAGICTECH_OBJ "${CMAKE_CURRENT_BINARY_DIR}/magictech_globalized.o")
set(_magictech_globalize_args "")
foreach(sym ${EMUDIFF_MAGICTECH_STATICS})
    list(APPEND _magictech_globalize_args "--globalize-symbol=${sym}")
endforeach()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/magictechobj")

add_custom_command(
    OUTPUT "${EMUDIFF_MAGICTECH_OBJ}"
    COMMAND "${CMAKE_AR}" x "$<TARGET_FILE:arcanum-game>" magictech.c.o
    COMMAND "${CMAKE_OBJCOPY}" ${_magictech_globalize_args} magictech.c.o "${EMUDIFF_MAGICTECH_OBJ}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f magictech.c.o
    COMMAND "${CMAKE_COMMAND}"
        "-DOBJ=${EMUDIFF_MAGICTECH_OBJ}"
        "-DSYMS=${EMUDIFF_MAGICTECH_STATICS}"
        "-DDATA_SYMS="
        "-DNM=${CMAKE_NM}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_globalized.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/magictechobj"
    DEPENDS arcanum-game
    COMMENT "emudiff: globalizing magictech.c statics (no source change)"
    VERBATIM
)
add_custom_target(arcanum-emudiff-magictech-obj DEPENDS "${EMUDIFF_MAGICTECH_OBJ}")

# The same objcopy trick for `reaction.c` and `skill_ui.c` (card V4, batch 2).
# One static candidate in each, and both are bare field reads once the guard is
# passed - the two cheapest things left in the candidate tail.
#
# `skill_ui.c` is in `src/ui` rather than `src/game`, which the card flagged as
# something to check before copying this pattern. There is nothing to check:
# `src/ui/*.c` is in `${SRCS}` and therefore in `libarcanum-game.a` - `ar t`
# lists `skill_ui.c.o` - so the archive member comes from the same place every
# other block here pulls from.
set(EMUDIFF_REACTION_STATICS
    reaction_get_base
)
set(EMUDIFF_REACTION_OBJ "${CMAKE_CURRENT_BINARY_DIR}/reaction_globalized.o")
set(_reaction_globalize_args "")
foreach(sym ${EMUDIFF_REACTION_STATICS})
    list(APPEND _reaction_globalize_args "--globalize-symbol=${sym}")
endforeach()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/reactionobj")

add_custom_command(
    OUTPUT "${EMUDIFF_REACTION_OBJ}"
    COMMAND "${CMAKE_AR}" x "$<TARGET_FILE:arcanum-game>" reaction.c.o
    COMMAND "${CMAKE_OBJCOPY}" ${_reaction_globalize_args} reaction.c.o "${EMUDIFF_REACTION_OBJ}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f reaction.c.o
    COMMAND "${CMAKE_COMMAND}"
        "-DOBJ=${EMUDIFF_REACTION_OBJ}"
        "-DSYMS=${EMUDIFF_REACTION_STATICS}"
        "-DDATA_SYMS="
        "-DNM=${CMAKE_NM}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_globalized.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/reactionobj"
    DEPENDS arcanum-game
    COMMENT "emudiff: globalizing reaction.c statics (no source change)"
    VERBATIM
)
add_custom_target(arcanum-emudiff-reaction-obj DEPENDS "${EMUDIFF_REACTION_OBJ}")

set(EMUDIFF_SKILL_UI_STATICS
    skill_ui_is_pickpocket_immune
)
set(EMUDIFF_SKILL_UI_OBJ "${CMAKE_CURRENT_BINARY_DIR}/skill_ui_globalized.o")
set(_skill_ui_globalize_args "")
foreach(sym ${EMUDIFF_SKILL_UI_STATICS})
    list(APPEND _skill_ui_globalize_args "--globalize-symbol=${sym}")
endforeach()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skilluiobj")

add_custom_command(
    OUTPUT "${EMUDIFF_SKILL_UI_OBJ}"
    COMMAND "${CMAKE_AR}" x "$<TARGET_FILE:arcanum-game>" skill_ui.c.o
    COMMAND "${CMAKE_OBJCOPY}" ${_skill_ui_globalize_args} skill_ui.c.o "${EMUDIFF_SKILL_UI_OBJ}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f skill_ui.c.o
    COMMAND "${CMAKE_COMMAND}"
        "-DOBJ=${EMUDIFF_SKILL_UI_OBJ}"
        "-DSYMS=${EMUDIFF_SKILL_UI_STATICS}"
        "-DDATA_SYMS="
        "-DNM=${CMAKE_NM}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_globalized.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/skilluiobj"
    DEPENDS arcanum-game
    COMMENT "emudiff: globalizing skill_ui.c statics (no source change)"
    VERBATIM
)
add_custom_target(arcanum-emudiff-skill-ui-obj DEPENDS "${EMUDIFF_SKILL_UI_OBJ}")

# `target.c` (card V4, batch 2). The two list-append functions are `static`,
# which `nm` said and the header did not - both are declared nowhere, so
# `-Werror=implicit-function-declaration` is what turned it up rather than a
# link error, which is the guard doing exactly its job.
set(EMUDIFF_TARGET_STATICS
    target_list_add_obj_unless_off
    target_list_add_obj_unless_destroyed
)
set(EMUDIFF_TARGET_OBJ "${CMAKE_CURRENT_BINARY_DIR}/target_globalized.o")
set(_target_globalize_args "")
foreach(sym ${EMUDIFF_TARGET_STATICS})
    list(APPEND _target_globalize_args "--globalize-symbol=${sym}")
endforeach()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/targetobj")

add_custom_command(
    OUTPUT "${EMUDIFF_TARGET_OBJ}"
    COMMAND "${CMAKE_AR}" x "$<TARGET_FILE:arcanum-game>" target.c.o
    COMMAND "${CMAKE_OBJCOPY}" ${_target_globalize_args} target.c.o "${EMUDIFF_TARGET_OBJ}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f target.c.o
    COMMAND "${CMAKE_COMMAND}"
        "-DOBJ=${EMUDIFF_TARGET_OBJ}"
        "-DSYMS=${EMUDIFF_TARGET_STATICS}"
        "-DDATA_SYMS="
        "-DNM=${CMAKE_NM}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_globalized.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/targetobj"
    DEPENDS arcanum-game
    COMMENT "emudiff: globalizing target.c statics (no source change)"
    VERBATIM
)
add_custom_target(arcanum-emudiff-target-obj DEPENDS "${EMUDIFF_TARGET_OBJ}")

# An implicitly declared function defaults to returning `int`, and on x86-64 a
# `bool` return only sets AL - the rest of EAX keeps whatever the callee last
# computed. A wrapper that calls a bool-returning engine function without its
# header therefore reads a plausible-looking number instead of 0 or 1, and the
# differential reports it as a mismatch against the original.
#
# That is not hypothetical: on 2026-09-01 `fate_is_activated` came back as
# 1025 where the original said 1024, which reads exactly like a real finding.
# It was a missing `#include "game/fate.h"`, and seven other wrappers in the
# same batch had the same defect without showing a symptom, because their
# functions happen to return `int`.
#
# Promoted to an error here rather than left as a warning: a warning in a
# 2,700-line file that builds clean otherwise is a warning nobody reads.
target_compile_options(arcanum-emudiff-oracle PRIVATE
    -Werror=implicit-function-declaration
)

# `mt_ai.c` (card V4, batch 3). `mt_ai_item_allowed_for_ai` (0x4CCA30) is
# `static` - the producer prints it as `sub_4CCA30` because the symbol ledger
# has no name for it, not because CE has none. Two field reads and one art-id
# call, plus a `MagicTechAi*` of which the function reads exactly one member.
set(EMUDIFF_MT_AI_STATICS
    mt_ai_item_allowed_for_ai
)
set(EMUDIFF_MT_AI_OBJ "${CMAKE_CURRENT_BINARY_DIR}/mt_ai_globalized.o")
set(_mt_ai_globalize_args "")
foreach(sym ${EMUDIFF_MT_AI_STATICS})
    list(APPEND _mt_ai_globalize_args "--globalize-symbol=${sym}")
endforeach()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/mtaiobj")

add_custom_command(
    OUTPUT "${EMUDIFF_MT_AI_OBJ}"
    COMMAND "${CMAKE_AR}" x "$<TARGET_FILE:arcanum-game>" mt_ai.c.o
    COMMAND "${CMAKE_OBJCOPY}" ${_mt_ai_globalize_args} mt_ai.c.o "${EMUDIFF_MT_AI_OBJ}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f mt_ai.c.o
    COMMAND "${CMAKE_COMMAND}"
        "-DOBJ=${EMUDIFF_MT_AI_OBJ}"
        "-DSYMS=${EMUDIFF_MT_AI_STATICS}"
        "-DDATA_SYMS="
        "-DNM=${CMAKE_NM}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_globalized.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/mtaiobj"
    DEPENDS arcanum-game
    COMMENT "emudiff: globalizing mt_ai.c statics (no source change)"
    VERBATIM
)
add_custom_target(arcanum-emudiff-mt-ai-obj DEPENDS "${EMUDIFF_MT_AI_OBJ}")

# `anim.c`, moved here from below on 2026-09-04 (card V4, batch 3). It was
# written for `arcanum-tests` alone; `anim_is_portal` makes the oracle a second
# consumer, and a globalized object has to be listed BEFORE `arcanum-game` or
# the linker pulls `anim.c.o` out of the archive and every non-static
# definition in the file is defined twice. That ordering is why the block moved
# rather than being referenced where it stood: `target_link_libraries` appends,
# so a second call could only have put it after the archive. One object serves
# both targets - making a second from the same archive member would be two
# copies of a 15,000-line translation unit that could drift apart.
# The same objcopy trick again, for `anim.c` - porting bug #33.
#
# The bug is three defects across three `static` functions, and its entire
# symptom is audible, so the ledger carried it from 2026-08-22 as "verified
# against the original's bytes but not by ear" and `PLAYTEST-REMAINING.md`
# asked a human to go and listen.
#
# It does not need ears. Every one of the three things a listener was told to
# judge - does it loop, does it follow, does it restart - is a decision these
# functions make, and each is observable from outside: which branch `begin`
# takes, whether `tig_sound_set_position` is driven from the object, and
# whether `restart` acts when there is no live handle.
#
# Globalizing costs no source change and no runtime behaviour, exactly as for
# `art.c` and `dialog.c` above. `anim.c` is 15,000 lines and these three are
# 90 of them; nothing else in the file is touched.
set(ANIM_TEST_STATICS
    anim_is_portal
    anim_eye_candy_sound_begin
    anim_eye_candy_sound_move
    anim_eye_candy_sound_restart
)
set(ANIM_TEST_OBJ "${CMAKE_CURRENT_BINARY_DIR}/anim_globalized.o")
set(_anim_globalize_args "")
foreach(sym ${ANIM_TEST_STATICS})
    list(APPEND _anim_globalize_args "--globalize-symbol=${sym}")
endforeach()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/animobj")

add_custom_command(
    OUTPUT "${ANIM_TEST_OBJ}"
    COMMAND "${CMAKE_AR}" x "$<TARGET_FILE:arcanum-game>" anim.c.o
    COMMAND "${CMAKE_OBJCOPY}" ${_anim_globalize_args} anim.c.o "${ANIM_TEST_OBJ}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f anim.c.o
    COMMAND "${CMAKE_COMMAND}"
        "-DOBJ=${ANIM_TEST_OBJ}"
        "-DSYMS=${ANIM_TEST_STATICS}"
        "-DDATA_SYMS="
        "-DNM=${CMAKE_NM}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_globalized.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/animobj"
    DEPENDS arcanum-game
    COMMENT "tests: globalizing anim.c eye-candy sound statics (no source change)"
    VERBATIM
)

add_custom_target(arcanum-anim-test-obj DEPENDS "${ANIM_TEST_OBJ}")


target_link_libraries(arcanum-emudiff-oracle PRIVATE
    "${EMUDIFF_ART_OBJ}"
    "${EMUDIFF_COMBAT_OBJ}"
    "${EMUDIFF_OBJECT_OBJ}"
    "${EMUDIFF_MAGICTECH_OBJ}"
    "${EMUDIFF_REACTION_OBJ}"
    "${EMUDIFF_SKILL_UI_OBJ}"
    "${EMUDIFF_TARGET_OBJ}"
    "${EMUDIFF_MT_AI_OBJ}"
    "${ANIM_TEST_OBJ}"
    arcanum-game
    tig
    SDL3::SDL3
)
add_dependencies(arcanum-emudiff-oracle
    arcanum-emudiff-reaction-obj
    arcanum-emudiff-skill-ui-obj
    arcanum-emudiff-target-obj
    arcanum-emudiff-art-obj
    arcanum-emudiff-combat-obj
    arcanum-emudiff-object-obj
    arcanum-emudiff-magictech-obj
    arcanum-emudiff-mt-ai-obj
    arcanum-anim-test-obj
)


# The same objcopy trick as above, for `dialog.c`, so the unit suite can reach
# `dialog_collect_pc_options`.
#
# It is `static`, and it is the function the 2026-08-30 parent-link change
# touches - the whole point of that change is a second collection pass, so
# shipping it with the pass untested would be shipping the interesting half on
# faith. Globalizing costs no source change and no runtime behaviour: the object
# is swapped in ahead of `arcanum-game`, exactly as `art_globalized.o` is ahead
# of the oracle's link.
set(DIALOG_TEST_STATICS
    dialog_collect_pc_options
)
set(DIALOG_TEST_OBJ "${CMAKE_CURRENT_BINARY_DIR}/dialog_globalized.o")
set(_dialog_globalize_args "")
foreach(sym ${DIALOG_TEST_STATICS})
    list(APPEND _dialog_globalize_args "--globalize-symbol=${sym}")
endforeach()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dialogobj")

add_custom_command(
    OUTPUT "${DIALOG_TEST_OBJ}"
    COMMAND "${CMAKE_AR}" x "$<TARGET_FILE:arcanum-game>" dialog.c.o
    COMMAND "${CMAKE_OBJCOPY}" ${_dialog_globalize_args} dialog.c.o "${DIALOG_TEST_OBJ}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f dialog.c.o
    COMMAND "${CMAKE_COMMAND}"
        "-DOBJ=${DIALOG_TEST_OBJ}"
        "-DSYMS=${DIALOG_TEST_STATICS}"
        "-DDATA_SYMS="
        "-DNM=${CMAKE_NM}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_globalized.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dialogobj"
    DEPENDS arcanum-game
    COMMENT "tests: globalizing dialog.c statics (no source change)"
    VERBATIM
)

add_custom_target(arcanum-dialog-test-obj DEPENDS "${DIALOG_TEST_OBJ}")
target_link_libraries(arcanum-tests PRIVATE "${DIALOG_TEST_OBJ}")
add_dependencies(arcanum-tests arcanum-dialog-test-obj)


target_link_libraries(arcanum-tests PRIVATE "${ANIM_TEST_OBJ}")
add_dependencies(arcanum-tests arcanum-anim-test-obj)


# The CE half of the archive differential. `arcanum-work/scripts/verify_dbmaker.py`
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
# The fuzz probe runs CE's own data-file readers over one file and exits, so
# a malformed file can be pointed at the real reader without booting the
# game. Links arcanum-game because the readers live there, not in tig.
add_executable(arcanum-fuzz-probe
    "fuzz_probe.c"
)

target_link_libraries(arcanum-fuzz-probe PRIVATE
    arcanum-game
    tig
    SDL3::SDL3
    ${ZLIB_LIBRARIES}
)

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
