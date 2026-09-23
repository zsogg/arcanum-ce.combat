#include "game/rt_budget.h"

#include <stdio.h>
#include <string.h>

#include <tig/tig.h>

#include "game/combat.h"
#include "game/combat_observer.h"
#include "game/critter.h"
#include "game/feature.h"
#include "game/gamelib.h"
#include "game/obj.h"
#include "game/obj_pool.h"
#include "game/settings.h"
#include "game/stat.h"

// Settings a mod can tune, beside the switch in feature_list.h.
#define RT_BUDGET_PACE_KEY "real time pace ms"
#define RT_BUDGET_MOVEMENT_KEY "real time recovery pauses while moving"

// Actors that have acted recently enough to still be tracked. An actor with no
// entry is ready. A fight on the scale this is tuned for - the player, two
// followers and their enemies - uses a handful; a full table evicts a ready
// entry, never one that is recovering.
#define RT_BUDGET_MAX_ENTRIES 64

static RtBudgetEntry rt_budget_entries[RT_BUDGET_MAX_ENTRIES];
static bool rt_budget_was_turn_based;

// ---------------------------------------------------------------------------
// The rules, on one entry. Pure: no engine state is read here.
// ---------------------------------------------------------------------------

int rt_budget_duration_ms(int pace_ms, int action_points, int speed)
{
    int allowance;

    if (pace_ms <= 0 || action_points <= 0) {
        return 0;
    }

    allowance = speed > 5 ? speed : 5;
    return (int)(((long long)pace_ms * action_points + allowance / 2) / allowance);
}

void rt_budget_entry_accept(RtBudgetEntry* entry, RtBudgetCategory category,
    AnimID run, int action_points, int duration_ms)
{
    entry->state = RT_BUDGET_ACCEPTED;
    entry->category = category;
    entry->run = run;
    entry->action_points = action_points;
    entry->duration_ms = duration_ms;
    entry->elapsed_ms = 0;
}

void rt_budget_entry_release(RtBudgetEntry* entry)
{
    if (entry->state == RT_BUDGET_ACCEPTED) {
        entry->state = RT_BUDGET_RELEASED;
    }
}

bool rt_budget_entry_ready(const RtBudgetEntry* entry)
{
    // Rule 6's minimum: an action is over when its time has elapsed AND it has
    // released, so a wind-up longer than the budget simply ends at its release.
    return entry->state == RT_BUDGET_READY
        || (entry->state == RT_BUDGET_RELEASED
            && entry->elapsed_ms >= entry->duration_ms);
}

bool rt_budget_entry_tick(RtBudgetEntry* entry, int delta_ms, bool counts)
{
    if (entry->state == RT_BUDGET_READY) {
        return false;
    }

    // The wind-up counts too: the action's time runs from acceptance.
    if (counts && delta_ms > 0) {
        entry->elapsed_ms += delta_ms;
        if (entry->elapsed_ms > entry->duration_ms) {
            entry->elapsed_ms = entry->duration_ms;
        }
    }

    // Ready here even when the release came after the time had already run
    // out - a wind-up longer than the budget - so that moment is reported too.
    if (rt_budget_entry_ready(entry)) {
        entry->state = RT_BUDGET_READY;
        return true;
    }

    return false;
}

bool rt_budget_entry_turn_based_began(RtBudgetEntry* entry)
{
    if (entry->state == RT_BUDGET_ACCEPTED) {
        entry->state = RT_BUDGET_READY;
        return true;
    }

    if (entry->state == RT_BUDGET_RELEASED) {
        entry->elapsed_ms = 0;
    }

    return false;
}

// ---------------------------------------------------------------------------
// The engine side.
// ---------------------------------------------------------------------------

void rt_budget_init(void)
{
    char pace[16];

    snprintf(pace, sizeof(pace), "%d", RT_BUDGET_DEFAULT_PACE_MS);
    settings_register(&settings, RT_BUDGET_PACE_KEY, pace, NULL);
    settings_register(&settings, RT_BUDGET_MOVEMENT_KEY, "1", NULL);
    rt_budget_reset();
}

void rt_budget_reset(void)
{
    memset(rt_budget_entries, 0, sizeof(rt_budget_entries));
    rt_budget_was_turn_based = false;
}

bool rt_budget_active(void)
{
    return feature_enabled(FEATURE_REAL_TIME_BUDGET)
        && !combat_turn_based_is_active();
}

static bool rt_budget_same_run(AnimID a, AnimID b)
{
    return a.slot_num == b.slot_num && a.unique_id == b.unique_id;
}

