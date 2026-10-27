// The sight and hearing overhaul's pure rules (DB-3, 2026-10-27): what a wall
// does to a sound, how far the side of a critter's view reaches, which strike
// is loud and how loud a critter is by being there. The engine side - who
// hears, who looks, who investigates - is exercised on the sight bench.

#include "game/combat.h"
#include "game/sense_rules.h"

#include "test_harness.h"

TEST(a_wall_stops_ordinary_sound_and_lets_a_third_of_a_loud_one_through)
{
    // Stock: the cost between comes off the open range, loud or not.
    CHECK_INT(sense_rules_hear_distance(5, 0, false, false), 5);
    CHECK_INT(sense_rules_hear_distance(5, 2, false, false), 3);
    CHECK_INT(sense_rules_hear_distance(8, 3, true, false), 5);
    // Under the rule: open air is as stock...
    CHECK_INT(sense_rules_hear_distance(5, 0, false, true), 5);
    CHECK_INT(sense_rules_hear_distance(8, 0, true, true), 8);
    // ...a wall or a closed door stops anything not loud...
    CHECK_INT(sense_rules_hear_distance(5, 2, false, true), -1);
    CHECK_INT(sense_rules_hear_distance(5, 1, false, true), -1);
    // ...and a loud sound carries a third of its open range, however many
    // walls stand between.
    CHECK_INT(sense_rules_hear_distance(9, 2, true, true), 3);
    CHECK_INT(sense_rules_hear_distance(9, 6, true, true), 3);
    CHECK_INT(sense_rules_hear_distance(8, 1, true, true), 2);
}

TEST(the_side_of_the_view_sees_half_as_far_and_the_back_sees_nothing)
{
    int delta;

    // Stock: five of eight directions see the full range.
    for (delta = 0; delta < 8; delta++) {
        bool seen = delta <= 2 || delta >= 6;
        CHECK_INT(sense_rules_sight_range(8, delta, false), seen ? 8 : -1);
    }
    // Under the rule the outer two see half, never less than a tile.
    CHECK_INT(sense_rules_sight_range(8, 0, true), 8);
    CHECK_INT(sense_rules_sight_range(8, 1, true), 8);
    CHECK_INT(sense_rules_sight_range(8, 7, true), 8);
    CHECK_INT(sense_rules_sight_range(8, 2, true), 4);
    CHECK_INT(sense_rules_sight_range(8, 6, true), 4);
    CHECK_INT(sense_rules_sight_range(1, 2, true), 1);
    CHECK_INT(sense_rules_sight_range(8, 3, true), -1);
    CHECK_INT(sense_rules_sight_range(8, 4, true), -1);
    CHECK_INT(sense_rules_sight_range(8, 5, true), -1);
    // A delta outside 0-7 wraps.
    CHECK_INT(sense_rules_sight_range(8, -2, true), 4);
    CHECK_INT(sense_rules_sight_range(8, 10, true), 4);
}

TEST(a_blunt_strike_is_loud_only_under_the_rule)
{
    CHECK_INT(sense_rules_weapon_loudness(LOUDNESS_NORMAL, true, false), LOUDNESS_NORMAL);
    CHECK_INT(sense_rules_weapon_loudness(LOUDNESS_NORMAL, true, true), LOUDNESS_LOUD);
    CHECK_INT(sense_rules_weapon_loudness(LOUDNESS_NORMAL, false, true), LOUDNESS_NORMAL);
    // A silenced gun is not blunt, and stays silent.
    CHECK_INT(sense_rules_weapon_loudness(LOUDNESS_SILENT, false, true), LOUDNESS_SILENT);
    CHECK_INT(sense_rules_weapon_loudness(LOUDNESS_LOUD, false, true), LOUDNESS_LOUD);
}

TEST(standing_still_is_silent_only_under_the_rule)
{
    CHECK_INT(sense_rules_presence_loudness(false, false, false), LOUDNESS_NORMAL);
    CHECK_INT(sense_rules_presence_loudness(false, true, false), LOUDNESS_NORMAL);
    CHECK_INT(sense_rules_presence_loudness(true, true, false), LOUDNESS_SILENT);
    CHECK_INT(sense_rules_presence_loudness(false, false, true), LOUDNESS_SILENT);
    CHECK_INT(sense_rules_presence_loudness(false, true, true), LOUDNESS_NORMAL);
    CHECK_INT(sense_rules_presence_loudness(true, true, true), LOUDNESS_SILENT);
}

void test_sense_rules_suite(void)
{
    RUN(a_wall_stops_ordinary_sound_and_lets_a_third_of_a_loud_one_through);
    RUN(the_side_of_the_view_sees_half_as_far_and_the_back_sees_nothing);
    RUN(a_blunt_strike_is_loud_only_under_the_rule);
    RUN(standing_still_is_silent_only_under_the_rule);
}
