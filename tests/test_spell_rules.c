// The Combat Rebalance's spell table, `rules/spells.mes`, and the cost rule
// it feeds (magic, milestone MG-1): what an entry may say, and what a cast
// costs a caster, pure. The engine side - the charge, the preview, the AI -
// is exercised in scripted runs.

#include "game/combat.h"
#include "game/spell_rules.h"

#include "test_harness.h"

TEST(an_entry_reads_its_ap_cost)
{
    int ap = -1;

    CHECK(spell_rule_parse("APCost: 5", &ap));
    CHECK_INT(ap, 5);
    CHECK(spell_rule_parse("  APCost:7 ", &ap));
    CHECK_INT(ap, 7);
    CHECK(spell_rule_parse("APCost: 4", &ap));
    CHECK_INT(ap, 4);
}

TEST(a_cost_below_4_or_anything_else_is_refused_whole)
{
    int ap = 6;

    // zsogg, 2026-10-10: no spell below 4 AP - refused, not clamped.
    CHECK(!spell_rule_parse("APCost: 3", &ap));
    CHECK(!spell_rule_parse("APCost: 11", &ap));
    CHECK(!spell_rule_parse("APCost: four", &ap));
    CHECK(!spell_rule_parse("Cost: 5", &ap));
    CHECK(!spell_rule_parse("APCost: 5, Crit: 1", &ap));
    CHECK(!spell_rule_parse("", &ap));
    CHECK_INT(ap, 6);
}

TEST(a_complete_college_casts_for_one_less_never_below_3)
{
    // zsogg, 2026-10-10: all five of a college known - 1 AP less, never below 3.
    CHECK_INT(combat_spell_ap_rule(4, false), 4);
    CHECK_INT(combat_spell_ap_rule(7, false), 7);
    CHECK_INT(combat_spell_ap_rule(4, true), 3);
    CHECK_INT(combat_spell_ap_rule(5, true), 4);
    CHECK_INT(combat_spell_ap_rule(7, true), 6);
    CHECK_INT(COMBAT_ITEM_CAST_COST, 4);
}

TEST(the_aptitude_part_of_scaled_damage_is_30_percent_smaller)
{
    // zsogg, 2026-10-11. Harm is 3-40: stock gives a full mage 40.
    CHECK_INT(spell_rules_scaled_damage(3, 40, 100, 100), 40);
    CHECK_INT(spell_rules_scaled_damage(3, 40, 100, SPELL_APTITUDE_DAMAGE_PERCENT), 28);
    CHECK_INT(spell_rules_scaled_damage(3, 40, 60, SPELL_APTITUDE_DAMAGE_PERCENT), 18);
    CHECK_INT(spell_rules_scaled_damage(20, 70, 100, SPELL_APTITUDE_DAMAGE_PERCENT), 55);
    CHECK_INT(spell_rules_scaled_damage(50, 100, 100, SPELL_APTITUDE_DAMAGE_PERCENT), 85);
    // At aptitude 0 or below, the minimum, as stock.
    CHECK_INT(spell_rules_scaled_damage(3, 40, 0, SPELL_APTITUDE_DAMAGE_PERCENT), 3);
    CHECK_INT(spell_rules_scaled_damage(3, 40, -40, SPELL_APTITUDE_DAMAGE_PERCENT), 3);
}

void test_spell_rules_suite(void)
{
    RUN(an_entry_reads_its_ap_cost);
    RUN(a_cost_below_4_or_anything_else_is_refused_whole);
    RUN(a_complete_college_casts_for_one_less_never_below_3);
    RUN(the_aptitude_part_of_scaled_damage_is_30_percent_smaller);
}
