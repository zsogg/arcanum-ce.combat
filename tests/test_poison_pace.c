// Poison paced by combat turns (`poison per turn`; zsogg, 2026-09-24): the two
// counting rules, pure. The engine side - which turns come round, the
// real-time clock, the game-time event standing down - is exercised in
// scripted runs.

#include "game/stat.h"

#include "test_harness.h"

TEST(poison_ticks_on_every_second_turn)
{
    int turns = 0;

    CHECK(!stat_poison_turn_counts(&turns));
    CHECK(stat_poison_turn_counts(&turns));
    CHECK(!stat_poison_turn_counts(&turns));
    CHECK(stat_poison_turn_counts(&turns));
    CHECK_INT(turns, 0);
}

TEST(poison_ticks_every_two_turn_lengths_in_real_time)
{
    unsigned int ms = 0;

    // 4 s at the stock 2000 ms turn length.
    CHECK_INT(stat_poison_time_counts(&ms, 3999, 4000), 0);
    CHECK_INT(stat_poison_time_counts(&ms, 1, 4000), 1);
    CHECK_INT(ms, 0);
    // A long frame catches up rather than losing a tick.
    CHECK_INT(stat_poison_time_counts(&ms, 9000, 4000), 2);
    CHECK_INT(ms, 1000);
    CHECK_INT(stat_poison_time_counts(&ms, 0, 4000), 0);
    CHECK_INT(stat_poison_time_counts(&ms, 100, 0), 0);
    CHECK_INT(ms, 1000);
}

void test_poison_pace_suite(void)
{
    RUN(poison_ticks_on_every_second_turn);
    RUN(poison_ticks_every_two_turn_lengths_in_real_time);
}
