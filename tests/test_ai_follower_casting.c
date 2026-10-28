// Combat AI milestone AI-5 (`follower casting`): when an NPC reaches for its
// spells. The stock rule (`ai_magictech_chance`, 0x4ABE20) gives a critter with
// a leader no chance while both are above half health and its foe is not
// stronger; zsogg chose on 2026-10-27 that a caster follower of the player is
// not held back. Pure, so each part is pinned before it is seen in a fight.

#include "game/ai.h"
#include "game/magictech.h"
#include "game/obj_flags.h"
#include "game/target.h"

#include "test_harness.h"

// has_leader, player_leader, own hp, leader hp, foe stronger, caster type,
// keeps distance, follower casting.
static int chance(bool has_leader, bool player_leader, int own, int leader, bool stronger,
    bool caster, bool distance, bool switch_on, bool* waived)
{
    return ai_spell_chance_rule(has_leader, player_leader, own, leader, stronger, caster,
        distance, switch_on, waived);
}

TEST(a_leaderless_npc_has_the_stock_chance)
{
    bool waived;

    CHECK_INT(chance(false, false, 100, 0, false, true, true, false, &waived), 50);
    CHECK(!waived);
    CHECK_INT(chance(false, false, 100, 0, false, false, false, true, &waived), 25);
    CHECK(!waived);
}

TEST(a_healthy_follower_is_held_back_in_stock)
{
    bool waived;

    CHECK_INT(chance(true, true, 100, 100, false, true, true, false, &waived), 0);
    CHECK(!waived);
    CHECK_INT(chance(true, true, 51, 51, false, true, true, false, &waived), 0);
}

TEST(the_restraint_lifts_when_someone_is_hurt_or_the_foe_is_stronger)
{
    bool waived;

    // At half health or below, the follower's or its leader's.
    CHECK_INT(chance(true, true, 50, 100, false, true, true, false, &waived), 50);
    CHECK_INT(chance(true, true, 100, 50, false, false, false, false, &waived), 25);
    CHECK_INT(chance(true, true, 100, 100, true, false, true, false, &waived), 50);
    CHECK(!waived);
}

TEST(follower_casting_frees_a_caster_follower_of_the_player)
{
    bool waived;

    CHECK_INT(chance(true, true, 100, 100, false, true, true, true, &waived), 50);
    CHECK(waived);
    CHECK_INT(chance(true, true, 100, 100, false, true, false, true, &waived), 25);
    CHECK(waived);
}

TEST(follower_casting_keeps_a_fighter_follower_held_back)
{
    bool waived;

    CHECK_INT(chance(true, true, 100, 100, false, false, true, true, &waived), 0);
    CHECK(!waived);
}

TEST(follower_casting_leaves_an_npc_leaders_followers_alone)
{
    bool waived;

    // An enemy caster in a group with an NPC leader keeps the stock rule.
    CHECK_INT(chance(true, false, 100, 100, false, true, true, true, &waived), 0);
    CHECK(!waived);
}

TEST(a_waiver_is_reported_only_when_it_is_what_allowed_the_roll)
{
    bool waived = true;

    // Hurt: the stock rule already allows the roll.
    CHECK_INT(chance(true, true, 40, 100, false, true, true, true, &waived), 50);
    CHECK(!waived);
}

TEST(cast_highest_binds_all_but_a_follower_of_the_player_under_the_switch)
{
    CHECK(ai_cast_highest_applies(ONF_CAST_HIGHEST, false, false));
    CHECK(ai_cast_highest_applies(ONF_CAST_HIGHEST, true, false));
    CHECK(ai_cast_highest_applies(ONF_CAST_HIGHEST, false, true));
    CHECK(!ai_cast_highest_applies(ONF_CAST_HIGHEST, true, true));
    CHECK(!ai_cast_highest_applies(0, false, false));
    CHECK(!ai_cast_highest_applies(ONF_ALOOF, true, false));
}

TEST(a_spell_centred_on_its_caster_reaches_only_its_radius)
{
    CHECK(!ai_caster_centred_out_of_reach(1, 1));
    CHECK(!ai_caster_centred_out_of_reach(1, 0));
    CHECK(ai_caster_centred_out_of_reach(1, 2));
    CHECK(ai_caster_centred_out_of_reach(0, 1));
    // Any other spell is never refused for reach here.
    CHECK(!ai_caster_centred_out_of_reach(-1, 30));
}

// AI-6 F4 (7a): which buffs can go on the player, by the spell's begin
// targets in rules/spelllist.mes.
TEST(a_buff_on_any_critter_can_go_on_the_player)
{
    // Agility of Fire, Shield of Protection: [Begin]AoE: Tgt_Obj_T_Critter |
    // Tgt_Obj_No_ST_Critter_Dead (and the loader's no-invulnerable).
    CHECK(magictech_tgt_reaches_others(TGT_OBJ_T_CRITTER | TGT_OBJ_NO_ST_CRITTER_DEAD | TGT_OBJ_NO_INVULNERABLE));
    // Reflection Shield: AoE: Tgt_Object.
    CHECK(magictech_tgt_reaches_others(TGT_OBJECT));
}

TEST(a_self_only_buff_stays_on_its_caster)
{
    // Blur Sight: AoE: Tgt_Self.
    CHECK(!magictech_tgt_reaches_others(TGT_SELF));
    // Jolt-like: centred on the caster.
    CHECK(!magictech_tgt_reaches_others(TGT_SELF | TGT_OBJ_RADIUS | TGT_OBJ_T_CRITTER));
    // A critter spell that excludes the player.
    CHECK(!magictech_tgt_reaches_others(TGT_OBJ_T_CRITTER | TGT_OBJ_NO_T_PC));
    CHECK(!magictech_tgt_reaches_others(TGT_NONE));
}

void test_ai_follower_casting_suite(void)
{
    RUN(a_leaderless_npc_has_the_stock_chance);
    RUN(a_healthy_follower_is_held_back_in_stock);
    RUN(the_restraint_lifts_when_someone_is_hurt_or_the_foe_is_stronger);
    RUN(follower_casting_frees_a_caster_follower_of_the_player);
    RUN(follower_casting_keeps_a_fighter_follower_held_back);
    RUN(follower_casting_leaves_an_npc_leaders_followers_alone);
    RUN(a_waiver_is_reported_only_when_it_is_what_allowed_the_roll);
    RUN(cast_highest_binds_all_but_a_follower_of_the_player_under_the_switch);
    RUN(a_spell_centred_on_its_caster_reaches_only_its_radius);
    RUN(a_buff_on_any_critter_can_go_on_the_player);
    RUN(a_self_only_buff_stays_on_its_caster);
}
