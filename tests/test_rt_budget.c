// Tests for the real-time action budget's rules (real-time session B).
//
// The rules live on one entry at a time, with no engine state, so each rule on
// the session-2 rule sheet can be checked on its own: the duration formula
// against the sheet's own worked numbers, then how an action goes from
// accepted to released to ready, and what pausing, moving and a switch to
// turn-based do to it.
#include <stdio.h>
#include <string.h>

#include "game/combat.h"
#include "game/combat_observer.h"
#include "game/feature.h"
#include "game/gamelib.h"
#include "game/rt_budget.h"
#include "game/settings.h"
#include "game/uap.h"

#include "test_harness.h"

static AnimID run_of(int slot, int unique)
{
    AnimID id;

    memset(&id, 0, sizeof(id));
    id.slot_num = slot;
    id.unique_id = unique;
    return id;
}

static RtBudgetEntry accepted(int duration_ms)
{
    RtBudgetEntry entry;

    memset(&entry, 0, sizeof(entry));
    rt_budget_entry_accept(&entry, RT_BUDGET_ATTACK, run_of(3, 77), 6,
        duration_ms);
    return entry;
}

TEST(the_duration_is_the_rule_sheets_own_arithmetic)
{
    // Rule 1, R = 2 s, and the worked timelines: the fighter's Speed is 11 and
    // a guard's 9. The divisor is what a turn grants, through the rule
    // turn-based itself uses.
    CHECK_INT(rt_budget_duration_ms(2000, 6, combat_action_points_for_speed(11)), 1091); // stock Bow
    CHECK_INT(rt_budget_duration_ms(2000, 4, combat_action_points_for_speed(11)), 727); // fast Bow, Harm
    CHECK_INT(rt_budget_duration_ms(2000, 5, combat_action_points_for_speed(11)), 909); // Quality Sword
    CHECK_INT(rt_budget_duration_ms(2000, 7, combat_action_points_for_speed(9)), 1556); // guard, Rusty Sword

    // `max(5, Speed)`: a slow actor is not made slower still.
    CHECK_INT(combat_action_points_for_speed(3), 5);
    CHECK_INT(combat_action_points_for_speed(5), 5);
    CHECK_INT(combat_action_points_for_speed(11), 11);
    CHECK_INT(rt_budget_duration_ms(2000, 4, combat_action_points_for_speed(3)), 1600);

    // No cost, pace or allowance is no time - never a negative one.
    CHECK_INT(rt_budget_duration_ms(2000, 0, 11), 0);
    CHECK_INT(rt_budget_duration_ms(0, 6, 11), 0);
    CHECK_INT(rt_budget_duration_ms(-5, 6, 11), 0);
    CHECK_INT(rt_budget_duration_ms(2000, 6, 0), 0);
}

TEST(an_action_is_over_only_when_it_has_released_and_its_time_is_up)
{
    RtBudgetEntry entry = accepted(1000);

    // Winding up: not ready however much time passes, because nothing has
    // happened yet.
    CHECK(!rt_budget_entry_tick(&entry, 1500, true));
    CHECK(!rt_budget_entry_ready(&entry));
    CHECK_INT(entry.state, RT_BUDGET_ACCEPTED);

    // Released after its time was already up - a wind-up longer than the
    // budget. Ready at once, and reported by the next tick (rule 6's minimum).
    rt_budget_entry_release(&entry);
    CHECK(rt_budget_entry_ready(&entry));
    CHECK(rt_budget_entry_tick(&entry, 0, true));
    CHECK_INT(entry.state, RT_BUDGET_READY);
}

TEST(recovery_counts_from_acceptance_and_reports_ready_once)
{
    RtBudgetEntry entry = accepted(1091);

    CHECK(!rt_budget_entry_tick(&entry, 700, true)); // the wind-up counts
    rt_budget_entry_release(&entry);
    CHECK(!rt_budget_entry_ready(&entry));
    CHECK(!rt_budget_entry_tick(&entry, 390, true)); // 1090 of 1091
    CHECK(rt_budget_entry_tick(&entry, 16, true)); // ready on this tick
    CHECK_INT(entry.elapsed_ms, 1091); // capped, not 1106
    CHECK(!rt_budget_entry_tick(&entry, 16, true)); // and only once
}

TEST(time_that_does_not_count_does_not_recover)
{
    RtBudgetEntry entry = accepted(500);

    // Rules 8 and 12: paused, or moving with the setting on - the caller says
    // it does not count, and nothing accrues.
    rt_budget_entry_release(&entry);
    CHECK(!rt_budget_entry_tick(&entry, 400, false));
    CHECK(!rt_budget_entry_tick(&entry, 400, false));
    CHECK_INT(entry.elapsed_ms, 0);
    CHECK(!rt_budget_entry_tick(&entry, 400, true));
    CHECK(rt_budget_entry_tick(&entry, 100, true));
}

