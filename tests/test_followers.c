// The Followers mod (card X9; zsogg, 2026-10-19): `follower alignment free`,
// `follower level free` and `follower reaction joins`, against the real
// `ai_check_follow`, which decides both joining and staying. Its sibling
// `ai_check_leader` (the warnings) is `static` and outside this binary's
// reach; its one change, an early return, is checked in a game. The AI packets come from a small
// `rules/ai_params.mes` read by the real `ai_init`; packet 1 carries the
// follow limits of the shipped packet 4, the strictest: reaction 20,
// alignment 20 either way, 2 levels.
#include <stdio.h>

#include "game/ai.h"
#include "game/critter.h"
#include "game/feature.h"
#include "game/gamelib.h"
#include "game/magictech.h"
#include "game/obj.h"
#include "game/obj_flags.h"
#include "game/reaction.h"
#include "game/settings.h"
#include "game/skill.h"
#include "game/stat.h"
#include "obj_harness.h"
#include "test_harness.h"
#include "tig/file.h"

#define FOLLOWERS_PACKETS "rules/ai_params.mes"
#define STRICT_PACKET 1

static int64_t followers_pc;
static int64_t followers_npc;

static void followers_switch(int feature, bool on)
{
    settings_set_value(&settings, feature_key(feature), on ? 1 : 0);
}

static void followers_reaction(int target)
{
    reaction_adj_ex(followers_npc, followers_pc, target - reaction_get(followers_npc, followers_pc), false);
}

static bool followers_begin(void)
{
    FILE* stream;

    settings_init(&settings, "unused");
    feature_init();
    if (!obj_harness_init()) {
        return false;
    }
    tig_file_mkdir("rules");
    stream = fopen(FOLLOWERS_PACKETS, "wb");
    if (stream == NULL) {
        return false;
    }
    // The generic packet, then one with packet 4's follow limits (D6-D9).
    fputs("{0}{0 1000 1000 20 6 20 1001 1000 2 0 0 2001 100 50 20 1 1}\n"
          "{1}{0 1000 1000 20 6 20 20 20 2 0 0 2001 100 50 20 1 1}\n",
        stream);
    fclose(stream);
    if (!ai_init(NULL)) {
        return false;
    }

    followers_pc = obj_harness_create_proto(OBJ_TYPE_PC);
    followers_npc = obj_harness_create_proto(OBJ_TYPE_NPC);
    obj_field_int32_set(followers_npc, OBJ_F_NPC_AI_DATA, STRICT_PACKET);
    obj_field_int32_set(followers_npc, OBJ_F_NPC_FLAGS, 0);
    stat_base_set(followers_pc, STAT_ALIGNMENT, 0);
    stat_base_set(followers_npc, STAT_ALIGNMENT, 0);
    stat_base_set(followers_pc, STAT_LEVEL, 5);
    stat_base_set(followers_npc, STAT_LEVEL, 5);
    followers_reaction(50);
    return true;
}

static void followers_end(void)
{
    followers_switch(FEATURE_FOLLOWER_ALIGNMENT_FREE, false);
    followers_switch(FEATURE_FOLLOWER_LEVEL_FREE, false);
    followers_switch(FEATURE_FOLLOWER_REACTION_JOINS, false);
    obj_harness_destroy(followers_npc);
    obj_harness_destroy(followers_pc);
    remove(FOLLOWERS_PACKETS);
    // As test_ai_targeting.c does: later suites enumerate repositories.
    obj_harness_exit();
    tig_file_repository_remove(".");
    settings_exit(&settings);
}

TEST(the_strict_packet_lets_a_matching_npc_join)
{
    CHECK(followers_begin());
    CHECK_INT(reaction_get(followers_npc, followers_pc), 50);
    CHECK_INT(stat_level_get(followers_npc, STAT_LEVEL), 5);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, true), AI_FOLLOW_OK);
    followers_end();
}

TEST(alignment_refuses_stock_and_not_under_follower_alignment_free)
{
    CHECK(followers_begin());
    stat_base_set(followers_npc, STAT_ALIGNMENT, 50);
    stat_base_set(followers_pc, STAT_ALIGNMENT, -50);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, true), AI_FOLLOW_TOO_BAD);
    followers_switch(FEATURE_FOLLOWER_ALIGNMENT_FREE, true);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, true), AI_FOLLOW_OK);

    stat_base_set(followers_npc, STAT_ALIGNMENT, -60);
    stat_base_set(followers_pc, STAT_ALIGNMENT, 90);
    followers_switch(FEATURE_FOLLOWER_ALIGNMENT_FREE, false);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, true), AI_FOLLOW_TOO_GOOD);
    followers_switch(FEATURE_FOLLOWER_ALIGNMENT_FREE, true);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, true), AI_FOLLOW_OK);
    followers_end();
}

