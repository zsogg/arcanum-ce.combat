// Initiative and Awareness, milestones 2-3 (card X10; zsogg, 2026-10-20):
// `encounter awareness` and `ambush safeguard`'s rules, against the real
// `encounter_awareness_score`, `encounter_awareness_difficulty`,
// `encounter_awareness_spots` and `combat_ambush_party_slot`. The check
// before a random encounter and the ambush's opening round are measured on
// the observatory (the replay's `encounter` row).
#include <stdbool.h>

#include "game/combat.h"
#include "test_harness.h"

TEST(awareness_is_perception_plus_intelligence)
{
    CHECK_INT(encounter_awareness_score(8, 9), 17);
}

TEST(the_difficulty_rises_five_a_power_step)
{
    CHECK_INT(encounter_awareness_difficulty(15, 1), 20);
    CHECK_INT(encounter_awareness_difficulty(15, 3), 30);
}

TEST(spotting_needs_the_difficulty_reached)
{
    CHECK(encounter_awareness_spots(16, 9, 25));
    CHECK(!encounter_awareness_spots(16, 8, 25));
}

TEST(the_ambush_moves_the_first_able_party_member_to_second)
{
    // Enemy, enemy, enemy, follower, player: the follower moves up.
    bool order[] = { false, false, false, true, true };
    CHECK_INT(combat_ambush_party_slot(order, 5), 3);
}

TEST(a_party_member_first_or_second_leaves_the_order)
{
    bool first[] = { true, false, false };
    bool second[] = { false, true, false };
    CHECK_INT(combat_ambush_party_slot(first, 3), -1);
    CHECK_INT(combat_ambush_party_slot(second, 3), -1);
}

TEST(no_able_party_member_leaves_the_order)
{
    bool none[] = { false, false, false };
    CHECK_INT(combat_ambush_party_slot(none, 3), -1);
}

void test_awareness_suite(void)
{
    RUN(awareness_is_perception_plus_intelligence);
    RUN(the_difficulty_rises_five_a_power_step);
    RUN(spotting_needs_the_difficulty_reached);
    RUN(the_ambush_moves_the_first_able_party_member_to_second);
    RUN(a_party_member_first_or_second_leaves_the_order);
    RUN(no_able_party_member_leaves_the_order);
}
