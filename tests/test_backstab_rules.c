// `backstab rules` (zsogg, 2026-10-28): a full backstab deals the weapon's
// damage twice plus 3 x Backstab level, and its critical bonus has a floor of
// 8 from level 4 and 14 from level 8.

#include "game/combat.h"
#include "game/skill.h"

#include "test_harness.h"

TEST(a_backstab_doubles_the_roll_and_adds_three_per_level)
{
    // A Dagger's 3 at one point (level 4), and at five (level 20).
    CHECK_INT(combat_backstab_damage(3, 4), 18);
    CHECK_INT(combat_backstab_damage(3, 20), 66);
    CHECK_INT(combat_backstab_bonus(20), 60);
    CHECK_INT(combat_backstab_damage(0, 0), 0);
}

TEST(the_critical_floor_steps_at_one_and_two_points)
{
    CHECK_INT(skill_backstab_crit_floor(0), 0);
    CHECK_INT(skill_backstab_crit_floor(3), 0);
    CHECK_INT(skill_backstab_crit_floor(4), 8);
    CHECK_INT(skill_backstab_crit_floor(7), 8);
    CHECK_INT(skill_backstab_crit_floor(8), 14);
    CHECK_INT(skill_backstab_crit_floor(20), 14);
}

void test_backstab_rules_suite(void)
{
    RUN(a_backstab_doubles_the_roll_and_adds_three_per_level);
    RUN(the_critical_floor_steps_at_one_and_two_points);
}
