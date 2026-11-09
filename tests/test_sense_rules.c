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

TEST(stock_hearing_through_a_wall_grows_with_distance_and_never_reads_1000)
{
    // Review 2026-10-31, finding 9: with every switch off, a door that took
    // the hearing distance below zero made `ai_can_hear` answer 1000 - the
    // prowl indicator's "Safe" - where stock answered the distance past it.
    CHECK_INT(sense_rules_hear_shortfall(2, -1, false), 3);
    CHECK_INT(sense_rules_hear_shortfall(0, -3, false), 3);
    CHECK_INT(sense_rules_hear_shortfall(2, 4, false), 0);
    CHECK_INT(sense_rules_hear_shortfall(6, 4, false), 2);
    // Under `walls stop sound`, -1 is "not heard at any distance".
    CHECK_INT(sense_rules_hear_shortfall(2, -1, true), 1000);
    CHECK_INT(sense_rules_hear_shortfall(6, 4, true), 2);
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

// Card PG1: `ai_presence_loudness` looks `moving` up only for a critter in
// plain view under the rule, because the rule reads it nowhere else. Should
// the rule change to read it elsewhere, this fails and the shortcut with it.
TEST(moving_matters_only_in_plain_view_under_the_rule)
{
    int concealed;
    int moving;
    int still_is_silent;

    for (concealed = 0; concealed <= 1; concealed++) {
        for (moving = 0; moving <= 1; moving++) {
            for (still_is_silent = 0; still_is_silent <= 1; still_is_silent++) {
                CHECK_INT(sense_rules_presence_loudness(concealed, !concealed && still_is_silent && moving, still_is_silent),
                    sense_rules_presence_loudness(concealed, moving, still_is_silent));
            }
        }
    }
}

// Review 2026-11-08, targeting-2: a hunter or an NPC turning from an
// unreachable target picks only a member it perceives - the Human Thief no
// longer walks to an unseen, prowling archer.
TEST(a_side_member_is_turned_to_only_if_perceived)
{
    CHECK(sense_rules_perceives(true, false, false));
    CHECK(sense_rules_perceives(true, false, true));
    CHECK(sense_rules_perceives(false, true, false)); // stock: heard counts
    CHECK(!sense_rules_perceives(false, true, true)); // hearing alerts: heard only does not
    CHECK(!sense_rules_perceives(false, false, false));
}

void test_sense_rules_suite(void)
{
    RUN(a_side_member_is_turned_to_only_if_perceived);
    RUN(moving_matters_only_in_plain_view_under_the_rule);
    RUN(a_wall_stops_ordinary_sound_and_lets_a_third_of_a_loud_one_through);
    RUN(stock_hearing_through_a_wall_grows_with_distance_and_never_reads_1000);
    RUN(the_side_of_the_view_sees_half_as_far_and_the_back_sees_nothing);
    RUN(a_blunt_strike_is_loud_only_under_the_rule);
    RUN(standing_still_is_silent_only_under_the_rule);
}
