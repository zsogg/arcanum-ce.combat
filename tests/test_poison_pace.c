// Poison paced by combat turns (`poison per turn`; zsogg, 2026-09-24): the two
// counting rules, pure; and the poison retune's (zsogg, 2026-10-28) damage and
// dose cap. The engine side - which turns come round, the
// real-time clock, the game-time event standing down - is exercised in
// scripted runs.

#include "game/stat.h"

#include "test_harness.h"

TEST(poison_ticks_on_every_second_turn)
{
    int turns = 0;

    CHECK(!stat_poison_turn_counts(&turns, STAT_POISON_TURNS_PER_TICK));
    CHECK(stat_poison_turn_counts(&turns, STAT_POISON_TURNS_PER_TICK));
    CHECK(!stat_poison_turn_counts(&turns, STAT_POISON_TURNS_PER_TICK));
    CHECK(stat_poison_turn_counts(&turns, STAT_POISON_TURNS_PER_TICK));
    CHECK_INT(turns, 0);
}

// `poison every turn` (zsogg, 2026-10-28): every turn ticks.
TEST(poison_every_turn_ticks_on_each_turn)
{
    int turns = 0;

    CHECK(stat_poison_turn_counts(&turns, 1));
    CHECK(stat_poison_turn_counts(&turns, 1));
    CHECK_INT(turns, 0);
}

TEST(stock_tick_damage_steps_at_200_and_550)
{
    CHECK_INT(stat_poison_tick_damage(0, false), 0);
    CHECK_INT(stat_poison_tick_damage(1, false), 1);
    CHECK_INT(stat_poison_tick_damage(199, false), 1);
    CHECK_INT(stat_poison_tick_damage(200, false), 2);
    CHECK_INT(stat_poison_tick_damage(549, false), 2);
    CHECK_INT(stat_poison_tick_damage(550, false), 3);
    CHECK_INT(stat_poison_tick_damage(1000, false), 3);
}

TEST(poison_every_turn_deals_one_plus_a_hundredth)
{
    CHECK_INT(stat_poison_tick_damage(0, true), 0);
    CHECK_INT(stat_poison_tick_damage(72, true), 1);
    CHECK_INT(stat_poison_tick_damage(186, true), 2);
    CHECK_INT(stat_poison_tick_damage(220, true), 3);
    CHECK_INT(stat_poison_tick_damage(1000, true), 11);
}

// `poison dose cap`: four times the dose, never lower than before.
TEST(a_dose_raises_the_level_to_four_times_itself)
{
    // Venom Hound bites of 12: 12, 24, 36, 48, then held at 48.
    CHECK_INT(stat_poison_dose_capped(0, 12), 12);
    CHECK_INT(stat_poison_dose_capped(36, 12), 48);
    CHECK_INT(stat_poison_dose_capped(48, 12), 48);
    // A bigger bite lifts the ceiling to its own.
    CHECK_INT(stat_poison_dose_capped(48, 15), 60);
    // A weak dose never lowers a level a strong one left.
    CHECK_INT(stat_poison_dose_capped(186, 5), 186);
    // Under the ceiling a dose adds in full.
    CHECK_INT(stat_poison_dose_capped(100, 62), 162);
    CHECK_INT(stat_poison_dose_capped(200, 62), 248);
    CHECK_INT(stat_poison_dose_capped(70, 0), 70);
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
    RUN(poison_every_turn_ticks_on_each_turn);
    RUN(stock_tick_damage_steps_at_200_and_550);
    RUN(poison_every_turn_deals_one_plus_a_hundredth);
    RUN(a_dose_raises_the_level_to_four_times_itself);
}
