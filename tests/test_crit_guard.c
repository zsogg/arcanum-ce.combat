// Tests for protection after critical incapacitation (criticals, milestone
// CR-1).
//
// The rules live on one record at a time, with no engine state, so zsogg's
// rule can be checked case by case: protection starts on recovery, lasts in
// turn-based until the end of the first turn that begins with the character
// up, and in real time for one turn length from the last moment they were
// seen down.
#include <string.h>

#include "game/combat.h"
#include "game/crit_effects.h"
#include "game/crit_guard.h"
#include "game/skill.h"

#include "test_harness.h"

#define R 2000

static CritGuardEntry incapacitated_at(unsigned int clock_ms)
{
    CritGuardEntry entry;

    memset(&entry, 0, sizeof(entry));
    entry.obj = 1;
    crit_guard_entry_incapacitate(&entry, clock_ms);
    crit_guard_entry_observe(&entry, true, clock_ms);
    return entry;
}

TEST(protection_starts_on_recovery)
{
    CritGuardEntry entry = incapacitated_at(1000);

    // Still stunned, down or unconscious: another critical can land.
    CHECK(!crit_guard_entry_protects(&entry, true, true, 1000, R));
    CHECK(!crit_guard_entry_protects(&entry, true, false, 1000, R));

    // Up again: protected.
    CHECK(crit_guard_entry_protects(&entry, false, true, 1200, R));
    CHECK(crit_guard_entry_protects(&entry, false, false, 1200, R));
}

TEST(nothing_protects_before_they_are_seen_down)
{
    CritGuardEntry entry;

    // A fumble rolls its hit on its wielder twice in one attack: the second
    // pass comes before the first pass's knockdown is applied, and must not
    // count as a recovery.
    memset(&entry, 0, sizeof(entry));
    entry.obj = 1;
    crit_guard_entry_incapacitate(&entry, 500);
    CHECK(!crit_guard_entry_protects(&entry, false, true, 500, R));
    CHECK(!crit_guard_entry_protects(&entry, false, false, 500, R));

    // Seen down, then up.
    crit_guard_entry_observe(&entry, true, 700);
    crit_guard_entry_observe(&entry, false, 900);
    CHECK(crit_guard_entry_protects(&entry, false, false, 900, R));
    CHECK(crit_guard_entry_protects(&entry, false, false, 700 + R - 1, R));
    CHECK(!crit_guard_entry_protects(&entry, false, false, 700 + R, R));
}

TEST(an_effect_that_never_took_lapses)
{
    CritGuardEntry entry;

    memset(&entry, 0, sizeof(entry));
    entry.obj = 1;
    crit_guard_entry_incapacitate(&entry, 0);
    CHECK(crit_guard_entry_lapsed(&entry, false, false, R, R));
}

TEST(turn_based_protection_lasts_through_their_next_full_turn)
{
    CritGuardEntry entry = incapacitated_at(0);

    // Knocked down on the attacker's turn, up 200 ms later. Their own turn
    // begins with them up, and ends their protection when it ends.
    crit_guard_entry_turn_started(&entry, false);
    CHECK(crit_guard_entry_protects(&entry, false, true, 5000, R));
    CHECK(crit_guard_entry_turn_ended(&entry));
}

TEST(a_turn_that_begins_with_them_down_does_not_count)
{
    CritGuardEntry entry = incapacitated_at(0);

    // Still lying down when their turn begins: they get up during it, which
    // costs AP, so that turn is not the full one.
    crit_guard_entry_turn_started(&entry, true);
    CHECK(!crit_guard_entry_turn_ended(&entry));

    crit_guard_entry_turn_started(&entry, false);
    CHECK(crit_guard_entry_turn_ended(&entry));
}

TEST(an_incapacitation_during_their_own_turn_restarts_the_count)
{
    CritGuardEntry entry = incapacitated_at(0);

    // A fumble knocks them down on a turn that began with them up.
    crit_guard_entry_turn_started(&entry, false);
    crit_guard_entry_incapacitate(&entry, 100);
    CHECK(!crit_guard_entry_turn_ended(&entry));
}

TEST(real_time_protection_lasts_one_turn_length_from_recovery)
{
    // Last seen down at 10 s, so recovered just after.
    CritGuardEntry entry = incapacitated_at(10000);

    CHECK(crit_guard_entry_protects(&entry, false, false, 10000 + R - 1, R));
    CHECK(!crit_guard_entry_lapsed(&entry, false, false, 10000 + R - 1, R));

    CHECK(!crit_guard_entry_protects(&entry, false, false, 10000 + R, R));
    CHECK(crit_guard_entry_lapsed(&entry, false, false, 10000 + R, R));
}

TEST(time_does_not_end_protection_in_turn_based_or_while_down)
{
    CritGuardEntry entry = incapacitated_at(0);

    // Turn-based: only the end of their full turn does.
    CHECK(!crit_guard_entry_lapsed(&entry, false, true, 60000, R));
    // Down: the record stays, and protects once they are up.
    CHECK(!crit_guard_entry_lapsed(&entry, true, false, 60000, R));
}

TEST(the_clock_may_wrap)
{
    CritGuardEntry entry = incapacitated_at(0xFFFFFF00u);

    CHECK(crit_guard_entry_protects(&entry, false, false, 0x00000100u, R));
    CHECK(!crit_guard_entry_lapsed(&entry, false, false, 0x00000100u, R));
}

