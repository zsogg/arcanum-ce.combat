// Quirk 68, `interrupted casts end` (2026-11-02): a cast interrupted while it
// is still `AG_THROW_SPELL` keeps its spell's run active on the target in
// stock, because the goal's cancel entry restores the stand animation and
// releases nothing. Under the switch `anim_interrupt` releases the run, unpaid.
//
// `anim_interrupted_cast_end`, which `anim_interrupt` calls for each goal it
// cancels, over a hand-built run slot (the whole interrupt needs a running
// anim system; the in-game run is in quirk 68). `magictech_release` is
// wrapped (`ld --wrap`, tests/CMakeLists.txt) so the release is seen without a
// spell system behind it.

#include <string.h>

#include "game/anim.h"
#include "game/anim_private.h"
#include "game/feature.h"
#include "game/gamelib.h"
#include "game/settings.h"
#include "test_harness.h"

void __real_magictech_release(int mt_id, unsigned int flags);

static bool release_armed;
static int released_id;
static unsigned int released_flags;
static int released_count;

void __wrap_magictech_release(int mt_id, unsigned int flags)
{
    if (!release_armed) {
        __real_magictech_release(mt_id, flags);
        return;
    }
    released_id = mt_id;
    released_flags = flags;
    released_count++;
}

// One goal on a run slot, carrying spell run 7.
static AnimRunInfo cast_run;

static bool interrupt(int goal, unsigned int extra_flags, bool on)
{
    memset(&cast_run, 0, sizeof(cast_run));
    cast_run.flags = 0x01 | extra_flags;
    cast_run.current_goal = 1;
    cast_run.goals[0].type = AG_ANIMATE;
    cast_run.goals[0].params[AGDATA_SPELL_DATA].data = 7;
    cast_run.goals[1].type = goal;
    cast_run.goals[1].params[AGDATA_SPELL_DATA].data = 7;

    settings_init(&settings, "unused");
    feature_init();
    release_armed = true;
    released_id = -1;
    released_flags = 0xFF;
    released_count = 0;
    settings_set_value(&settings, feature_key(FEATURE_INTERRUPTED_CASTS_END), on ? 1 : 0);
    anim_interrupted_cast_end(&cast_run, 1);
    release_armed = false;
    settings_exit(&settings);
    return released_count > 0;
}

TEST(an_interrupted_throw_spell_keeps_its_run_stock_and_releases_it_under_the_switch)
{
    CHECK(!interrupt(AG_THROW_SPELL, 0, false));
    CHECK(interrupt(AG_THROW_SPELL, 0, true));
    CHECK_INT(released_id, 7);
    CHECK_INT((int)released_flags, 0);
    CHECK_INT(released_count, 1);
    // Every goal on the slot that named the run forgets it.
    CHECK_INT(cast_run.goals[0].params[AGDATA_SPELL_DATA].data, -1);
    CHECK_INT(cast_run.goals[1].params[AGDATA_SPELL_DATA].data, -1);
    CHECK(interrupt(AG_THROW_SPELL_W_CAST_ANIM, 0, true));
}

TEST(a_begun_spell_and_an_attempt_are_left_to_their_own_cleanup)
{
    // Begun (0x08): the spell's run ends itself.
    CHECK(!interrupt(AG_THROW_SPELL, 0x08, true));
    // AG_ATTEMPT_SPELL's own cancel entry, `anim_cleanup_spell`, releases it.
    CHECK(!interrupt(AG_ATTEMPT_SPELL, 0, true));
}

void test_interrupted_cast_suite(void)
{
    RUN(an_interrupted_throw_spell_keeps_its_run_stock_and_releases_it_under_the_switch);
    RUN(a_begun_spell_and_an_attempt_are_left_to_their_own_cleanup);
}
