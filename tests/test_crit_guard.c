// Tests for protection after critical incapacitation (criticals, milestone
// CR-1).
//
// The rules live on one record at a time, with no engine state, so zsogg's
// rule can be checked case by case: protection starts on recovery, lasts in
// turn-based until the end of the first turn that begins with the character
// up, and in real time for one turn length from the last moment they were
// seen down.
#include <string.h>

#include "game/crit_guard.h"

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
}
