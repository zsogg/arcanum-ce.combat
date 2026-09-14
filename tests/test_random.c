// Tests for `game/random.c`.
//
// The whole game's chance lives here: to-hit rolls, damage, loot, critical
// tables, reaction rolls, world-map encounters. It is a 4-line linear
// congruential generator, which makes it exhaustively testable - and makes
// *exactness* the thing worth testing, because a faithful port has to reproduce
// Troika's sequence, not merely produce random numbers. A generator that is
// statistically fine but numerically different would rebalance the game and
// break `-norandom` reproducibility, and nothing else in the tree would notice.
//
// So the golden vectors below were computed independently of this codebase,
// from the recurrence in the source comment (Numerical Recipes 2nd ed., ch 7.1,
// "An Even Quicker Generator"):
//
//     state = 1664525 * state + 1013904223   (mod 2^32)
//     result = (state >> 8) & 0x7FFF
//
// Those vectors were then checked against the exe itself, so this suite pins
// Troika's sequence and not merely CE's. 0x4CEF50 is six instructions:
//
//     mov  eax, ds:0x601778        ; random_prev_value
//     imul eax, eax, 0x19660d      ; 1664525
//     add  eax, 0x3c6ef35f         ; 1013904223
//     mov  ds:0x601778, eax
//     shr  eax, 8
//     and  eax, 0x7fff
//
// CE's transcription is bit-exact, which is what lets a CE save, a CE replay
// and the original agree on every roll.
//
// If someone swaps in Mersenne Twister - which the file's own NOTE floats as a
// possibility, following ToEE - these fail, which is the point: that has to be
// a deliberate change behind a switch, not a silent one.

#include <limits.h>

#include "game/random.h"

#include "test_harness.h"

// Independently computed from the recurrence above, not captured from a run.
static const int SEED_1_FIRST_8[] = { 2137, 2181, 5633, 29498, 28781, 6337, 22237, 25183 };
static const int SEED_0_FIRST_8[] = { 28403, 20521, 19702, 31059, 21221, 11974, 32364, 22879 };
static const int SEED_12345_FIRST_8[] = { 14620, 15482, 3138, 2322, 30641, 19017, 29339, 24727 };

static void check_sequence(unsigned int seed, const int* expected, int count)
{
    int i;

    random_seed(seed);

    for (i = 0; i < count; i++) {
        CHECK_INT(random_rand(), expected[i]);
    }
}

TEST(the_generator_matches_its_documented_recurrence)
{
    check_sequence(1, SEED_1_FIRST_8, 8);
    check_sequence(0, SEED_0_FIRST_8, 8);
    check_sequence(12345, SEED_12345_FIRST_8, 8);
}

TEST(a_seed_reproduces_its_sequence_exactly)
{
    // What `-norandom` and any replay or savegame determinism rests on.
    int first[32];
    int i;
    bool same = true;

    random_seed(987654321u);
    for (i = 0; i < 32; i++) {
        first[i] = random_rand();
    }

    random_seed(987654321u);
    for (i = 0; i < 32; i++) {
        if (random_rand() != first[i]) {
            same = false;
        }
    }

    CHECK(same);
}

TEST(different_seeds_diverge)
{
    int a;
    int b;

    random_seed(1);
    a = random_rand();

    random_seed(2);
    b = random_rand();

    CHECK(a != b);
}

TEST(rand_stays_inside_fifteen_bits)
{
    // `(state >> 8) & 0x7FFF` - so 0..32767 inclusive, never negative even
    // though the return type is signed. Callers use the result as an array
    // index and in `%` arithmetic, so a negative would be immediate corruption.
    int i;
    int value;
    int out_of_range = 0;
    int negatives = 0;

    random_seed(1);

    for (i = 0; i < 200000; i++) {
        value = random_rand();
        if (value < 0) {
            negatives++;
        }
        if (value > 32767) {
            out_of_range++;
        }
    }

    CHECK_INT(negatives, 0);
    CHECK_INT(out_of_range, 0);
}