TEST(an_alignment_mismatch_no_longer_makes_a_follower_leave)
{
    // The idle re-check that makes a follower leave is the same call.
    CHECK(followers_begin());
    critter_leader_set(followers_npc, followers_pc);
    stat_base_set(followers_npc, STAT_ALIGNMENT, 80);
    stat_base_set(followers_pc, STAT_ALIGNMENT, -80);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, true), AI_FOLLOW_TOO_BAD);
    followers_switch(FEATURE_FOLLOWER_ALIGNMENT_FREE, true);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, true), AI_FOLLOW_OK);
    followers_end();
}

TEST(a_higher_level_npc_refuses_stock_and_joins_under_follower_level_free)
{
    CHECK(followers_begin());
    stat_base_set(followers_npc, STAT_LEVEL, 12);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, true), AI_FOLLOW_LOW_LEVEL);
    followers_switch(FEATURE_FOLLOWER_LEVEL_FREE, true);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, true), AI_FOLLOW_OK);
    // And a follower that outgrows the player no longer leaves.
    critter_leader_set(followers_npc, followers_pc);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, true), AI_FOLLOW_OK);
    followers_end();
}

TEST(low_reaction_no_longer_stops_joining_under_follower_reaction_joins)
{
    CHECK(followers_begin());
    followers_reaction(10);
    CHECK_INT(reaction_get(followers_npc, followers_pc), 10);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, true), AI_FOLLOW_DISLIKE);
    followers_switch(FEATURE_FOLLOWER_REACTION_JOINS, true);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, true), AI_FOLLOW_OK);
    followers_end();
}

TEST(a_follower_whose_reaction_collapses_still_leaves)
{
    CHECK(followers_begin());
    critter_leader_set(followers_npc, followers_pc);
    followers_reaction(10);
    followers_switch(FEATURE_FOLLOWER_ALIGNMENT_FREE, true);
    followers_switch(FEATURE_FOLLOWER_LEVEL_FREE, true);
    followers_switch(FEATURE_FOLLOWER_REACTION_JOINS, true);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, true), AI_FOLLOW_DISLIKE);
    followers_end();
}

TEST(the_charisma_follower_limit_stays)
{
    int64_t existing;

    // Charisma 1 allows one follower (STAT_MAX_FOLLOWERS is Charisma / 4,
    // never below 1); with one already in the party the next is refused,
    // whatever the three switches say.
    CHECK(followers_begin());
    existing = obj_harness_create_proto(OBJ_TYPE_NPC);
    obj_field_int32_set(existing, OBJ_F_NPC_FLAGS, 0);
    stat_base_set(followers_pc, STAT_CHARISMA, 1);
    CHECK_INT(stat_level_get(followers_pc, STAT_MAX_FOLLOWERS), 1);
    followers_switch(FEATURE_FOLLOWER_ALIGNMENT_FREE, true);
    followers_switch(FEATURE_FOLLOWER_LEVEL_FREE, true);
    followers_switch(FEATURE_FOLLOWER_REACTION_JOINS, true);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, false), AI_FOLLOW_OK);
    obj_arrayfield_obj_set(followers_pc, OBJ_F_CRITTER_FOLLOWER_IDX, 0, existing);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, false), AI_FOLLOW_LIMIT_REACHED);
    obj_harness_destroy(existing);
    followers_end();
}

// Porting bug #82: a Master persuader skips the reaction, alignment, level and
// rival-leader tests, but the original still applies the Charisma cap
// (0x4AD9C9 `jge 0x4ADAF7`). CE returned AI_FOLLOW_OK before reaching it.
TEST(a_master_persuader_is_still_held_to_the_charisma_limit)
{
    int64_t existing;

    CHECK(followers_begin());
    existing = obj_harness_create_proto(OBJ_TYPE_NPC);
    obj_field_int32_set(existing, OBJ_F_NPC_FLAGS, 0);
    // Master training (tier 3) written directly: the setter enforces one tier
    // at a time. Mastery also adds a follower, so Charisma 1 allows one.
    obj_arrayfield_int32_set(followers_pc, OBJ_F_CRITTER_BASIC_SKILL_IDX,
        BASIC_SKILL_PERSUATION, TRAINING_MASTER << 6);
    stat_base_set(followers_pc, STAT_CHARISMA, 1);
    CHECK_INT(basic_skill_training_get(followers_pc, BASIC_SKILL_PERSUATION), TRAINING_MASTER);
    CHECK_INT(stat_level_get(followers_pc, STAT_MAX_FOLLOWERS), 1);

    // A strict packet's refusals are skipped for a Master...
    stat_base_set(followers_npc, STAT_ALIGNMENT, 80);
    stat_base_set(followers_pc, STAT_ALIGNMENT, -80);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, false), AI_FOLLOW_OK);

    // ...the cap is not.
    obj_arrayfield_obj_set(followers_pc, OBJ_F_CRITTER_FOLLOWER_IDX, 0, existing);
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, false), AI_FOLLOW_LIMIT_REACHED);
    // And the caller that waives it still may.
    CHECK_INT(ai_check_follow(followers_npc, followers_pc, true), AI_FOLLOW_OK);

    obj_harness_destroy(existing);
    followers_end();
}

