// Traps T1 (`trap spotting`; zsogg, 2026-10-22; arcanum-work
// `docs/TRAPS.md`): Spot Trap takes Melee's and Bow's curve, a trained
// spotter sees an easy trap without a roll, and a trap object's difficulty
// counts against spotting it. The party's spotting along a path needs a map
// and a player, and is measured in the observatory.

#include "game/feature.h"
#include "game/gamelib.h"
#include "game/obj.h"
#include "game/settings.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/trap.h"
#include "obj_harness.h"
#include "tig/file.h"

#include "test_harness.h"

TEST(a_trained_spotter_sees_an_easy_trap_without_a_roll)
{
    // An Apprentice a non-magical trap, an Expert a magical one too.
    CHECK(!trap_spot_certain(TRAINING_NONE, false, 0));
    CHECK(trap_spot_certain(TRAINING_APPRENTICE, false, 0));
    CHECK(trap_spot_certain(TRAINING_MASTER, false, 0));
    CHECK(!trap_spot_certain(TRAINING_APPRENTICE, true, 0));
    CHECK(trap_spot_certain(TRAINING_EXPERT, true, 0));

    // Only at difficulty 0, the shipped traps' but 56 of 1,998.
    CHECK(!trap_spot_certain(TRAINING_MASTER, false, 10));
    CHECK(!trap_spot_certain(TRAINING_MASTER, true, 50));
}

TEST(spot_trap_takes_the_melee_curve_under_the_switch)
{
    int64_t pc;
    int points;

    settings_init(&settings, "unused");
    feature_init();
    CHECK(obj_harness_init());
    pc = obj_harness_create_proto(OBJ_TYPE_PC);
    CHECK(pc != OBJ_HANDLE_NULL);
    if (pc == OBJ_HANDLE_NULL) {
        settings_exit(&settings);
        return;
    }
    // Perception 20, so the key stat caps nothing.
    stat_base_set(pc, STAT_PERCEPTION, 20);

    for (points = 0; points <= 5; points++) {
        obj_arrayfield_int32_set(pc, OBJ_F_CRITTER_BASIC_SKILL_IDX, BASIC_SKILL_SPOT_TRAP, points);
        CHECK_INT(basic_skill_level(pc, BASIC_SKILL_SPOT_TRAP), 4 * points);

        // Stock: 4 x the level, 16% a point.
        settings_set_value(&settings, feature_key(FEATURE_TRAP_SPOTTING), 0);
        CHECK_INT(basic_skill_effectiveness(pc, BASIC_SKILL_SPOT_TRAP, OBJ_HANDLE_NULL), 16 * points);

        // The switch: 5 x the level + 25, 45% at 1 point, 85% at 3, and
        // the skill's cap of 95 from 4.
        settings_set_value(&settings, feature_key(FEATURE_TRAP_SPOTTING), 1);
        CHECK_INT(basic_skill_effectiveness(pc, BASIC_SKILL_SPOT_TRAP, OBJ_HANDLE_NULL),
            20 * points + 25 < 95 ? 20 * points + 25 : 95);
    }

    settings_set_value(&settings, feature_key(FEATURE_TRAP_SPOTTING), 0);
    obj_harness_destroy(pc);
    // As test_ai_targeting.c does: the harness registers "." as a
    // repository, which the file-listing suites after this one would see.
    obj_harness_exit();
    tig_file_repository_remove(".");
    settings_exit(&settings);
}

TEST(only_a_trap_object_has_a_spotting_difficulty)
{
    int64_t trap;
    int64_t door;

    CHECK(obj_harness_init());
    trap = obj_harness_create_proto(OBJ_TYPE_TRAP);
    door = obj_harness_create_proto(OBJ_TYPE_PORTAL);
    CHECK(trap != OBJ_HANDLE_NULL && door != OBJ_HANDLE_NULL);
    if (trap == OBJ_HANDLE_NULL || door == OBJ_HANDLE_NULL) {
        return;
    }

    obj_field_int32_set(trap, OBJ_F_TRAP_DIFFICULTY, 30);
    CHECK_INT(trap_spot_difficulty(trap), 30);
    // A trapped door or chest carries its trap as a script: no difficulty.
    CHECK_INT(trap_spot_difficulty(door), 0);

    obj_harness_destroy(door);
    obj_harness_destroy(trap);
    obj_harness_exit();
    tig_file_repository_remove(".");
}

TEST(off_or_outside_the_party_the_mover_spots_for_itself)
{
    int64_t npc;
    int64_t trap;

    settings_init(&settings, "unused");
    feature_init();
    CHECK(obj_harness_init());
    npc = obj_harness_create_proto(OBJ_TYPE_NPC);
    trap = obj_harness_create_proto(OBJ_TYPE_TRAP);
    CHECK(npc != OBJ_HANDLE_NULL && trap != OBJ_HANDLE_NULL);
    if (npc == OBJ_HANDLE_NULL || trap == OBJ_HANDLE_NULL) {
        settings_exit(&settings);
        return;
    }

    settings_set_value(&settings, feature_key(FEATURE_TRAP_SPOTTING), 0);
    CHECK(!trap_party_spots_path(npc));
    CHECK(trap_party_spotter(npc, trap) == npc);

    // On, an NPC that follows no player is still its own spotter.
    settings_set_value(&settings, feature_key(FEATURE_TRAP_SPOTTING), 1);
    CHECK(!trap_party_spots_path(npc));
    CHECK(trap_party_spotter(npc, trap) == npc);
    CHECK(!trap_party_spots_path(OBJ_HANDLE_NULL));

    settings_set_value(&settings, feature_key(FEATURE_TRAP_SPOTTING), 0);
    obj_harness_destroy(trap);
    obj_harness_destroy(npc);
    obj_harness_exit();
    tig_file_repository_remove(".");
    settings_exit(&settings);
}

void test_trap_spotting_suite(void)
{
    RUN(a_trained_spotter_sees_an_easy_trap_without_a_roll);
    RUN(spot_trap_takes_the_melee_curve_under_the_switch);
    RUN(only_a_trap_object_has_a_spotting_difficulty);
    RUN(off_or_outside_the_party_the_mover_spots_for_itself);
}
