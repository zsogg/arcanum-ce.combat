// Tests for the virtual clock and the reproducible-run switch.
//
// B0 from the 2026-08-20 roadmap. Three wall-clock reads decide what a run
// does - the RNG seed, the GUID seed, and TIG's timer - and the last is the
// one with teeth, because the game advances its own clock from it. The
// save-state matrix recorded the consequence twice: `Slot0003 turnbased-mid`
// and `Slot0006 random encounter` both differed from the original only
// because time passed between the two engines reaching their save point.
//
// The virtual clock is what makes a run depend on how many frames executed
// rather than on how long they took. These tests pin its contract; the
// end-to-end proof that two CE runs of one save agree byte for byte lives in
// `arcanum-work/harness/determinism-check.sh`.

#include <tig/timer.h>

#include "game/random.h"

#include "test_harness.h"

TEST(virtual_clock_is_off_by_default)
{
    // The faithful path must be untouched: nothing turns this on unless a run
    // asks for it.
    CHECK_INT(tig_timer_virtual_enabled(), 0);
}

TEST(virtual_clock_advances_only_when_told)
{
    tig_timestamp_t first;
    tig_timestamp_t second;

    tig_timer_virtual_set(true, 16);
    tig_timer_now(&first);
    tig_timer_now(&second);

    // Reading the time must not move it. Two reads inside one frame see one
    // instant, which is the property real elapsed-time reads do not have.
    CHECK_INT((int)first, (int)second);

    tig_timer_virtual_advance();
    tig_timer_now(&second);
    CHECK_INT((int)(second - first), 16);

    tig_timer_virtual_set(false, 0);
}

TEST(virtual_clock_elapsed_matches_frames_advanced)
{
    tig_timestamp_t start;
    int index;

    tig_timer_virtual_set(true, 20);
    tig_timer_now(&start);

    for (index = 0; index < 5; index++) {
        tig_timer_virtual_advance();
    }

    CHECK_INT((int)tig_timer_elapsed(start), 100);

    tig_timer_virtual_set(false, 0);
}

TEST(virtual_clock_restarts_from_the_same_point)
{
    tig_timestamp_t first_run;
    tig_timestamp_t second_run;
    int index;

    // Two runs of the same length must report the same time. This is the
    // whole property: reproducibility, not just monotonicity.
    tig_timer_virtual_set(true, 16);
    for (index = 0; index < 7; index++) {
        tig_timer_virtual_advance();
    }
    tig_timer_now(&first_run);

    tig_timer_virtual_set(true, 16);
    for (index = 0; index < 7; index++) {
        tig_timer_virtual_advance();
    }
    tig_timer_now(&second_run);

    CHECK_INT((int)first_run, (int)second_run);

    tig_timer_virtual_set(false, 0);
}

TEST(virtual_clock_never_starts_at_zero)
{
    tig_timestamp_t now;

    // Several callers keep a `tig_timestamp_t` field and treat 0 as "unset".
    // A clock that starts at 0 would make the first frame indistinguishable
    // from never having run.
    tig_timer_virtual_set(true, 16);
    tig_timer_now(&now);
    CHECK(now != 0);

    tig_timer_virtual_set(false, 0);
}

TEST(virtual_clock_refuses_a_zero_step)
{
    tig_timestamp_t start;
    tig_timestamp_t after;

    // A frozen clock is not determinism: every "has N ms passed" test would
    // answer no forever and the game would never advance. A zero step falls
    // back to the default rather than hanging.
    tig_timer_virtual_set(true, 0);
    tig_timer_now(&start);
    tig_timer_virtual_advance();
    tig_timer_now(&after);
    CHECK(after > start);

    tig_timer_virtual_set(false, 0);
}

TEST(seeded_rng_replays_exactly)
{
    int first[16];
    int second[16];
    int index;

    // The other half of reproducibility. `random_seed` is the original's own
    // 0x4CEED0; this only asserts that seeding it twice the same way gives
    // the same stream, which is what a replayed run depends on.
    random_seed(12345);
    for (index = 0; index < 16; index++) {
        first[index] = random_between(0, 1000);
    }

    random_seed(12345);
    for (index = 0; index < 16; index++) {
        second[index] = random_between(0, 1000);
    }

    for (index = 0; index < 16; index++) {
        CHECK_INT(second[index], first[index]);
    }
}

TEST(different_seeds_diverge)
{
    int first;
    int second;

    // Guards against the degenerate pass: a stream that ignores its seed
    // would satisfy the test above perfectly.
    random_seed(1);
    first = random_between(0, 1000000);

    random_seed(2);
    second = random_between(0, 1000000);

    CHECK(first != second);
}

void test_determinism_suite(void)
{
    RUN(virtual_clock_is_off_by_default);
    RUN(virtual_clock_advances_only_when_told);
    RUN(virtual_clock_elapsed_matches_frames_advanced);
    RUN(virtual_clock_restarts_from_the_same_point);
    RUN(virtual_clock_never_starts_at_zero);
    RUN(virtual_clock_refuses_a_zero_step);
    RUN(seeded_rng_replays_exactly);
    RUN(different_seeds_diverge);
}