// Porting bug #85 and the `heal living patients` switch. Stock follows the
// original: an undead or mechanical HEALER cannot use Heal, and a living one
// may heal anyone (0x4AE6C5-0x4AE6DC reads the healer's flags). The switch is
// the rule CE had before: anyone may heal, but not an undead or mechanical
// patient.
TEST(heal_refuses_by_the_healer_stock_and_by_the_patient_under_the_switch)
{
    int64_t kit;
    int64_t robot;

    CHECK(followers_begin());
    kit = obj_harness_create_proto(OBJ_TYPE_GENERIC);
    robot = obj_harness_create_proto(OBJ_TYPE_NPC);
    obj_field_int32_set(kit, OBJ_F_GENERIC_FLAGS,
        obj_field_int32_get(kit, OBJ_F_GENERIC_FLAGS) | OGF_IS_HEALING_ITEM);
    obj_field_int32_set(robot, OBJ_F_CRITTER_FLAGS,
        obj_field_int32_get(robot, OBJ_F_CRITTER_FLAGS) | OCF_MECHANICAL);

    // A living healer and a mechanical patient.
    CHECK_INT(ai_check_use_skill(followers_npc, robot, kit, SKILL_HEAL), AI_USE_SKILL_OK);
    // A mechanical healer and a living patient.
    CHECK_INT(ai_check_use_skill(robot, followers_pc, kit, SKILL_HEAL), AI_USE_SKILL_BAD_TARGET);

    followers_switch(FEATURE_HEAL_LIVING_PATIENTS, true);
    CHECK_INT(ai_check_use_skill(followers_npc, robot, kit, SKILL_HEAL), AI_USE_SKILL_BAD_TARGET);
    CHECK_INT(ai_check_use_skill(robot, followers_pc, kit, SKILL_HEAL), AI_USE_SKILL_OK);
    followers_switch(FEATURE_HEAL_LIVING_PATIENTS, false);

    obj_harness_destroy(robot);
    obj_harness_destroy(kit);
    followers_end();
}

// Follower AI survey, finding 1 (zsogg, 2026-10-28): under `grenade blasts`
// a spell or blast hit within the player's party is registered as accidental,
// so a follower caught in the player's grenade does not turn on them and
// leave. Stock flags every spell victim as deliberately attacked.
TEST(a_party_hit_by_a_spell_or_blast_is_accidental_under_grenade_blasts)
{
    int64_t second;
    int64_t stranger;

    CHECK(followers_begin());
    second = obj_harness_create_proto(OBJ_TYPE_NPC);
    stranger = obj_harness_create_proto(OBJ_TYPE_NPC);
    obj_field_int32_set(second, OBJ_F_NPC_FLAGS, 0);
    obj_field_int32_set(stranger, OBJ_F_NPC_FLAGS, 0);
    critter_leader_set(followers_npc, followers_pc);
    critter_leader_set(second, followers_pc);

    CHECK_INT(magictech_attack_flags(followers_pc, followers_npc), 0);
    CHECK_INT(magictech_attack_flags(followers_npc, followers_pc), 0);

    followers_switch(FEATURE_GRENADE_BLASTS, true);
    CHECK_INT(magictech_attack_flags(followers_pc, followers_npc), 0x01);
    CHECK_INT(magictech_attack_flags(followers_npc, followers_pc), 0x01);
    CHECK_INT(magictech_attack_flags(followers_npc, second), 0x01);
    CHECK_INT(magictech_attack_flags(followers_pc, stranger), 0);
    CHECK_INT(magictech_attack_flags(stranger, followers_npc), 0);
    followers_switch(FEATURE_GRENADE_BLASTS, false);

    critter_leader_set(second, OBJ_HANDLE_NULL);
    obj_harness_destroy(stranger);
    obj_harness_destroy(second);
    followers_end();
}

void test_followers_suite(void)
{
    RUN(the_strict_packet_lets_a_matching_npc_join);
    RUN(alignment_refuses_stock_and_not_under_follower_alignment_free);
    RUN(an_alignment_mismatch_no_longer_makes_a_follower_leave);
    RUN(a_higher_level_npc_refuses_stock_and_joins_under_follower_level_free);
    RUN(low_reaction_no_longer_stops_joining_under_follower_reaction_joins);
    RUN(a_follower_whose_reaction_collapses_still_leaves);
    RUN(the_charisma_follower_limit_stays);
    RUN(a_master_persuader_is_still_held_to_the_charisma_limit);
    RUN(heal_refuses_by_the_healer_stock_and_by_the_patient_under_the_switch);
    RUN(a_party_hit_by_a_spell_or_blast_is_accidental_under_grenade_blasts);
}
