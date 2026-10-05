// Combat AI milestone AI-3 (`ai targeting`): when an NPC that is already
// fighting turns to a new attacker. Pure, so the rule zsogg chose on
// 2026-10-04 - commit to a nearby opponent, turn only for an adjacent or a
// clearly nearer attacker - is pinned before it is seen in a fight.

#include "game/ai.h"
#include "game/feature.h"
#include "game/gamelib.h"
#include "game/obj.h"
#include "game/obj_flags.h"
#include "game/settings.h"
#include "obj_harness.h"
#include "tig/file.h"

#include "test_harness.h"

TEST(an_adjacent_attacker_draws_an_npc_whose_target_is_not)
{
    CHECK(ai_targeting_prefers_new_attacker(2, 1));
    CHECK(ai_targeting_prefers_new_attacker(15, 1));
}

TEST(an_adjacent_target_is_kept_against_another_adjacent_attacker)
{
    CHECK(!ai_targeting_prefers_new_attacker(1, 1));
    CHECK(!ai_targeting_prefers_new_attacker(0, 1));
}

TEST(a_clearly_nearer_attacker_draws_it)
{
    // At least three tiles nearer and at most half the distance.
    CHECK(ai_targeting_prefers_new_attacker(6, 3));
    CHECK(ai_targeting_prefers_new_attacker(10, 5));
    CHECK(ai_targeting_prefers_new_attacker(20, 2));
}

TEST(a_slightly_nearer_attacker_does_not)
{
    CHECK(!ai_targeting_prefers_new_attacker(5, 3)); // two nearer
    CHECK(!ai_targeting_prefers_new_attacker(10, 6)); // four nearer, over half
    CHECK(!ai_targeting_prefers_new_attacker(4, 4));
}

TEST(a_farther_attacker_never_does)
{
    CHECK(!ai_targeting_prefers_new_attacker(1, 5));
    CHECK(!ai_targeting_prefers_new_attacker(3, 8));
}

TEST(a_hunter_entry_is_read_by_description)
{
    ai_roles_reset();
    CHECK_INT(ai_roles_apply_entry("Hunter: 17085"), AI_FATIGUE_ENTRY_APPLIED);
    CHECK_INT(ai_roles_apply_entry("  Hunter: 27336"), AI_FATIGUE_ENTRY_APPLIED);
    CHECK(ai_roles_is_hunter_description(17085));
    CHECK(ai_roles_is_hunter_description(27336));
    CHECK(!ai_roles_is_hunter_description(17086));
    ai_roles_reset();
    CHECK(!ai_roles_is_hunter_description(17085));
}

TEST(a_malformed_hunter_entry_says_so_and_adds_nothing)
{
    ai_roles_reset();
    // The key carries its colon, as the fatigue file's do; the first build
    // matched "Hunter" alone and read every entry as ": 17085".
    CHECK_INT(ai_roles_apply_entry("Hunter: thief"), AI_FATIGUE_ENTRY_BAD_VALUE);
    CHECK_INT(ai_roles_apply_entry("Hunter 17085"), AI_FATIGUE_ENTRY_UNKNOWN);
    CHECK_INT(ai_roles_apply_entry("Hunters: 17085"), AI_FATIGUE_ENTRY_UNKNOWN);
    CHECK(!ai_roles_is_hunter_description(17085));
}

// Milestones AI-3 and AI-4: the follower leash is the Defensive stance. A
// follower of the player reaches 8 tiles under `ai follower leash`, 20 once
// told to Spread Out (Aggressive) or when led by an NPC, and 20 with the
// switch off. The switch is put back as it was found: the settings are shared.
TEST(the_leash_is_the_defensive_stance)
{
    int64_t pc;
    int64_t npc_leader;
    int64_t follower;

    settings_init(&settings, "unused");
    feature_init();
    CHECK(obj_harness_init());
    pc = obj_harness_create_proto(OBJ_TYPE_PC);
    npc_leader = obj_harness_create_proto(OBJ_TYPE_NPC);
    follower = obj_harness_create_proto(OBJ_TYPE_NPC);
    obj_field_int32_set(follower, OBJ_F_NPC_FLAGS, 0);

    settings_set_value(&settings, feature_key(FEATURE_AI_FOLLOWER_LEASH), 0);
    CHECK_INT(ai_follower_reach(follower, pc), 20);

    settings_set_value(&settings, feature_key(FEATURE_AI_FOLLOWER_LEASH), 1);
    CHECK_INT(ai_follower_reach(follower, pc), AI_FOLLOWER_LEASH);
    CHECK_INT(ai_follower_reach(follower, npc_leader), 20);

    obj_field_int32_set(follower, OBJ_F_NPC_FLAGS, ONF_AI_SPREAD_OUT);
    CHECK_INT(ai_follower_reach(follower, pc), 20);

    settings_set_value(&settings, feature_key(FEATURE_AI_FOLLOWER_LEASH), 0);
    obj_harness_destroy(follower);
    obj_harness_destroy(npc_leader);
    obj_harness_destroy(pc);
    // As test_combat_observer.c does: the harness registers "." as a
    // repository, and the file-listing suites that run later enumerate
    // repositories and would see its rules/effect.mes fixture.
    obj_harness_exit();
    tig_file_repository_remove(".");
    settings_exit(&settings);
}

void test_ai_targeting_suite(void)
{
    RUN(an_adjacent_attacker_draws_an_npc_whose_target_is_not);
    RUN(an_adjacent_target_is_kept_against_another_adjacent_attacker);
    RUN(a_clearly_nearer_attacker_draws_it);
    RUN(a_slightly_nearer_attacker_does_not);
    RUN(a_farther_attacker_never_does);
    RUN(a_hunter_entry_is_read_by_description);
    RUN(a_malformed_hunter_entry_says_so_and_adds_nothing);
    RUN(the_leash_is_the_defensive_stance);
}