TEST(rand_reaches_both_ends_of_its_range)
{
    // A generator that never returns 0 or 32767 would be subtly wrong in a way
    // the range check above cannot see. Over 200k draws both must appear.
    int i;
    int value;
    bool saw_zero = false;
    bool saw_max = false;

    random_seed(1);

    for (i = 0; i < 200000; i++) {
        value = random_rand();
        if (value == 0) {
            saw_zero = true;
        }
        if (value == 32767) {
            saw_max = true;
        }
    }

    CHECK(saw_zero);
    CHECK(saw_max);
}

// --- card O2: capture and restore, which is what replay stands on ---------

TEST(the_captured_state_resumes_the_sequence_it_was_taken_from)
{
    int before[6];
    int after[6];
    unsigned int captured;
    int i;

    // Run a little way in, so the captured state is mid-sequence rather than
    // the seed itself - that is the case a Ready barrier actually produces.
    random_seed(20260913u);
    for (i = 0; i < 4; i++) {
        (void)random_rand();
    }

    captured = random_state_get();
    for (i = 0; i < 6; i++) {
        before[i] = random_rand();
    }

    // Somebody else's run happens in between, moving the generator.
    random_seed(1u);
    for (i = 0; i < 50; i++) {
        (void)random_rand();
    }

    random_seed(captured);
    for (i = 0; i < 6; i++) {
        after[i] = random_rand();
    }

    for (i = 0; i < 6; i++) {
        CHECK_INT(after[i], before[i]);
    }
}

TEST(capturing_the_state_does_not_move_it)
{
    unsigned int first;
    unsigned int second;
    int drawn;

    random_seed(4242u);
    first = random_state_get();
    second = random_state_get();
    CHECK_INT((int)first, (int)second);

    // And it is genuinely the live state, not a stale copy: one draw moves it.
    drawn = random_rand();
    (void)drawn;
    CHECK(random_state_get() != first);
}

TEST(the_state_is_the_seed_the_generator_was_given)
{
    // The whole reason capture is one integer. If this ever stops holding,
    // `random_seed` has grown a transform and O2's restore path is wrong.
    random_seed(123456789u);
    CHECK_INT((int)random_state_get(), (int)123456789u);
}

TEST(seed_generate_returns_the_seed_it_installed)
{
    // The return value is the only way a caller can reproduce the run it just
    // started, so it has to be the value actually seeded, not a copy taken
    // before or after.
    unsigned int seed;
    int first;
    int again;

    seed = random_seed_generate();
    first = random_rand();

    random_seed(seed);
    again = random_rand();

    CHECK_INT(first, again);
}

TEST(between_is_inclusive_of_both_bounds)
{
    int i;
    int value;
    bool saw_lower = false;
    bool saw_upper = false;
    int out_of_range = 0;

    random_seed(1);

    for (i = 0; i < 20000; i++) {
        value = random_between(3, 7);
        if (value < 3 || value > 7) {
            out_of_range++;
        }
        if (value == 3) {
            saw_lower = true;
        }
        if (value == 7) {
            saw_upper = true;
        }
    }

    CHECK_INT(out_of_range, 0);
    // Both ends reachable - an exclusive upper bound is the classic off-by-one
    // here, and it would quietly remove the best roll from every table.
    CHECK(saw_lower);
    CHECK(saw_upper);
}

TEST(between_with_equal_bounds_returns_that_value)
{
    random_seed(1);

    CHECK_INT(random_between(5, 5), 5);
    CHECK_INT(random_between(0, 0), 0);
    CHECK_INT(random_between(-4, -4), -4);
}

