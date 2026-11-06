// Card DG1: the Dodge rules, pure. One case per part of the card's table
// (arcanum-work tasks/DG1-dodge-rules.md), with its floor and rounding.

#include "game/critter.h"
#include "game/dodge_rules.h"
#include "game/skill.h"

#include "test_harness.h"

TEST(armor_takes_its_class_penalty)
{
    CHECK_INT(dodge_rules_chance(40, 0, ENCUMBRANCE_LEVEL_NONE, 100), 40);
    CHECK_INT(dodge_rules_chance(40, 3, ENCUMBRANCE_LEVEL_NONE, 100), 37); // leather
    CHECK_INT(dodge_rules_chance(40, 6, ENCUMBRANCE_LEVEL_NONE, 100), 34); // chain
    CHECK_INT(dodge_rules_chance(40, 12, ENCUMBRANCE_LEVEL_NONE, 100), 28); // plate
}

TEST(encumbrance_takes_two_a_level_and_sixteen_at_severe)
{
    CHECK_INT(dodge_rules_encumbrance_penalty(ENCUMBRANCE_LEVEL_NONE), 0);
    CHECK_INT(dodge_rules_encumbrance_penalty(ENCUMBRANCE_LEVEL_LIGHT), 2);
    CHECK_INT(dodge_rules_encumbrance_penalty(ENCUMBRANCE_LEVEL_MEDIUM), 6);
    CHECK_INT(dodge_rules_encumbrance_penalty(ENCUMBRANCE_LEVEL_HEAVY), 10);
    CHECK_INT(dodge_rules_encumbrance_penalty(ENCUMBRANCE_LEVEL_SEVERE), 16);
    // Plate on a lightly encumbered dodger: both penalties.
    CHECK_INT(dodge_rules_chance(40, 12, ENCUMBRANCE_LEVEL_LIGHT, 100), 26);
}

TEST(the_chance_never_falls_below_zero)
{
    CHECK_INT(dodge_rules_chance(8, 12, ENCUMBRANCE_LEVEL_NONE, 100), 0);
    CHECK_INT(dodge_rules_chance(20, 12, ENCUMBRANCE_LEVEL_SEVERE, 30), 0);
    CHECK_INT(dodge_rules_chance(0, 0, ENCUMBRANCE_LEVEL_NONE, 100), 0);
}

TEST(a_firearm_share_comes_after_armor_and_rounds_down)
{
    CHECK_INT(dodge_rules_chance(40, 0, ENCUMBRANCE_LEVEL_NONE, 30), 12);
    CHECK_INT(dodge_rules_chance(40, 6, ENCUMBRANCE_LEVEL_NONE, 30), 10); // 34 x 0.3 = 10.2
    CHECK_INT(dodge_rules_chance(40, 12, ENCUMBRANCE_LEVEL_NONE, 30), 8); // 28 x 0.3 = 8.4
    CHECK_INT(dodge_rules_chance(24, 0, ENCUMBRANCE_LEVEL_NONE, 30), 7); // 7.2
    CHECK_INT(dodge_rules_chance(40, 0, ENCUMBRANCE_LEVEL_NONE, 0), 0);
}

TEST(training_sets_the_attackers_fumble)
{
    CHECK_INT(dodge_rules_fumble_chance(TRAINING_NONE), 0);
    CHECK_INT(dodge_rules_fumble_chance(TRAINING_APPRENTICE), 5);
    CHECK_INT(dodge_rules_fumble_chance(TRAINING_EXPERT), 10);
    CHECK_INT(dodge_rules_fumble_chance(TRAINING_MASTER), 20);
    CHECK_INT(dodge_rules_fumble_chance(-1), 0);
    CHECK_INT(dodge_rules_fumble_chance(TRAINING_COUNT), 0);
}

void test_dodge_rules_suite(void)
{
    RUN(armor_takes_its_class_penalty);
    RUN(encumbrance_takes_two_a_level_and_sixteen_at_severe);
    RUN(the_chance_never_falls_below_zero);
    RUN(a_firearm_share_comes_after_armor_and_rounds_down);
    RUN(training_sets_the_attackers_fumble);
}
