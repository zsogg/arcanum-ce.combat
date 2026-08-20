// TIMER
// ---
//
// The TIMER subsystem provides a couple of types and functions to deal with
// time points which usually represent engine or system-level events.
//
// This subsystem is not used for in-game events (instead it have it's own
// notion of game time, which is not a part of TIG).

#include "tig/timer.h"

#include <limits.h>

// CE: the virtual clock. See the comment in tig/timer.h.
static bool tig_timer_virtual;
static unsigned int tig_timer_virtual_step = 16;
static unsigned int tig_timer_virtual_ticks;

static unsigned int tig_timer_ticks(void)
{
    return tig_timer_virtual
        ? tig_timer_virtual_ticks
        : (unsigned int)(SDL_GetTicks() & UINT_MAX);
}

// CE: not in the original - see tig/timer.h.
void tig_timer_virtual_set(bool enabled, unsigned int step_ms)
{
    tig_timer_virtual = enabled;

    // A zero step would freeze the clock, and a frozen clock is not
    // determinism - every "has N ms passed" test would answer no forever and
    // the game would never advance. Refuse it rather than hang.
    tig_timer_virtual_step = step_ms != 0 ? step_ms : 16;

    // Start from a non-zero value. Timestamp zero is indistinguishable from
    // "never set" in several callers that keep a `tig_timestamp_t` field and
    // treat 0 as unset.
    tig_timer_virtual_ticks = tig_timer_virtual_step;
}

// CE: not in the original - see tig/timer.h.
bool tig_timer_virtual_enabled(void)
{
    return tig_timer_virtual;
}

// CE: not in the original - see tig/timer.h.
void tig_timer_virtual_advance(void)
{
    if (tig_timer_virtual) {
        tig_timer_virtual_ticks += tig_timer_virtual_step;
    }
}

// 0x52DF80
int tig_timer_init(TigInitInfo* init_info)
{
    (void)init_info;

    return TIG_OK;
}

// 0x52DF90
void tig_timer_exit(void)
{
}

// 0x52DFA0
int tig_timer_now(tig_timestamp_t* timestamp_ptr)
{
    *timestamp_ptr = tig_timer_ticks();
    return TIG_OK;
}

// 0x52DFB0
tig_duration_t tig_timer_elapsed(tig_timestamp_t start)
{
    return tig_timer_ticks() - start;
}

// 0x52DFC0
tig_duration_t tig_timer_between(tig_timestamp_t start, tig_timestamp_t end)
{
    unsigned int diff = end - start;
    if (diff > INT_MAX) {
        diff = INT_MAX;
    }
    return diff;
}
