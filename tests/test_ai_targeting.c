// Combat AI milestone AI-3 (`ai targeting`): when an NPC that is already
// fighting turns to a new attacker. Pure, so the rule zsogg chose on
// 2026-10-04 - commit to a nearby opponent, turn only for an adjacent or a
// clearly nearer attacker - is pinned before it is seen in a fight.

#include "game/ai.h"

#include "test_harness.h"

TEST(an_adjacent_attacker_draws_an_npc_whose_target_is_not)
{
    CHECK(ai_targeting_prefers_new_attacker(2, 1));
    CHECK(ai_targeting_prefers_new_attacker(15, 1));
}

TEST(an_adjacent_target_is_kept_against_another_adjacent_attacker)
{
    CHECK(!ai_targeting_prefers_new_attacker(1, 1));
    CHECK(!ai_targeting_prefers_new_attacker(0, 1));
}

TEST(a_clearly_nearer_attacker_draws_it)
{
    // At least three tiles nearer and at most half the distance.
    CHECK(ai_targeting_prefers_new_attacker(6, 3));
    CHECK(ai_targeting_prefers_new_attacker(10, 5));
    CHECK(ai_targeting_prefers_new_attacker(20, 2));
}

TEST(a_slightly_nearer_attacker_does_not)
{
    CHECK(!ai_targeting_prefers_new_attacker(5, 3)); // two nearer
    CHECK(!ai_targeting_prefers_new_attacker(10, 6)); // four nearer, over half
    CHECK(!ai_targeting_prefers_new_attacker(4, 4));
}

TEST(a_farther_attacker_never_does)
{
    CHECK(!ai_targeting_prefers_new_attacker(1, 5));
    CHECK(!ai_targeting_prefers_new_attacker(3, 8));
}

void test_ai_targeting_suite(void)
{
    RUN(an_adjacent_attacker_draws_an_npc_whose_target_is_not);
    RUN(an_adjacent_target_is_kept_against_another_adjacent_attacker);
    RUN(a_clearly_nearer_attacker_draws_it);
    RUN(a_slightly_nearer_attacker_does_not);
    RUN(a_farther_attacker_never_does);
}
