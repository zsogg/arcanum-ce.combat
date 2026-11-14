// Quirk 75, `body spell turn end` (2026-11-14, LG1): a turn-based critter
// standing still is free to end its turn when its only busy goal is its
// fidget (`anim_ready_to_end_turn`, which the End Turn button and a group
// turn read).
// Stock asks whether the critter's first slot is the fidget, so a concurrent
// goal in an earlier slot - a body spell's eye candy - hid the fidget and the
// turn could never be ended. Over hand-built run slots; the in-game run is in
// quirk 75.

#include <string.h>

#include "game/anim.h"
#include "game/anim_private.h"
#include "game/feature.h"
#include "game/gamelib.h"
#include "game/settings.h"
#include "test_harness.h"

#define OBJ ((int64_t)0x1234)

static AnimRunInfo saved[2];
static int saved_high_water;

static void slot(int index, int goal)
{
    AnimRunInfo* run_info = &(anim_run_info[index]);

    memset(run_info, 0, sizeof(*run_info));
    run_info->id.slot_num = index;
    run_info->id.unique_id = 100 + index;
    run_info->flags = 0x01;
    run_info->current_goal = 0;
    run_info->anim_obj = OBJ;
    run_info->goals[0].type = goal;
}

static bool idle(int first, int second, bool on)
{
    bool result;

    memcpy(saved, anim_run_info, sizeof(saved));
    saved_high_water = anim_run_info_high_water;
    slot(0, first);
    slot(1, second);
    anim_run_info_high_water = 2;

    settings_init(&settings, "unused");
    feature_init();
    settings_set_value(&settings, feature_key(FEATURE_BODY_SPELL_TURN_END), on ? 1 : 0);
    result = anim_ready_to_end_turn(OBJ);
    settings_exit(&settings);

    memcpy(anim_run_info, saved, sizeof(saved));
    anim_run_info_high_water = saved_high_water;
    return result;
}

TEST(eye_candy_before_the_fidget_blocks_the_turn_stock_and_not_under_the_switch)
{
    CHECK(!idle(AG_EYE_CANDY, AG_ANIM_FIDGET, false));
    CHECK(idle(AG_EYE_CANDY, AG_ANIM_FIDGET, true));
}

TEST(a_fidget_in_the_first_slot_is_idle_either_way)
{
    CHECK(idle(AG_ANIM_FIDGET, AG_EYE_CANDY, false));
    CHECK(idle(AG_ANIM_FIDGET, AG_EYE_CANDY, true));
}

TEST(an_attack_behind_eye_candy_is_busy_either_way)
{
    CHECK(!idle(AG_EYE_CANDY, AG_ATTACK, false));
    CHECK(!idle(AG_EYE_CANDY, AG_ATTACK, true));
}

void test_body_spell_turn_end_suite(void)
{
    RUN(eye_candy_before_the_fidget_blocks_the_turn_stock_and_not_under_the_switch);
    RUN(a_fidget_in_the_first_slot_is_idle_either_way);
    RUN(an_attack_behind_eye_candy_is_busy_either_way);
}
