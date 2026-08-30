// Tests for `tig/core.c` - subsystem startup, shutdown and the frame tick.
//
// 168 lines. Written 2026-08-29.
//
// **Most of it cannot be reached from here, and that is stated rather than
// worked around.** `tig_init` opens SDL with `SDL_INIT_AUDIO | SDL_INIT_VIDEO |
// SDL_INIT_EVENTS` and then brings up all 21 subsystems including `video`,
// `window` and `art` - a display, an audio device and the game's assets. The
// test binary deliberately has none of those. What `tig_init` does is instead
// covered by every run of the real engine, and by the smoke step in `gate.sh`,
// which reaches `player_create`.
//
// So this suite owns the three things that *are* reachable and are not
// incidental: the active flag, the re-entrancy guard on `tig_exit`, and
// `tig_ping` - which is the frame tick, and the one piece of `core.c` CE
// changed rather than transcribed.
//
// **`tig_ping` is why determinism works at all.** CE added
// `tig_timer_virtual_advance()` as its first statement, so the virtual clock
// moves exactly one step per frame, before anything reads the time. Get that
// wrong - advance twice, advance after the read, advance when the clock is off
// - and two runs of one save stop agreeing. `test_determinism.c` measures the
// consequence over 16 modules; these cases pin the mechanism.

#include <string.h>

#include "tig/core.h"
#include "tig/message.h"
#include "tig/timer.h"

#include "test_harness.h"

#define CORE_DRAIN_LIMIT 256

// `tig_ping` enqueues a PING every frame, so any case that pings owns a queue
// and has to give it back. `tig_message_init` reassigns the heads without
// freeing them (faithful - 0x52B6E1/0x52B6E7), so init must be paired with exit
// or the nodes are simply orphaned; ASan reported 1,280 bytes from these cases
// before this bracketing existed.
static void core_begin(void)
{
    TigMessage drain;
    int count = 0;

    tig_message_init(NULL);
    while (count < CORE_DRAIN_LIMIT && tig_message_dequeue(&drain) == TIG_OK) {
        count++;
    }
}

static void core_end(void)
{
    TigMessage drain;
    int count = 0;

    while (count < CORE_DRAIN_LIMIT && tig_message_dequeue(&drain) == TIG_OK) {
        count++;
    }
    tig_message_exit();

    tig_timer_virtual_set(false, 0);
    // Leave the flag as every other suite expects to find it. `tig_mouse_ping`
    // does real work when active, and `tig_ping` calls it.
    tig_set_active(false);
}

TEST(the_active_flag_round_trips)
{
    core_begin();

    // Nothing in the test binary has set it, and the static starts false.
    CHECK_INT(tig_get_active(), 0);

    tig_set_active(true);
    CHECK_INT(tig_get_active(), 1);

    tig_set_active(false);
    CHECK_INT(tig_get_active(), 0);

    // Setting the same value twice is not a toggle.
    tig_set_active(true);
    tig_set_active(true);
    CHECK_INT(tig_get_active(), 1);

    core_end();
}

// `tig_exit` is registered with `atexit` *and* callable directly, so it can be
// entered twice. It is guarded on `tig_initialized`, which is false here
// because `tig_init` never ran - so calling it must be a no-op rather than
// tearing down subsystems the suites are still using.
TEST(exit_without_init_is_a_no_op)
{
    TigMessage sent;
    TigMessage got;

    // The observable is a message left in the queue across the call. A real
    // teardown runs `tig_message_exit`, which frees the queue outright - so if
    // the message survives, the teardown did not happen.
    //
    // An earlier version of this case asserted only that the active flag still
    // round-tripped, which a full teardown does not disturb; the mutation that
    // deletes the guard passed it. Measured 2026-08-29.
    core_begin();
    memset(&sent, 0, sizeof(sent));
    sent.type = TIG_MESSAGE_QUIT;
    sent.data.quit.exit_code = 1234;
    tig_message_enqueue(&sent);

    tig_exit();
    tig_exit();

    CHECK_INT(tig_message_dequeue(&got), TIG_OK);
    CHECK_INT(got.type, TIG_MESSAGE_QUIT);
    CHECK_INT(got.data.quit.exit_code, 1234);

    tig_set_active(true);
    CHECK_INT(tig_get_active(), 1);
    tig_set_active(false);

    core_end();
}

// The frame tick publishes the time it read into `tig_ping_timestamp`, which is
// the timestamp every message enqueued during that frame carries.
TEST(ping_publishes_the_time_it_read)
{
    tig_timestamp_t now;

    core_begin();
    tig_timer_virtual_set(true, 16);

    tig_ping();
    CHECK_INT(tig_timer_now(&now), TIG_OK);
    CHECK_INT(tig_ping_timestamp, now);

    core_end();
}

// **One step per frame, and the advance comes first.** The clock starts at one
// step after `virtual_set`; `tig_ping` advances before reading, so the first
// ping publishes two steps, not one.
TEST(ping_advances_the_virtual_clock_exactly_once)
{
    tig_timestamp_t start;

    core_begin();
    tig_timer_virtual_set(true, 16);
    tig_timer_now(&start);
    CHECK_INT(start, 16);

    tig_ping();
    CHECK_INT(tig_ping_timestamp, 32);

    tig_ping();
    CHECK_INT(tig_ping_timestamp, 48);

    tig_ping();
    CHECK_INT(tig_ping_timestamp, 64);

    // Three frames, three steps - not six, and not two.
    CHECK_INT(tig_timer_between(start, tig_ping_timestamp), 48);

    core_end();
}

// The step is whatever was asked for, so a different frame budget produces a
// proportionally different clock. This is what `-deterministic=<seed>` relies on.
TEST(ping_advances_by_the_configured_step)
{
    core_begin();
    tig_timer_virtual_set(true, 25);

    tig_ping();
    CHECK_INT(tig_ping_timestamp, 50);
    tig_ping();
    CHECK_INT(tig_ping_timestamp, 75);

    core_end();
}

// With the virtual clock off, `tig_ping` must not manufacture time - it reads
// the real one. The only safe assertion about `SDL_GetTicks` is that it does
// not go backwards and does not leap.
TEST(ping_does_not_advance_a_disabled_clock)
{
    tig_timestamp_t first;
    tig_timestamp_t second;

    core_begin();
    tig_timer_virtual_set(false, 0);

    tig_ping();
    first = tig_ping_timestamp;
    tig_ping();
    second = tig_ping_timestamp;

    CHECK(second >= first);
    // A virtual step would show up as a fixed 16ms jump per call; a real clock
    // across two back-to-back pings moves by almost nothing.
    CHECK(tig_timer_between(first, second) < 1000);

    core_end();
}

// Pings keep working after `tig_exit` has been called, because `tig_exit` is a
// no-op without a matching `tig_init`. Worth pinning: it is the property that
// makes the guard above safe for the rest of the suite run.
TEST(ping_still_works_after_a_no_op_exit)
{
    core_begin();
    tig_timer_virtual_set(true, 16);

    tig_exit();
    tig_ping();
    CHECK_INT(tig_ping_timestamp, 32);

    core_end();
}

void test_core_suite(void)
{
    RUN(the_active_flag_round_trips);
    RUN(exit_without_init_is_a_no_op);
    RUN(ping_publishes_the_time_it_read);
    RUN(ping_advances_the_virtual_clock_exactly_once);
    RUN(ping_advances_by_the_configured_step);
    RUN(ping_does_not_advance_a_disabled_clock);
    RUN(ping_still_works_after_a_no_op_exit);
}
