#include "game/determinism.h"

#include <tig/debug.h>
#include <tig/guid.h>
#include <tig/mouse.h>
#include <tig/timer.h>

#include "game/gamelib.h"
#include "game/random.h"
#include "game/settings.h"

// The default virtual frame step. 16 ms is the nominal 60 Hz frame the rest
// of the engine is tuned around; it is a step size, not a frame rate cap, so
// the run goes as fast as the machine allows and only the reported time is
// fixed.
#define DETERMINISM_DEFAULT_STEP 16

// A seed of 0 means "not configured". The value is otherwise arbitrary; it is
// recorded here so a harness can reproduce a run from the log line below
// without having to know what the default was.
#define DETERMINISM_DEFAULT_SEED 1

static bool determinism_on;
static unsigned int determinism_active_seed;
static bool determinism_requested;
static unsigned int determinism_requested_seed;

void determinism_request(unsigned int seed)
{
    determinism_requested = true;
    if (seed != 0) {
        determinism_requested_seed = seed;
    }
}

void determinism_init(void)
{
    unsigned int seed;
    int step;

    settings_register(&settings, DETERMINISM_KEY, "0", NULL);
    settings_register(&settings, DETERMINISM_SEED_KEY, "0", NULL);
    settings_register(&settings, DETERMINISM_STEP_KEY, "0", NULL);

    determinism_on = determinism_requested
        || settings_get_value(&settings, DETERMINISM_KEY) != 0;
    if (!determinism_on) {
        return;
    }

    seed = (unsigned int)settings_get_value(&settings, DETERMINISM_SEED_KEY);
    if (seed == 0) {
        seed = determinism_requested_seed;
    }
    if (seed == 0) {
        seed = DETERMINISM_DEFAULT_SEED;
    }

    step = settings_get_value(&settings, DETERMINISM_STEP_KEY);
    if (step <= 0) {
        step = DETERMINISM_DEFAULT_STEP;
    }

    determinism_active_seed = seed;

    // `random_init` has already seeded from the clock by the time this runs;
    // overwriting it here is deliberate, and keeps the faithful init path
    // untouched for every run that does not ask for this.
    random_seed(seed);
    tig_guid_seed_set(seed);
    tig_timer_virtual_set(true, (unsigned int)step);

    // The pointer is ambient state, not input: it is always somewhere, and
    // the engine reads it every frame. Resting near the window edge it
    // scrolls the map, which draws more terrain and moves the call count of
    // everything on the drawing path; over an object it runs target picking.
    // Measured 2026-08-21 against the trace fixture: 7 function-membership
    // changes and 311 changed counts from the pointer alone, reproducible
    // enough that two runs agreed byte for byte and it read as a real
    // regression.
    tig_mouse_freeze();

    // Print it. A reproducible run is only reproducible if the log says what
    // to reproduce it with.
    tig_debug_printf("Determinism: on, seed %u, virtual frame step %d ms, "
                     "pointer pinned\n",
        seed,
        step);
}

bool determinism_enabled(void)
{
    return determinism_on;
}

unsigned int determinism_seed(void)
{
    return determinism_active_seed;
}
