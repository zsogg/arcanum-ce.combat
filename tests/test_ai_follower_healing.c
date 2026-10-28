// Combat AI milestone AI-6 (`follower healing`): the pure parts of how a
// follower of the player heals. Stock tries itself, then its leader, then the
// other followers, taking the first that yields an action; zsogg chose on
// 2026-10-28 the worst hurt first. Stock waives the 38% failure refusal for
// White Necromancy on a patient at 30% or below; zsogg kept the waiver but
// cut it off at 75%.

#include "game/ai.h"

#include "test_harness.h"

TEST(the_worst_hurt_patient_comes_first)
{
    // Itself, the player, two followers.
    const int hp[] = { 80, 40, 100, 15 };
    int order[4];

    ai_heal_patient_order(hp, 4, order);
    CHECK_INT(order[0], 3);
    CHECK_INT(order[1], 1);
    CHECK_INT(order[2], 0);
    CHECK_INT(order[3], 2);
}

TEST(ties_keep_the_stock_order)
{
    // Itself and the player equally hurt: itself first, as stock.
    const int hp[] = { 50, 50, 90, 50 };
    int order[4];

    ai_heal_patient_order(hp, 4, order);
    CHECK_INT(order[0], 0);
    CHECK_INT(order[1], 1);
    CHECK_INT(order[2], 3);
    CHECK_INT(order[3], 2);
}

TEST(a_dead_patient_comes_before_the_living)
{
    const int hp[] = { 60, 0, 30 };
    int order[3];

    ai_heal_patient_order(hp, 3, order);
    CHECK_INT(order[0], 1);
    CHECK_INT(order[1], 2);
    CHECK_INT(order[2], 0);
}

TEST(a_lone_patient_is_itself)
{
    const int hp[] = { 20 };
    int order[1];

    ai_heal_patient_order(hp, 1, order);
    CHECK_INT(order[0], 0);
}

TEST(the_failure_refusal_is_38_unless_stock_waives_it)
{
    CHECK_INT(ai_failure_refusal_threshold(false, false), 38);
    CHECK_INT(ai_failure_refusal_threshold(false, true), 38);
    // Stock: a waived White heal is never refused for its chance.
    CHECK_INT(ai_failure_refusal_threshold(true, false), -1);
}

TEST(follower_healing_refuses_a_waived_heal_from_75)
{
    CHECK_INT(ai_failure_refusal_threshold(true, true), 75);
}

void test_ai_follower_healing_suite(void)
{
    RUN(the_worst_hurt_patient_comes_first);
    RUN(ties_keep_the_stock_order);
    RUN(a_dead_patient_comes_before_the_living);
    RUN(a_lone_patient_is_itself);
    RUN(the_failure_refusal_is_38_unless_stock_waives_it);
    RUN(follower_healing_refuses_a_waived_heal_from_75);
}
