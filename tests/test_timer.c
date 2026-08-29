// Tests for `tig/timer.c` - the engine clock.
//
// 90 lines. It had one emudiff case (`tig_timer_between`) and is executed by
// `test_determinism.c`, neither of which is a suite named for it. Written
// 2026-08-29.
//
// **The division of labour with `test_determinism.c` is deliberate.** That
// suite owns the CE-only virtual clock as its *subject* - it exists to prove the
// clock can be driven reproducibly. This one owns the three **transcribed**
// functions, and uses the virtual clock as its *instrument*: with it running,
// `tig_timer_now` answers a value these tests choose, so the wrap and clamp
// behaviour can be checked at exact boundaries rather than near whatever
// `SDL_GetTicks` happens to return.
//
// **The asymmetry worth knowing.** `tig_timer_between` clamps at `INT_MAX`;
// `tig_timer_elapsed` does not clamp at all. Both are faithful - the original
// clamps with an *unsigned* compare at 0x52DFCA (`cmp eax,0x7fffffff; jbe`) and
// its `tig_timer_elapsed` at 0x52DFB0 is two instructions, `call` then `sub`,
// with no test. So a caller that measures a span across a wrap gets `INT_MAX`
// from one and a value near 2^32 from the other.

#include <limits.h>

#include "tig/timer.h"

#include "test_harness.h"

// Restore the clock to the state every other suite expects: off. Nothing in the
// test binary enables it except `test_determinism.c`, which also turns it off.
static void timer_end(void)
{
    tig_timer_virtual_set(false, 0);
    CHECK_INT(tig_timer_virtual_enabled(), 0);
}

TEST(init_and_exit_are_no_ops_that_report_success)
{
    CHECK_INT(tig_timer_init(NULL), TIG_OK);
    tig_timer_exit();
    // Still usable afterwards: neither function keeps state.
    CHECK_INT(tig_timer_init(NULL), TIG_OK);
}

// `tig_timer_between(start, end)` is `end - start` as an unsigned difference,
// clamped to INT_MAX. The clamp is what stops a backwards or wrapped span
// reading as an enormous positive duration.
TEST(between_measures_a_forward_span)
{
    CHECK_INT(tig_timer_between(0, 0), 0);
    CHECK_INT(tig_timer_between(10, 20), 10);
    CHECK_INT(tig_timer_between(0, 1000), 1000);
    CHECK_INT(tig_timer_between(4294967295u, 4294967295u), 0);
}

TEST(between_clamps_at_int_max_rather_than_reporting_a_huge_span)
{
    // Exactly INT_MAX is not clamped - the compare is `> INT_MAX`.
    CHECK_INT(tig_timer_between(0, (tig_timestamp_t)INT_MAX), INT_MAX);
    // One past it is.
    CHECK_INT(tig_timer_between(0, (tig_timestamp_t)INT_MAX + 1u), INT_MAX);
}

// A backwards span is not negative and not small: the unsigned subtraction
// makes it enormous, and the clamp turns it into INT_MAX.
TEST(a_backwards_span_clamps_rather_than_going_negative)
{
    CHECK_INT(tig_timer_between(20, 10), INT_MAX);
    CHECK_INT(tig_timer_between(1, 0), INT_MAX);
}

// A span that crosses the 32-bit wrap is measured correctly as long as it is
// under INT_MAX - the unsigned subtraction handles the wrap on its own.
TEST(a_span_across_the_wrap_is_measured_correctly)
{
    CHECK_INT(tig_timer_between(4294967290u, 9u), 15);
    CHECK_INT(tig_timer_between(4294967295u, 0u), 1);
}

// The instrument: with the virtual clock running, `tig_timer_now` answers a
// value this suite chooses.
TEST(now_reads_the_virtual_clock_and_advances_by_its_step)
{
    tig_timestamp_t first;
    tig_timestamp_t second;

    tig_timer_virtual_set(true, 25);
    CHECK_INT(tig_timer_virtual_enabled(), 1);

    CHECK_INT(tig_timer_now(&first), TIG_OK);
    // `virtual_set` starts at one step, never at zero - several callers treat a
    // zero timestamp as "never set".
    CHECK_INT(first, 25);

    tig_timer_virtual_advance();
    CHECK_INT(tig_timer_now(&second), TIG_OK);
    CHECK_INT(second, 50);

    CHECK_INT(tig_timer_between(first, second), 25);

    timer_end();
}

