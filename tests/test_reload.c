// Reload prototype session 2: the reload rules and the turn-based work, pure.
//
// What the rules file may say, and what one Reload command does with the AP a
// turn has left. The engine side - loaded shots on the weapon, the Q key, the
// shot - is exercised in scripted runs.

#include <string.h>

#include "game/reload.h"

#include "test_harness.h"

TEST(a_rule_reads_capacity_and_both_loading_costs)
{
    ReloadRule rule;

    memset(&rule, 0, sizeof(rule));
    CHECK(reload_rule_parse(6101, "Capacity: 1, LoadAP: 4, LoadAPApprentice: 3", &rule));
    CHECK_INT(rule.prototype, 6101);
    CHECK_INT(rule.capacity, 1);
    CHECK_INT(rule.load_ap, 4);
    CHECK_INT(rule.load_ap_apprentice, 3);

    // The Repeater, keys in another order, spacing as a hand writes it.
    CHECK(reload_rule_parse(6065, "LoadAP:3 ,Capacity: 25,  LoadAPApprentice : 2 ", &rule));
    CHECK_INT(rule.capacity, 25);
    CHECK_INT(rule.load_ap, 3);
    CHECK_INT(rule.load_ap_apprentice, 2);
}

TEST(training_costs_the_same_when_the_rule_does_not_say)
{
    ReloadRule rule;

    CHECK(reload_rule_parse(6101, "Capacity: 1, LoadAP: 4", &rule));
    CHECK_INT(rule.load_ap_apprentice, 4);
}

TEST(a_value_outside_its_range_is_clamped)
{
    ReloadRule rule;

    CHECK(reload_rule_parse(6101, "Capacity: 0, LoadAP: 99, LoadAPApprentice: -3", &rule));
    CHECK_INT(rule.capacity, RELOAD_CAPACITY_MIN);
    CHECK_INT(rule.load_ap, RELOAD_LOAD_AP_MAX);
    CHECK_INT(rule.load_ap_apprentice, RELOAD_LOAD_AP_MIN);
    CHECK(reload_rule_parse(6101, "Capacity: 500, LoadAP: 0", &rule));
    CHECK_INT(rule.capacity, RELOAD_CAPACITY_MAX);
    CHECK_INT(rule.load_ap, RELOAD_LOAD_AP_MIN);
}

TEST(a_malformed_rule_lists_nothing_and_changes_nothing)
{
    ReloadRule rule;

    memset(&rule, 0x5a, sizeof(rule));
    CHECK(!reload_rule_parse(6101, "Capacity: 1", &rule)); // no LoadAP
    CHECK(!reload_rule_parse(6101, "LoadAP: 4", &rule)); // no Capacity
    CHECK(!reload_rule_parse(6101, "Capacity: one, LoadAP: 4", &rule));
    CHECK(!reload_rule_parse(6101, "Capacity: 1, LoadAP: 4 AP", &rule));
    CHECK(!reload_rule_parse(6101, "Capacity: 1, LoadAP: 4, Jams: 2", &rule)); // unknown key
    CHECK(!reload_rule_parse(6101, "Capacity 1, LoadAP: 4", &rule)); // no colon
    CHECK(!reload_rule_parse(6101, "", &rule));
    CHECK_INT(rule.capacity, 0x5a5a5a5a);
}

TEST(a_reload_spends_only_what_the_turn_has)
{
    ReloadWork work;

    // The Flintlock untrained, 4 AP, after firing with 8: one AP left.
    work = reload_work(4, 0, 1);
    CHECK_INT(work.spent, 1);
    CHECK_INT(work.progress, 1);
    CHECK(!work.completed);

    // Next turn, the three still owed, with 8 to spend.
    work = reload_work(4, 1, 8);
    CHECK_INT(work.spent, 3);
    CHECK(work.completed);
    CHECK_INT(work.progress, 0);

    // Exactly enough.
    work = reload_work(3, 0, 3);
    CHECK_INT(work.spent, 3);
    CHECK(work.completed);

    // Nothing to spend: no work, no shot.
    work = reload_work(4, 2, 0);
    CHECK_INT(work.spent, 0);
    CHECK_INT(work.progress, 2);
    CHECK(!work.completed);
}

TEST(work_already_past_a_lowered_cost_completes_for_nothing)
{
    ReloadWork work;

    // 3 AP stored toward an untrained 4, then Apprentice training lowers the
    // cost to 3: the shot is finished without spending more.
    work = reload_work(3, 3, 5);
    CHECK_INT(work.spent, 0);
    CHECK(work.completed);
}

void test_reload_suite(void)
{
    RUN(a_rule_reads_capacity_and_both_loading_costs);
    RUN(training_costs_the_same_when_the_rule_does_not_say);
    RUN(a_value_outside_its_range_is_clamped);
    RUN(a_malformed_rule_lists_nothing_and_changes_nothing);
    RUN(a_reload_spends_only_what_the_turn_has);
    RUN(work_already_past_a_lowered_cost_completes_for_nothing);
}