static RtBudgetEntry* rt_budget_find(int64_t obj)
{
    int index;

    for (index = 0; index < RT_BUDGET_MAX_ENTRIES; index++) {
        if (rt_budget_entries[index].obj == obj) {
            return &(rt_budget_entries[index]);
        }
    }

    return NULL;
}

static RtBudgetEntry* rt_budget_find_or_add(int64_t obj)
{
    RtBudgetEntry* entry;
    int index;

    entry = rt_budget_find(obj);
    if (entry != NULL) {
        return entry;
    }

    // A free slot, else a ready one: a recovering actor is never evicted.
    for (index = 0; index < RT_BUDGET_MAX_ENTRIES; index++) {
        entry = &(rt_budget_entries[index]);
        if (entry->obj == OBJ_HANDLE_NULL || entry->state == RT_BUDGET_READY) {
            memset(entry, 0, sizeof(*entry));
            entry->obj = obj;
            return entry;
        }
    }

    return NULL;
}

// Which budget, if any, the goal an update is running belongs to.
static bool rt_budget_goal_category(AnimRunInfo* run_info,
    RtBudgetCategory* category)
{
    if (run_info->cur_stack_data == NULL) {
        return false;
    }

    switch (run_info->cur_stack_data->type) {
    case AG_ATTEMPT_ATTACK:
        *category = RT_BUDGET_ATTACK;
        return true;
    case AG_ATTEMPT_SPELL:
    case AG_ATTEMPT_SPELL_FRIENDLY:
    case AG_ATTEMPT_SPELL_W_CAST_ANIM:
        *category = RT_BUDGET_CAST;
        return true;
    default:
        return false;
    }
}

static int rt_budget_action_points(int64_t obj, RtBudgetCategory category)
{
    // The costs turn-based charges: `combat_attack_cost` in AGbeginAnimAttack,
    // and the flat four of `anim_pay_spell_cost`.
    return category == RT_BUDGET_ATTACK ? combat_attack_cost(obj) : 4;
}

static void rt_budget_observe(const char* event, int64_t obj,
    const RtBudgetEntry* entry)
{
    char actor[COMBAT_OBSERVER_IDENTITY_SIZE];

    if (!combat_observer_is_enabled()) {
        return;
    }

    combat_observer_object_id(obj, actor);
    combat_observer_budget(event, actor,
        entry->category == RT_BUDGET_ATTACK ? "attack" : "cast",
        entry->action_points, entry->duration_ms, entry->elapsed_ms);
}

// While holding: ask again when the recovery left is up, not a whole step later.
static void rt_budget_hold_pause(RtBudgetEntry* entry, AnimRunInfo* run_info)
{
    unsigned int remaining;

    if (!entry->held_pause_saved || !rt_budget_same_run(entry->held_run, run_info->id)) {
        entry->held_run = run_info->id;
        entry->held_pause_saved = true;
        entry->held_pause_ms = run_info->pause_time.milliseconds;
    }

    // An action still winding up has no known end; poll at the step it had.
    remaining = entry->held_pause_ms;
    if (entry->state == RT_BUDGET_RELEASED && entry->duration_ms > entry->elapsed_ms) {
        remaining = (unsigned int)(entry->duration_ms - entry->elapsed_ms);
    }
    if (remaining < 10) {
        remaining = 10;
    }
    if (remaining > entry->held_pause_ms) {
        remaining = entry->held_pause_ms;
    }
    run_info->pause_time.milliseconds = remaining;
}

// On acceptance: the run's own step delay back, if holding shortened it.
static void rt_budget_unhold(RtBudgetEntry* entry, AnimRunInfo* run_info)
{
    if (entry->held_pause_saved && rt_budget_same_run(entry->held_run, run_info->id)) {
        run_info->pause_time.milliseconds = entry->held_pause_ms;
        if (entry->held_action_flag) {
            run_info->flags |= 0x04;
        }
    }
    entry->held_pause_saved = false;
    entry->held_action_flag = false;
}

// Which action within its run this attempt is. -1 when that cannot be told -
// an attack begun before the budget was on.
static int rt_budget_action_key(RtBudgetEntry* entry, RtBudgetCategory category,
    AnimRunInfo* run_info)
{
    if (category == RT_BUDGET_CAST) {
        return run_info->cur_stack_data->params[AGDATA_SPELL_DATA].data;
    }

    return rt_budget_same_run(entry->attempt_run, run_info->id)
        ? entry->attempts_begun
        : -1;
}