TEST(between_with_equal_bounds_does_not_consume_a_number)
{
    // Worth pinning: the equal-bounds case returns before touching the
    // generator, so it does not advance the state. Anything that reproduces a
    // sequence - a replay, a seeded encounter table - depends on the number of
    // draws, not just the seed, so this is part of the contract.
    int expected;
    int actual;

    random_seed(4242);
    expected = random_rand();

    random_seed(4242);
    random_between(9, 9);
    random_between(-1, -1);
    actual = random_rand();

    CHECK_INT(actual, expected);
}

TEST(between_swaps_reversed_bounds)
{
    // Called with the larger value first it still yields the same inclusive
    // range rather than misbehaving, which callers that compute bounds from
    // stats rely on.
    int i;
    int value;
    int out_of_range = 0;
    bool saw_lower = false;
    bool saw_upper = false;

    random_seed(7);

    for (i = 0; i < 20000; i++) {
        value = random_between(7, 3);
        if (value < 3 || value > 7) {
            out_of_range++;
        }
        if (value == 3) {
            saw_lower = true;
        }
        if (value == 7) {
            saw_upper = true;
        }
    }

    CHECK_INT(out_of_range, 0);
    CHECK(saw_lower);
    CHECK(saw_upper);
}

TEST(between_handles_negative_and_straddling_ranges)
{
    int i;
    int value;
    int out_of_range = 0;

    random_seed(11);

    for (i = 0; i < 20000; i++) {
        value = random_between(-10, -5);
        if (value < -10 || value > -5) {
            out_of_range++;
        }
    }
    CHECK_INT(out_of_range, 0);

    for (i = 0; i < 20000; i++) {
        value = random_between(-3, 3);
        if (value < -3 || value > 3) {
            out_of_range++;
        }
    }
    CHECK_INT(out_of_range, 0);
}

TEST(between_covers_every_value_of_a_small_range)
{
    // A d6. Every face must come up, or some outcome in a table is unreachable.
    int seen[6] = { 0, 0, 0, 0, 0, 0 };
    int i;
    int value;
    int missing = 0;

    random_seed(1);

    for (i = 0; i < 20000; i++) {
        value = random_between(1, 6);
        if (value >= 1 && value <= 6) {
            seen[value - 1]++;
        }
    }

    for (i = 0; i < 6; i++) {
        if (seen[i] == 0) {
            missing++;
        }
    }

    CHECK_INT(missing, 0);
}

TEST(between_cannot_reach_past_32767_values_above_its_lower_bound)
{
    // A real limitation, pinned rather than fixed. `random_between` is
    // `lower + random_rand() % size`, and `random_rand()` tops out at 32767, so
    // for a range wider than 32768 the top of it is unreachable - the roll is
    // uniform over the first 32768 values and never beyond. Faithful, and fine
    // for the dice-sized ranges the game actually uses, but it is a trap for
    // any new caller that asks for a wide range.
    int i;
    int value;
    int above = 0;

    random_seed(1);

    for (i = 0; i < 50000; i++) {
        value = random_between(0, 1000000);
        if (value > 32767) {
            above++;
        }
    }

    CHECK_INT(above, 0);
}

void test_random_suite(void)
{
    RUN(the_generator_matches_its_documented_recurrence);
    RUN(a_seed_reproduces_its_sequence_exactly);
    RUN(different_seeds_diverge);
    RUN(rand_stays_inside_fifteen_bits);
    RUN(rand_reaches_both_ends_of_its_range);
    RUN(the_captured_state_resumes_the_sequence_it_was_taken_from);
    RUN(capturing_the_state_does_not_move_it);
    RUN(the_state_is_the_seed_the_generator_was_given);
    RUN(seed_generate_returns_the_seed_it_installed);

    RUN(between_is_inclusive_of_both_bounds);
    RUN(between_with_equal_bounds_returns_that_value);
    RUN(between_with_equal_bounds_does_not_consume_a_number);
    RUN(between_swaps_reversed_bounds);
    RUN(between_handles_negative_and_straddling_ranges);
    RUN(between_covers_every_value_of_a_small_range);
    RUN(between_cannot_reach_past_32767_values_above_its_lower_bound);
}