TEST(turn_based_cancels_a_wind_up_and_restarts_a_recovery)
{
    RtBudgetEntry winding = accepted(1000);
    RtBudgetEntry recovering = accepted(1000);

    // Rule 5. An action that had not released never happened: cancelled.
    CHECK(rt_budget_entry_turn_based_began(&winding));
    CHECK(rt_budget_entry_ready(&winding));

    // One that had released starts its recovery again from zero, so switching
    // modes can lengthen a recovery and never shorten it.
    rt_budget_entry_tick(&recovering, 800, true);
    rt_budget_entry_release(&recovering);
    CHECK(!rt_budget_entry_turn_based_began(&recovering));
    CHECK_INT(recovering.elapsed_ms, 0);
    CHECK(!rt_budget_entry_ready(&recovering));
    CHECK(!rt_budget_entry_tick(&recovering, 999, true));
    CHECK(rt_budget_entry_tick(&recovering, 1, true));

    // A ready actor is unaffected.
    {
        RtBudgetEntry ready;
        memset(&ready, 0, sizeof(ready));
        CHECK(!rt_budget_entry_turn_based_began(&ready));
        CHECK(rt_budget_entry_ready(&ready));
    }
}

TEST(a_release_only_counts_for_an_accepted_action)
{
    RtBudgetEntry entry;

    // A release with nothing accepted - an attempt that began before the
    // budget was switched on - does not invent a recovery.
    memset(&entry, 0, sizeof(entry));
    rt_budget_entry_release(&entry);
    CHECK_INT(entry.state, RT_BUDGET_READY);
    CHECK(rt_budget_entry_ready(&entry));
}

TEST(a_throw_costs_what_the_throw_goal_charges_in_each_build)
{
    // MODDING-NOTES.md quirk 51, read from both executables at 0x4332C0: in
    // 1.0.7.4 the player throws for nothing and everyone else pays 2; UAP
    // charges everyone 4. The budget reads this same function, so these are
    // its throw costs too (session C, zsogg's option 1).
    settings_init(&settings, "unused");
    feature_init();

    CHECK(!uap_fixes_enabled());
    CHECK_INT(combat_throw_cost_for(true), 0);
    CHECK_INT(combat_throw_cost_for(false), 2);

    settings_set_value(&settings, feature_key(FEATURE_UAP_FIXES), 1);
    CHECK(uap_fixes_enabled());
    CHECK_INT(combat_throw_cost_for(true), 4);
    CHECK_INT(combat_throw_cost_for(false), 4);

    // At the supported baseline, a Speed 11 thrower's grenade is the fast
    // Bow's and Harm's 727 ms; with the player exempt it takes no time at all,
    // which is the original's free throw carried into real time.
    CHECK_INT(rt_budget_duration_ms(2000, combat_throw_cost_for(true), combat_action_points_for_speed(11)), 727);
    settings_set_value(&settings, feature_key(FEATURE_UAP_FIXES), 0);
    CHECK_INT(rt_budget_duration_ms(2000, combat_throw_cost_for(true), combat_action_points_for_speed(11)), 0);

    feature_on_change(FEATURE_UAP_FIXES, NULL);
    settings_exit(&settings);
}

TEST(the_shared_costs_are_the_numbers_turn_based_charges)
{
    // The real-time budget holds no cost of its own: it reads the functions
    // turn-based charges through. These pin what those return today, so a
    // rebalance that changes one sees exactly which number moved - and moves
    // real time with it by construction. Attacks are pinned by the worked
    // timelines above (`combat_attack_cost` needs a wielded weapon).
    CHECK_INT(combat_cast_cost(0, -1), 4); // 0x429B50: a flat 4
    CHECK_INT(combat_cast_cost(0, 55), 4); // Harm: the spell does not change it yet
    CHECK_INT(combat_skill_cost(0, -1), 4); // 0x42A2A0, and disarming traps
    CHECK_INT(combat_use_object_cost(0, 0), 2); // 0x428A10: doors, containers
    CHECK_INT(combat_use_item_cost(0, 0), 4); // item_use_on_obj / _on_loc
    CHECK_INT(combat_pickup_cost(0, 0), 1); // 0x42AA70
    CHECK_INT(combat_get_up_cost(0), 5); // AGbeginGetUpAnim
}

