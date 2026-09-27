// Reload prototype session 2: the turn-based work, pure - what one Reload
// command does with the AP a turn has left. The rules file is
// `test_weapon_rules.c`. The engine side - loaded shots on the weapon, the Q key, the
// shot - is exercised in scripted runs.

#include "game/reload.h"

#include "test_harness.h"

TEST(a_reload_spends_only_what_the_turn_has)
{
    ReloadWork work;

    // The Flintlock untrained, 4 AP, after firing with 8: one AP left.
    work = reload_work(4, 0, 1);
    CHECK_INT(work.spent, 1);
    CHECK_INT(work.progress, 1);
    CHECK(!work.completed);

    // Next turn, the three still owed, with 8 to spend.
    work = reload_work(4, 1, 8);
    CHECK_INT(work.spent, 3);
    CHECK(work.completed);
    CHECK_INT(work.progress, 0);

    // Exactly enough.
    work = reload_work(3, 0, 3);
    CHECK_INT(work.spent, 3);
    CHECK(work.completed);

    // Nothing to spend: no work, no shot.
    work = reload_work(4, 2, 0);
    CHECK_INT(work.spent, 0);
    CHECK_INT(work.progress, 2);
    CHECK(!work.completed);
}

TEST(work_already_past_a_lowered_cost_completes_for_nothing)
{
    ReloadWork work;

    // 3 AP stored toward an untrained 4, then Apprentice training lowers the
    // cost to 3: the shot is finished without spending more.
    work = reload_work(3, 3, 5);
    CHECK_INT(work.spent, 0);
    CHECK(work.completed);
}

void test_reload_suite(void)
{
    RUN(a_reload_spends_only_what_the_turn_has);
    RUN(work_already_past_a_lowered_cost_completes_for_nothing);
}