// Reading the clock must not move it. A `now` that advanced would make every
// duration in the engine depend on how often it was measured.
TEST(reading_the_clock_does_not_advance_it)
{
    tig_timestamp_t a;
    tig_timestamp_t b;
    tig_timestamp_t c;

    tig_timer_virtual_set(true, 16);
    tig_timer_now(&a);
    tig_timer_now(&b);
    tig_timer_now(&c);

    CHECK_INT(a, b);
    CHECK_INT(b, c);

    timer_end();
}

TEST(elapsed_measures_from_a_start_to_now)
{
    tig_timestamp_t start;

    tig_timer_virtual_set(true, 10);
    tig_timer_now(&start);

    CHECK_INT(tig_timer_elapsed(start), 0);

    tig_timer_virtual_advance();
    CHECK_INT(tig_timer_elapsed(start), 10);

    tig_timer_virtual_advance();
    tig_timer_virtual_advance();
    CHECK_INT(tig_timer_elapsed(start), 30);

    timer_end();
}

// **`elapsed` does NOT clamp**, where `between` does. Faithful: 0x52DFB0 is
// `call` then `sub` and nothing else. A start in the future gives a value near
// 2^32 rather than INT_MAX, so the two functions disagree on the same span.
TEST(elapsed_does_not_clamp_where_between_does)
{
    tig_timestamp_t now;
    tig_timestamp_t future;

    tig_timer_virtual_set(true, 10);
    tig_timer_now(&now);

    // A start 100 ticks ahead of the clock.
    future = now + 100;

    // `between` clamps this to INT_MAX...
    CHECK_INT(tig_timer_between(future, now), INT_MAX);
    // ...while `elapsed` hands back the raw unsigned difference.
    CHECK_INT((unsigned int)tig_timer_elapsed(future), 4294967196u);

    timer_end();
}

// A zero step is refused and coerced to 16. A frozen clock is not determinism:
// every "has N ms passed" test would answer no forever. `test_determinism.c`
// owns the virtual clock generally; this pins the coercion as the *timer's*
// contract, because it is what stops `tig_timer_elapsed` returning 0 for ever.
TEST(a_zero_virtual_step_is_refused_rather_than_freezing_the_clock)
{
    tig_timestamp_t first;
    tig_timestamp_t second;

    tig_timer_virtual_set(true, 0);
    tig_timer_now(&first);
    CHECK_INT(first, 16);

    tig_timer_virtual_advance();
    tig_timer_now(&second);
    CHECK_INT(second, 32);
    CHECK(second > first);

    timer_end();
}

// With the clock off, advancing does nothing and the real clock is read. The
// only safe assertion about `SDL_GetTicks` is that it does not go backwards.
TEST(with_the_virtual_clock_off_the_real_clock_is_read)
{
    tig_timestamp_t a;
    tig_timestamp_t b;

    tig_timer_virtual_set(false, 0);
    CHECK_INT(tig_timer_virtual_enabled(), 0);

    tig_timer_now(&a);
    tig_timer_virtual_advance();
    tig_timer_now(&b);

    CHECK(b >= a);
    // Advancing a disabled clock must not jump it by a step.
    CHECK(tig_timer_between(a, b) < 1000);
}

void test_timer_suite(void)
{
    RUN(init_and_exit_are_no_ops_that_report_success);
    RUN(between_measures_a_forward_span);
    RUN(between_clamps_at_int_max_rather_than_reporting_a_huge_span);
    RUN(a_backwards_span_clamps_rather_than_going_negative);
    RUN(a_span_across_the_wrap_is_measured_correctly);
    RUN(now_reads_the_virtual_clock_and_advances_by_its_step);
    RUN(reading_the_clock_does_not_advance_it);
    RUN(elapsed_measures_from_a_start_to_now);
    RUN(elapsed_does_not_clamp_where_between_does);
    RUN(a_zero_virtual_step_is_refused_rather_than_freezing_the_clock);
    RUN(with_the_virtual_clock_off_the_real_clock_is_read);
}