TEST(a_pace_outside_its_range_is_the_default)
{
    // Session F: a malformed `real time pace ms` must not switch the budget
    // off (0 is what atoi makes of a word; "2" is seconds typed by mistake)
    // or overflow a duration. The range's ends are themselves accepted.
    CHECK_INT(rt_budget_pace_ms(2000), 2000);
    CHECK_INT(rt_budget_pace_ms(1500), 1500);
    CHECK_INT(rt_budget_pace_ms(RT_BUDGET_MIN_PACE_MS), RT_BUDGET_MIN_PACE_MS);
    CHECK_INT(rt_budget_pace_ms(RT_BUDGET_MAX_PACE_MS), RT_BUDGET_MAX_PACE_MS);
    CHECK_INT(rt_budget_pace_ms(0), RT_BUDGET_DEFAULT_PACE_MS);
    CHECK_INT(rt_budget_pace_ms(2), RT_BUDGET_DEFAULT_PACE_MS);
    CHECK_INT(rt_budget_pace_ms(-2000), RT_BUDGET_DEFAULT_PACE_MS);
    CHECK_INT(rt_budget_pace_ms(RT_BUDGET_MIN_PACE_MS - 1), RT_BUDGET_DEFAULT_PACE_MS);
    CHECK_INT(rt_budget_pace_ms(RT_BUDGET_MAX_PACE_MS + 1), RT_BUDGET_DEFAULT_PACE_MS);
    CHECK_INT(rt_budget_pace_ms(2147483647), RT_BUDGET_DEFAULT_PACE_MS);
    // The largest accepted pace keeps a heavy action's duration in range.
    CHECK_INT(rt_budget_duration_ms(RT_BUDGET_MAX_PACE_MS, 100, 5), 1200000);
}

TEST(a_chaser_swings_only_at_a_target_that_is_not_walking_away)
{
    // The chase stopgap (zsogg, 2026-09-24). Nothing changes unless it
    // applies. Then a swing waits while its target walks away in reach, is
    // given up at a target out of reach - moving or not, since it could only
    // strike air - and goes ahead at a target in reach that is not walking away.
    CHECK_INT(rt_chase_decide(false, true, true), RT_CHASE_SWING);
    CHECK_INT(rt_chase_decide(false, false, true), RT_CHASE_SWING);
    CHECK_INT(rt_chase_decide(false, false, false), RT_CHASE_SWING);
    CHECK_INT(rt_chase_decide(true, true, false), RT_CHASE_SWING);
    CHECK_INT(rt_chase_decide(true, true, true), RT_CHASE_WAIT);
    CHECK_INT(rt_chase_decide(true, false, true), RT_CHASE_GIVE_UP);
    CHECK_INT(rt_chase_decide(true, false, false), RT_CHASE_GIVE_UP);
}

TEST(the_budget_events_carry_what_a_tolerance_reads)
{
    char trace[4096];
    FILE* stream;
    size_t got;

    combat_observer_test_reset();
    remove("./rt-budget-test.jsonl");
    CHECK(combat_observer_open("./rt-budget-test.jsonl"));
    combat_observer_budget("action.accepted", "archer", "attack", 6, 1091, 0, NULL);
    combat_observer_budget("actor.ready", "archer", "attack", 6, 1091, 1091, NULL);
    combat_observer_budget("action.cancelled", "mage", "cast", 4, 727, 120, "turn_based");
    combat_observer_close();

    stream = fopen("./rt-budget-test.jsonl", "rb");
    CHECK(stream != NULL);
    if (stream == NULL) return;
    got = fread(trace, 1, sizeof(trace) - 1, stream);
    fclose(stream);
    trace[got] = '\0';
    remove("./rt-budget-test.jsonl");
    combat_observer_test_reset();

    CHECK(strstr(trace, "\"event\":\"action.accepted\",\"turn\":null,\"action_id\":null,\"actor\":\"archer\",\"category\":\"attack\",\"action_points\":6,\"duration_ms\":1091,\"elapsed_ms\":0}") != NULL);
    CHECK(strstr(trace, "\"event\":\"actor.ready\"") != NULL);
    CHECK(strstr(trace, "\"event\":\"action.cancelled\",\"turn\":null,\"action_id\":null,\"actor\":\"mage\",\"category\":\"cast\",\"action_points\":4,\"duration_ms\":727,\"elapsed_ms\":120,\"reason\":\"turn_based\"}") != NULL);
}

void test_rt_budget_suite(void)
{
    RUN(the_duration_is_the_rule_sheets_own_arithmetic);
    RUN(an_action_is_over_only_when_it_has_released_and_its_time_is_up);
    RUN(recovery_counts_from_acceptance_and_reports_ready_once);
    RUN(time_that_does_not_count_does_not_recover);
    RUN(turn_based_cancels_a_wind_up_and_restarts_a_recovery);
    RUN(a_release_only_counts_for_an_accepted_action);
    RUN(a_throw_costs_what_the_throw_goal_charges_in_each_build);
    RUN(the_shared_costs_are_the_numbers_turn_based_charges);
    RUN(a_pace_outside_its_range_is_the_default);
    RUN(a_chaser_swings_only_at_a_target_that_is_not_walking_away);
    RUN(the_budget_events_carry_what_a_tolerance_reads);
}
