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

void __real_magictech_missile_cancelled(int mt_id);

static bool cancel_armed;
static int cancelled_id;

void __wrap_magictech_missile_cancelled(int mt_id)
{
    if (!cancel_armed) {
        __real_magictech_missile_cancelled(mt_id);
        return;
    }
    cancelled_id = mt_id;
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

// A spell run freed while cast goals still name it: the run slot is reused
// by a later cast, which a leftover goal then began as its own (the
// demonstration battle, 2026-11-08). Under the switch, every spell goal
// naming the freed run forgets it; other goals' data in the slot, and goals
// naming other runs or on slots not in use, are left alone.
static void run_freed(bool on, AnimRunInfo* used, AnimRunInfo* other, AnimRunInfo* idle)
{
    memset(used, 0, sizeof(*used));
    used->flags = 0x01;
    used->current_goal = 2;
    used->goals[0].type = AG_ATTEMPT_USE_PICKPOCKET_SKILL_ON;
    used->goals[0].params[AGDATA_SPELL_DATA].data = 7;
    used->goals[1].type = AG_THROW_SPELL;
    used->goals[1].params[AGDATA_SPELL_DATA].data = 7;
    used->goals[2].type = AG_ATTEMPT_SPELL;
    used->goals[2].params[AGDATA_SPELL_DATA].data = 7;
    memset(other, 0, sizeof(*other));
    other->flags = 0x01;
    other->current_goal = 0;
    other->goals[0].type = AG_SHOOT_SPELL;
    other->goals[0].params[AGDATA_SPELL_DATA].data = 8;
    memset(idle, 0, sizeof(*idle));
    idle->current_goal = 0;
    idle->goals[0].type = AG_ATTEMPT_SPELL;
    idle->goals[0].params[AGDATA_SPELL_DATA].data = 7;

    settings_init(&settings, "unused");
    feature_init();
    settings_set_value(&settings, feature_key(FEATURE_INTERRUPTED_CASTS_END), on ? 1 : 0);
    anim_spell_run_freed(7);
    settings_exit(&settings);
}

TEST(a_freed_spell_run_is_forgotten_by_the_cast_goals_naming_it_under_the_switch)
{
    AnimRunInfo saved[3];
    AnimRunInfo* slots = &(anim_run_info[ANIM_RUN_INFO_COUNT - 3]);
    int saved_high_water = anim_run_info_high_water;

    memcpy(saved, slots, sizeof(saved));
    // The slots are filled by hand, not allocated, so the search bound that
    // allocation raises (card HW1) is raised over them here.
    anim_run_info_note_used(ANIM_RUN_INFO_COUNT - 1);

    run_freed(false, &slots[0], &slots[1], &slots[2]);
    CHECK_INT(slots[0].goals[1].params[AGDATA_SPELL_DATA].data, 7);
    CHECK_INT(slots[0].goals[2].params[AGDATA_SPELL_DATA].data, 7);

    run_freed(true, &slots[0], &slots[1], &slots[2]);
    CHECK_INT(slots[0].goals[0].params[AGDATA_SPELL_DATA].data, 7); // pickpocketing's own data
    CHECK_INT(slots[0].goals[1].params[AGDATA_SPELL_DATA].data, -1);
    CHECK_INT(slots[0].goals[2].params[AGDATA_SPELL_DATA].data, -1);
    CHECK_INT(slots[1].goals[0].params[AGDATA_SPELL_DATA].data, 8); // another run
    CHECK_INT(slots[2].goals[0].params[AGDATA_SPELL_DATA].data, 7); // a slot not in use

    memcpy(slots, saved, sizeof(saved));
    anim_run_info_high_water = saved_high_water;
}

// `ai waits for its missiles` (zsogg, 2026-11-12): a thrower's spell is in
// flight while a slot in use holds a shoot-spell goal naming it as parent.
TEST(a_spell_is_in_flight_while_its_missile_names_the_thrower)
{
    AnimRunInfo saved;
    AnimRunInfo* slot = &(anim_run_info[ANIM_RUN_INFO_COUNT - 1]);
    int saved_high_water = anim_run_info_high_water;
    int64_t thrower = 0x1234;

    memcpy(&saved, slot, sizeof(saved));
    anim_run_info_note_used(ANIM_RUN_INFO_COUNT - 1);

    memset(slot, 0, sizeof(*slot));
    slot->flags = 0x01;
    slot->current_goal = 1;
    slot->goals[0].type = AG_ANIMATE;
    slot->goals[1].type = AG_SHOOT_SPELL;
    slot->goals[1].params[AGDATA_PARENT_OBJ].obj = thrower;
    CHECK(anim_spell_in_flight_from(thrower));
    CHECK(!anim_spell_in_flight_from(0x5678));

    // Landed: the goal is popped.
    slot->current_goal = 0;
    CHECK(!anim_spell_in_flight_from(thrower));

    // A slot being freed (0x02) or not in use holds nothing in flight.
    slot->current_goal = 1;
    slot->flags = 0x01 | 0x02;
    CHECK(!anim_spell_in_flight_from(thrower));
    slot->flags = 0;
    CHECK(!anim_spell_in_flight_from(thrower));

    memcpy(slot, &saved, sizeof(saved));
    anim_run_info_high_water = saved_high_water;
}

// `missile spells paid at launch` (zsogg, 2026-11-12): a missile destroyed in
// flight forgets its spell run (refunded and freed by
// `magictech_missile_cancelled`); stock left the run named, never begun.
TEST(a_destroyed_missile_forgets_its_run_under_the_switch)
{
    AnimRunInfo missile;

    memset(&missile, 0, sizeof(missile));
    missile.flags = 0x01;
    missile.current_goal = 0;
    missile.goals[0].type = AG_SHOOT_SPELL;

    settings_init(&settings, "unused");
    feature_init();
    cancel_armed = true;
    cancelled_id = -1;
    missile.goals[0].params[AGDATA_SPELL_DATA].data = 9;
    anim_interrupted_cast_end(&missile, 0);
    CHECK_INT(missile.goals[0].params[AGDATA_SPELL_DATA].data, 9);
    CHECK_INT(cancelled_id, -1);

    settings_set_value(&settings, feature_key(FEATURE_MISSILE_PAID_AT_LAUNCH), 1);
    anim_interrupted_cast_end(&missile, 0);
    CHECK_INT(missile.goals[0].params[AGDATA_SPELL_DATA].data, -1);
    CHECK_INT(cancelled_id, 9);
    cancel_armed = false;
    settings_exit(&settings);
}

void test_interrupted_cast_suite(void)
{
    RUN(a_destroyed_missile_forgets_its_run_under_the_switch);
    RUN(a_spell_is_in_flight_while_its_missile_names_the_thrower);
    RUN(a_freed_spell_run_is_forgotten_by_the_cast_goals_naming_it_under_the_switch);
    RUN(an_interrupted_throw_spell_keeps_its_run_stock_and_releases_it_under_the_switch);
    RUN(a_begun_spell_and_an_attempt_are_left_to_their_own_cleanup);
}