bool rt_budget_cast_waiting(AnimID* anim_id)
{
    AnimRunInfo* run_info;
    RtBudgetEntry* entry;
    int index;

    if (!rt_budget_active() || !anim_id_to_run_info(anim_id, &run_info)) {
        return false;
    }

    // A cast is waiting when the run holds one that has not been accepted: the
    // one under way was, and anything else - queued at the bottom of the stack,
    // or current and held for the budget - is the one request rule 4 allows.
    entry = rt_budget_find(run_info->anim_obj);
    for (index = 0; index <= run_info->current_goal && index < 8; index++) {
        switch (run_info->goals[index].type) {
        case AG_THROW_SPELL:
        case AG_THROW_SPELL_W_CAST_ANIM:
        case AG_ATTEMPT_SPELL:
        case AG_ATTEMPT_SPELL_W_CAST_ANIM:
            if (entry == NULL
                || entry->category != RT_BUDGET_CAST
                || !rt_budget_same_run(entry->run, run_info->id)
                || entry->action_key != run_info->goals[index].params[AGDATA_SPELL_DATA].data) {
                return true;
            }
            break;
        default:
            break;
        }
    }

    return false;
}

void rt_budget_attack_begun(AnimRunInfo* run_info)
{
    RtBudgetEntry* entry;
    int64_t obj;

    if (!rt_budget_active() || run_info->cur_stack_data == NULL) {
        return;
    }

    obj = run_info->cur_stack_data->params[AGDATA_SELF_OBJ].obj;
    if (obj == OBJ_HANDLE_NULL
        || !obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        return;
    }

    entry = rt_budget_find_or_add(obj);
    if (entry != NULL) {
        entry->attempts_begun++;
        entry->attempt_run = run_info->id;
    }
}

static void rt_budget_accept(int64_t obj, RtBudgetEntry* entry,
    RtBudgetCategory category, AnimRunInfo* run_info)
{
    int pace_ms;
    int action_points;

    pace_ms = settings_get_value(&settings, RT_BUDGET_PACE_KEY);
    action_points = rt_budget_action_points(obj, category);
    // Rule 9: Speed is read now, and the action keeps it.
    rt_budget_entry_accept(entry, category, run_info->id, action_points,
        rt_budget_duration_ms(pace_ms, action_points,
            stat_level_get(obj, STAT_SPEED)));
    entry->action_key = rt_budget_action_key(entry, category, run_info);
    rt_budget_observe("action.accepted", obj, entry);
}

bool rt_budget_hold(AnimRunInfo* run_info)
{
    RtBudgetCategory category;
    RtBudgetEntry* entry;
    int64_t obj;

    if (!rt_budget_active() || !rt_budget_goal_category(run_info, &category)) {
        return false;
    }

    obj = run_info->cur_stack_data->params[AGDATA_SELF_OBJ].obj;
    if (obj == OBJ_HANDLE_NULL
        || !obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        return false;
    }

    entry = rt_budget_find_or_add(obj);
    if (entry == NULL) {
        return false;
    }

    // This action is already under way - winding up, or following through
    // after its release, even if its time has since run out. An action accepts
    // once here; only a rewind for another shot accepts again, through
    // rt_budget_hold_repeat. Otherwise a long follow-through would start a
    // second action with no release.
    if (entry->run.unique_id != 0
        && rt_budget_same_run(entry->run, run_info->id)
        && entry->action_key == rt_budget_action_key(entry, category, run_info)) {
        return false;
    }

    if (!rt_budget_entry_ready(entry)) {
        rt_budget_hold_pause(entry, run_info);
        // Holding. The next node asks whether the action frame was reached; an
        // attempt whose action frame is its first has that flag up already, and
        // must not release before it is accepted.
        if ((run_info->flags & 0x04) != 0) {
            run_info->flags &= ~0x04;
            entry->held_action_flag = true;
        }
        return true;
    }

    rt_budget_unhold(entry, run_info);
    rt_budget_accept(obj, entry, category, run_info);
    return false;
}

bool rt_budget_hold_repeat(AnimRunInfo* run_info)
{
    RtBudgetCategory category;
    RtBudgetEntry* entry;
    int64_t obj;

    if (!rt_budget_active() || !rt_budget_goal_category(run_info, &category)) {
        return false;
    }

    obj = run_info->cur_stack_data->params[AGDATA_SELF_OBJ].obj;
    entry = rt_budget_find_or_add(obj);
    if (entry == NULL) {
        return false;
    }

    // The rewind is a new shot: it waits for the last one's time, then owns
    // its own. Until then the bow stays drawn.
    if (!rt_budget_entry_ready(entry)) {
        rt_budget_hold_pause(entry, run_info);
        return true;
    }

    rt_budget_unhold(entry, run_info);
    rt_budget_accept(obj, entry, category, run_info);
    return false;
}