TEST(more_criticals_lift_a_firearm_to_a_floor_then_add_five)
{
    // Criticals milestone CR-2, zsogg's rule: a firearm gets max(8, stock)
    // + 5, every other attack stock + 5, and Dodge and the other skills
    // nothing.
    CHECK_INT(skill_more_criticals(SKILL_FIREARMS, 2), 13); // a beginner
    CHECK_INT(skill_more_criticals(SKILL_FIREARMS, 6), 13); // Firearms 20
    CHECK_INT(skill_more_criticals(SKILL_FIREARMS, 10), 15);
    CHECK_INT(skill_more_criticals(SKILL_MELEE, 2), 7);
    CHECK_INT(skill_more_criticals(SKILL_BOW, 6), 11);
    CHECK_INT(skill_more_criticals(SKILL_THROWING, 0), 5);
    CHECK_INT(skill_more_criticals(SKILL_DODGE, 2), 2);
    CHECK_INT(skill_more_criticals(SKILL_HEAL, 40), 40);
}

// Criticals milestone CR-2: temporary critical effects, zsogg's rules.

static CritEffectEntry effects_applied(unsigned int effects)
{
    CritEffectEntry entry;

    memset(&entry, 0, sizeof(entry));
    entry.obj = 1;
    crit_effects_entry_apply(&entry, effects, 1000);
    return entry;
}

TEST(a_critical_leaves_the_effect_of_where_it_struck)
{
    CHECK_INT(crit_effects_for_location(HIT_LOC_TORSO), CRIT_EFFECT_WINDED);
    CHECK_INT(crit_effects_for_location(HIT_LOC_ARM), CRIT_EFFECT_NUMBED_ARM);
    CHECK_INT(crit_effects_for_location(HIT_LOC_LEG), CRIT_EFFECT_HOBBLED);
    CHECK_INT(crit_effects_for_location(HIT_LOC_HEAD), CRIT_EFFECT_STAGGERED);
}

TEST(winded_is_spent_at_once_and_the_rest_last)
{
    CritEffectEntry entry = effects_applied(CRIT_EFFECT_WINDED | CRIT_EFFECT_HOBBLED);

    CHECK_INT(entry.effects, CRIT_EFFECT_HOBBLED);
}

TEST(numbed_and_dazzled_cost_twenty_to_hit_and_do_not_stack)
{
    CritEffectEntry entry = effects_applied(CRIT_EFFECT_NUMBED_ARM);

    CHECK_INT(crit_effects_entry_to_hit_penalty(&entry), 20);
    crit_effects_entry_apply(&entry, CRIT_EFFECT_DAZZLED, 1500);
    CHECK_INT(crit_effects_entry_to_hit_penalty(&entry), 20);
}

TEST(hobbled_doubles_moving_in_turn_based_only)
{
    CritEffectEntry entry = effects_applied(CRIT_EFFECT_HOBBLED);

    CHECK_INT(crit_effects_entry_move_multiplier(&entry, true), 2);
    CHECK_INT(crit_effects_entry_move_multiplier(&entry, false), 1);
}

TEST(staggered_halves_the_next_turn_only)
{
    CritEffectEntry entry = effects_applied(CRIT_EFFECT_STAGGERED);

    // Granted as the turn begins, before the turn counts as started.
    CHECK_INT(crit_effects_entry_turn_action_points(&entry, 9), 4);
    crit_effects_entry_turn_started(&entry);
    CHECK_INT(crit_effects_entry_turn_action_points(&entry, 9), 9);
    CHECK(crit_effects_entry_turn_ended(&entry));
}

TEST(effects_last_until_the_end_of_the_next_turn_or_r_in_real_time)
{
    CritEffectEntry entry = effects_applied(CRIT_EFFECT_NUMBED_ARM);

    // Not before a turn of theirs has begun.
    CHECK(!crit_effects_entry_turn_ended(&entry));
    // Real time: one turn length from when it was dealt.
    CHECK(!crit_effects_entry_lapsed(&entry, false, 1000 + R - 1, R));
    CHECK(crit_effects_entry_lapsed(&entry, false, 1000 + R, R));
    CHECK(!crit_effects_entry_lapsed(&entry, true, 1000 + 60000, R));
    // A new effect refreshes the record.
    crit_effects_entry_turn_started(&entry);
    crit_effects_entry_apply(&entry, CRIT_EFFECT_HOBBLED, 5000);
    CHECK(!crit_effects_entry_turn_ended(&entry));
    CHECK_INT(entry.effects, CRIT_EFFECT_NUMBED_ARM | CRIT_EFFECT_HOBBLED);
}

TEST(effects_are_named_for_the_observer)
{
    char names[64];

    crit_effects_names(CRIT_EFFECT_WINDED | CRIT_EFFECT_STAGGERED, names, (int)sizeof(names));
    CHECK_STR(names, "\"winded\",\"staggered\"");
}

void test_crit_guard_suite(void)
{
    RUN(protection_starts_on_recovery);
    RUN(nothing_protects_before_they_are_seen_down);
    RUN(an_effect_that_never_took_lapses);
    RUN(turn_based_protection_lasts_through_their_next_full_turn);
    RUN(a_turn_that_begins_with_them_down_does_not_count);
    RUN(an_incapacitation_during_their_own_turn_restarts_the_count);
    RUN(real_time_protection_lasts_one_turn_length_from_recovery);
    RUN(time_does_not_end_protection_in_turn_based_or_while_down);
    RUN(the_clock_may_wrap);
    RUN(more_criticals_lift_a_firearm_to_a_floor_then_add_five);
    RUN(a_critical_leaves_the_effect_of_where_it_struck);
    RUN(winded_is_spent_at_once_and_the_rest_last);
    RUN(numbed_and_dazzled_cost_twenty_to_hit_and_do_not_stack);
    RUN(hobbled_doubles_moving_in_turn_based_only);
    RUN(staggered_halves_the_next_turn_only);
    RUN(effects_last_until_the_end_of_the_next_turn_or_r_in_real_time);
    RUN(effects_are_named_for_the_observer);
}