void rt_budget_released(AnimRunInfo* run_info)
{
    RtBudgetCategory category;
    RtBudgetEntry* entry;

    if (!rt_budget_active() || !rt_budget_goal_category(run_info, &category)) {
        return;
    }

    entry = rt_budget_find(run_info->cur_stack_data->params[AGDATA_SELF_OBJ].obj);
    if (entry != NULL && entry->state == RT_BUDGET_ACCEPTED
        && rt_budget_same_run(entry->run, run_info->id)) {
        rt_budget_entry_release(entry);
    }
}

// Rule 12: the goals that count as moving - ordered walking and running, the
// approach an attack or a cast makes, following, fleeing. Not knockback, not
// turning, not the pause between steps.
static bool rt_budget_goal_is_movement(int goal_type)
{
    switch (goal_type) {
    case AG_MOVE_TO_TILE:
    case AG_RUN_TO_TILE:
    case AG_ATTEMPT_MOVE:
    case AG_MOVE_NEAR_TILE:
    case AG_MOVE_NEAR_OBJ:
    case AG_MOVE_STRAIGHT:
    case AG_ATTEMPT_MOVE_STRAIGHT:
    case AG_CHASE:
    case AG_FOLLOW:
    case AG_FLEE:
    case AG_MOVE_AWAY_FROM_OBJ:
    case AG_RUN_NEAR_TILE:
    case AG_RUN_NEAR_OBJ:
    case AG_ATTEMPT_MOVE_NEAR:
    case AG_WANDER:
    case AG_WANDER_SEEK_DARKNESS:
    case AG_PLEASE_MOVE:
    case AG_ATTEMPT_SPREAD_OUT:
    case AG_MOVE_NEAR_OBJ_COMBAT:
    case AG_ATTEMPT_MOVE_NEAR_COMBAT:
    case AG_BACK_OFF_FROM:
        return true;
    default:
        return false;
    }
}

static bool rt_budget_is_moving(int64_t obj)
{
    AnimRunInfo* run_info;
    int slot;

    slot = anim_find_first(obj);
    if (slot == -1) {
        return false;
    }

    run_info = &(anim_run_info[slot]);
    if (run_info->current_goal < 0) {
        return false;
    }

    return rt_budget_goal_is_movement(run_info->goals[run_info->current_goal].type);
}

void rt_budget_tick(int delta_ms)
{
    RtBudgetEntry* entry;
    AnimRunInfo* run_info;
    bool turn_based;
    bool pause_while_moving;
    bool counts;
    int index;

    if (!feature_enabled(FEATURE_REAL_TIME_BUDGET)) {
        return;
    }

    // Rule 5, on the edge into turn-based: an action winding up is cancelled,
    // and a recovery will start again from zero when real time resumes.
    turn_based = combat_turn_based_is_active();
    if (turn_based && !rt_budget_was_turn_based) {
        for (index = 0; index < RT_BUDGET_MAX_ENTRIES; index++) {
            entry = &(rt_budget_entries[index]);
            if (entry->obj != OBJ_HANDLE_NULL
                && rt_budget_entry_turn_based_began(entry)) {
                rt_budget_observe("action.cancelled", entry->obj, entry);
            }
        }
    }
    rt_budget_was_turn_based = turn_based;
    if (turn_based) {
        return;
    }

    pause_while_moving = settings_get_value(&settings, RT_BUDGET_MOVEMENT_KEY) != 0;

    for (index = 0; index < RT_BUDGET_MAX_ENTRIES; index++) {
        entry = &(rt_budget_entries[index]);
        if (entry->obj == OBJ_HANDLE_NULL || entry->state == RT_BUDGET_READY) {
            continue;
        }

        if (!obj_handle_is_valid(entry->obj)
            || critter_is_dead(entry->obj)) {
            memset(entry, 0, sizeof(*entry));
            continue;
        }

        // Rule 7: an action whose animation ended before it released never
        // happened. The run is gone, or has moved on to another goal.
        if (entry->state == RT_BUDGET_ACCEPTED
            && !anim_id_to_run_info(&(entry->run), &run_info)) {
            entry->state = RT_BUDGET_READY;
            rt_budget_observe("action.cancelled", entry->obj, entry);
            continue;
        }

        counts = !(pause_while_moving && rt_budget_is_moving(entry->obj));
        if (rt_budget_entry_tick(entry, delta_ms, counts)) {
            rt_budget_observe("actor.ready", entry->obj, entry);
        }
    }
}
