#include "game/rt_budget.h"

#include <stdio.h>
#include <string.h>

#include <tig/tig.h>

#include "game/combat.h"
#include "game/combat_observer.h"
#include "game/critter.h"
#include "game/feature.h"
#include "game/gamelib.h"
#include "game/item.h"
#include "game/magictech.h"
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
// Session D: switching the feature on starts every actor ready, as a load does
// (rule 3); entries frozen while it was off must not resume.
static bool rt_budget_was_enabled;
// The last out-of-range pace the log reported, so it says so once per value.
static int rt_budget_pace_reported = RT_BUDGET_DEFAULT_PACE_MS;

// ---------------------------------------------------------------------------
// The rules, on one entry. Pure: no engine state is read here.
// ---------------------------------------------------------------------------

int rt_budget_duration_ms(int pace_ms, int action_points, int action_points_per_turn)
{
    if (pace_ms <= 0 || action_points <= 0 || action_points_per_turn <= 0) {
        return 0;
    }

    return (int)(((long long)pace_ms * action_points + action_points_per_turn / 2)
        / action_points_per_turn);
}

int rt_budget_pace_ms(int configured)
{
    if (configured < RT_BUDGET_MIN_PACE_MS || configured > RT_BUDGET_MAX_PACE_MS) {
        return RT_BUDGET_DEFAULT_PACE_MS;
    }

    return configured;
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
    rt_budget_was_enabled = false;
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

    // A free slot first; only when the table is full, a ready one. A recovering
    // actor is never evicted.
    //
    // Session E: this took the first slot that was free *or* ready, so a guard
    // needing its first entry could evict the fighter's ready one with dozens
    // free - and that entry is what tells a gun's rewind from a new action.
    // Measured at 30 fps: the Revolver's third shot was then accepted as new by
    // the attack's own hold and held for good by the repeat hold, and the
    // fighter stood with his gun drawn for up to 30 s.
    for (index = 0; index < RT_BUDGET_MAX_ENTRIES; index++) {
        entry = &(rt_budget_entries[index]);
        if (entry->obj == OBJ_HANDLE_NULL) {
            memset(entry, 0, sizeof(*entry));
            entry->obj = obj;
            return entry;
        }
    }
    for (index = 0; index < RT_BUDGET_MAX_ENTRIES; index++) {
        entry = &(rt_budget_entries[index]);
        if (entry->state == RT_BUDGET_READY) {
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
    case AG_THROW_ITEM:
        *category = RT_BUDGET_THROW;
        return true;
    case AG_ATTEMPT_USE_SKILL_ON:
        *category = RT_BUDGET_SKILL;
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

static int rt_budget_action_points(int64_t obj, RtBudgetCategory category, int spell,
    int skill)
{
    // The costs turn-based charges: `combat_attack_cost` in AGbeginAnimAttack,
    // `combat_throw_cost` in the throw goal's last node (session C, zsogg's
    // option 1: whatever turn-based charges, so a change there moves both),
    // `combat_skill_cost` in `anim_run_skill_invocation`, and `combat_cast_cost`
    // in `anim_pay_spell_cost`.
    switch (category) {
    case RT_BUDGET_ATTACK:
        return combat_attack_cost(obj);
    case RT_BUDGET_THROW:
        return combat_throw_cost(obj);
    case RT_BUDGET_SKILL:
        return combat_skill_cost(obj, skill);
    default:
        return combat_cast_cost(obj, spell);
    }
}

static const char* rt_budget_category_name(RtBudgetCategory category)
{
    switch (category) {
    case RT_BUDGET_ATTACK:
        return "attack";
    case RT_BUDGET_THROW:
        return "throw";
    case RT_BUDGET_SKILL:
        return "skill";
    default:
        return "cast";
    }
}

// `reason` is NULL except on "action.cancelled" (session F).
static void rt_budget_observe(const char* event, int64_t obj,
    const RtBudgetEntry* entry, const char* reason)
{
    char actor[COMBAT_OBSERVER_IDENTITY_SIZE];

    if (!combat_observer_is_enabled()) {
        return;
    }

    combat_observer_object_id(obj, actor);
    combat_observer_budget(event, actor,
        rt_budget_category_name(entry->category),
        entry->action_points, entry->duration_ms, entry->elapsed_ms, reason);
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

// Which action within its run this attempt is: a count of attacks and casts
// begun, -1 when that cannot be told - one begun before the budget was on.
//
// Session E: casts were keyed by their spell run id, which the magic system
// reuses as soon as a spell ends. Measured in zsogg's 1500 ms playtest: a Harm
// requested 16 ms after the last one's spell freed its run got the same id,
// read as the cast already under way, and went off 250 ms after it, with no
// budget. Casts now count as they begin, as attacks do.
static int rt_budget_action_key(RtBudgetEntry* entry, RtBudgetCategory category,
    AnimRunInfo* run_info)
{
    (void)category;

    return rt_budget_same_run(entry->attempt_run, run_info->id)
        ? entry->attempts_begun
        : -1;
}

// A cast goal whose spell run still exists. When the target dies, the game
// interrupts every spell still aimed at it and frees its run - charging the
// caster, as the original does - but the casting goal plays on with nothing in
// it (measured in zsogg's playtest, 2026-09-22). Such a goal is not an action:
// the budget neither accepts it nor counts it as the one waiting request.
// A freed slot can be reused by a later spell, so this is exact only until the
// slot is taken again - the leftover goal is short-lived, which makes that rare.
static bool rt_budget_spell_live(int mt_id)
{
    MagicTechRunInfo* mt_run_info;

    return mt_id != -1 && magictech_id_to_run_info(mt_id, &mt_run_info);
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
            if (!rt_budget_spell_live(run_info->goals[index].params[AGDATA_SPELL_DATA].data)) {
                break;
            }
            if (entry == NULL
                || entry->category != RT_BUDGET_CAST
                || !rt_budget_same_run(entry->run, run_info->id)
                || entry->action_spell != run_info->goals[index].params[AGDATA_SPELL_DATA].data) {
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

// Session E: a cast's goal starts its visuals - `AGbeginAnimEyeCandy` for an
// ordinary or friendly cast, `anim_begin_previous_anim` for one with a casting
// animation - once per cast, before the step that holds it. Counted here, so a
// queued cast is a new action however the magic system numbers its runs.
void rt_budget_cast_begun(AnimRunInfo* run_info)
{
    RtBudgetCategory category;

    if (!rt_budget_active()
        || !rt_budget_goal_category(run_info, &category)
        || category != RT_BUDGET_CAST) {
        return;
    }

    rt_budget_attack_begun(run_info);
}

// `real time pace ms`, validated (session F).
static int rt_budget_configured_pace_ms(void)
{
    int configured;
    int pace_ms;

    configured = settings_get_value(&settings, RT_BUDGET_PACE_KEY);
    pace_ms = rt_budget_pace_ms(configured);
    if (pace_ms != configured && configured != rt_budget_pace_reported) {
        tig_debug_printf("rt_budget: \"%s\" is %d, outside %d..%d ms; using %d\n",
            RT_BUDGET_PACE_KEY, configured, RT_BUDGET_MIN_PACE_MS,
            RT_BUDGET_MAX_PACE_MS, pace_ms);
        rt_budget_pace_reported = configured;
    }

    return pace_ms;
}

static void rt_budget_accept(int64_t obj, RtBudgetEntry* entry,
    RtBudgetCategory category, AnimRunInfo* run_info)
{
    int pace_ms;
    int action_points;
    int spell;
    MagicTechRunInfo* mt_run_info;

    pace_ms = rt_budget_configured_pace_ms();
    spell = -1;
    if (category == RT_BUDGET_CAST
        && magictech_id_to_run_info(run_info->cur_stack_data->params[AGDATA_SPELL_DATA].data, &mt_run_info)) {
        spell = mt_run_info->spell;
    }
    action_points = rt_budget_action_points(obj, category, spell,
        category == RT_BUDGET_SKILL
            ? run_info->cur_stack_data->params[AGDATA_SKILL_DATA].data
            : -1);
    // Rule 9: Speed is read now, and the action keeps it. Divided by what a
    // turn grants, through the rule turn-based uses at each turn's start.
    rt_budget_entry_accept(entry, category, run_info->id, action_points,
        rt_budget_duration_ms(pace_ms, action_points,
            combat_action_points_per_turn(obj)));
    entry->action_key = rt_budget_action_key(entry, category, run_info);
    entry->action_spell = category == RT_BUDGET_CAST
        ? run_info->cur_stack_data->params[AGDATA_SPELL_DATA].data
        : -1;
    rt_budget_observe("action.accepted", obj, entry, NULL);
}

// Session D: a run the budget held when the budget stopped applying - the
// feature switched off mid-hold - gets its step delay and action-frame flag
// back before it goes on, or an action whose action frame is its first
// (Harm's) would never release. Turn-based beginning needs no help: it
// interrupts every goal (`anim_goal_interrupt_all_for_tb_combat`).
static void rt_budget_let_go(AnimRunInfo* run_info)
{
    RtBudgetEntry* entry;

    if (run_info->cur_stack_data == NULL) {
        return;
    }

    entry = rt_budget_find(run_info->cur_stack_data->params[AGDATA_SELF_OBJ].obj);
    if (entry != NULL
        && entry->held_pause_saved
        && rt_budget_same_run(entry->held_run, run_info->id)) {
        rt_budget_unhold(entry, run_info);
    }
}

bool rt_budget_hold(AnimRunInfo* run_info)
{
    RtBudgetCategory category;
    RtBudgetEntry* entry;
    int64_t obj;

    if (!rt_budget_active()) {
        rt_budget_let_go(run_info);
        return false;
    }

    if (!rt_budget_goal_category(run_info, &category)) {
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

    // Session C: a cast whose spell was cancelled while it waited - its target
    // died - plays out untimed. Accepting it spent a whole budget on nothing.
    // A hold is undone first: the run gets its step delay and action-frame flag
    // back, and its release then finds nothing accepted and is ignored.
    if (category == RT_BUDGET_CAST
        && !rt_budget_spell_live(run_info->cur_stack_data->params[AGDATA_SPELL_DATA].data)) {
        rt_budget_unhold(entry, run_info);
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

    if (!rt_budget_active()) {
        rt_budget_let_go(run_info);
        return false;
    }

    if (!rt_budget_goal_category(run_info, &category)) {
        return false;
    }

    obj = run_info->cur_stack_data->params[AGDATA_SELF_OBJ].obj;
    entry = rt_budget_find_or_add(obj);
    if (entry == NULL) {
        return false;
    }

    // This run's shot was accepted already - by the attack's own hold, if the
    // entry had been lost in between. Holding an accepted action would hold it
    // for good; it goes on.
    if (entry->state == RT_BUDGET_ACCEPTED && rt_budget_same_run(entry->run, run_info->id)) {
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

// A melee attack whose target is out of reach, by the game's own test. Melee
// is a weapon of range 1 or none, as `anim_target_in_weapon_range_of` reads it.
static bool rt_budget_is_whiff(int64_t obj, int64_t target_obj)
{
    int64_t weapon_obj;

    weapon_obj = combat_critter_weapon(obj);
    if (weapon_obj != OBJ_HANDLE_NULL && item_weapon_range(weapon_obj, obj) > 1) {
        return false;
    }

    return !anim_target_in_weapon_range_of(obj, target_obj);
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
        // Session D, zsogg's decision (2026-09-23): a melee swing whose target
        // is out of reach at its strike frame is a whiff, and a whiff costs no
        // recovery - rule 7, nothing happened. The game asks the same question
        // one node later and then applies no damage and charges no AP; the
        // budget had been charging a full recovery for it. Measured before:
        // with recovery pausing while moving, guard-beta whiffed 4 of 8
        // swings, against 1 of 14 in stock. Ranged attacks are unchanged.
        if (category == RT_BUDGET_ATTACK
            && rt_budget_is_whiff(run_info->cur_stack_data->params[AGDATA_SELF_OBJ].obj,
                run_info->cur_stack_data->params[AGDATA_TARGET_OBJ].obj)) {
            entry->state = RT_BUDGET_READY;
            rt_budget_observe("action.cancelled", entry->obj, entry, "whiff");
            return;
        }
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
        rt_budget_was_enabled = false;
        return;
    }

    // Session D: on the edge into enabled, everyone starts ready.
    if (!rt_budget_was_enabled) {
        memset(rt_budget_entries, 0, sizeof(rt_budget_entries));
        rt_budget_was_enabled = true;
    }

    // Rule 5, on the edge into turn-based: an action winding up is cancelled,
    // and a recovery will start again from zero when real time resumes.
    turn_based = combat_turn_based_is_active();
    if (turn_based && !rt_budget_was_turn_based) {
        for (index = 0; index < RT_BUDGET_MAX_ENTRIES; index++) {
            entry = &(rt_budget_entries[index]);
            if (entry->obj != OBJ_HANDLE_NULL
                && rt_budget_entry_turn_based_began(entry)) {
                rt_budget_observe("action.cancelled", entry->obj, entry, "turn_based");
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
            rt_budget_observe("action.cancelled", entry->obj, entry, "unreleased");
            continue;
        }

        counts = !(pause_while_moving && rt_budget_is_moving(entry->obj));
        if (rt_budget_entry_tick(entry, delta_ms, counts)) {
            rt_budget_observe("actor.ready", entry->obj, entry, NULL);
        }
    }
}

// ---------------------------------------------------------------------------
// The chase stopgap (rebalance, zsogg 2026-09-24). See rt_budget.h.
// ---------------------------------------------------------------------------

RtChaseVerdict rt_chase_decide(bool applies, bool target_in_reach,
    bool target_moving)
{
    if (!applies) {
        return RT_CHASE_SWING;
    }

    // Out of reach already - it stepped off between the goal's range check
    // and the first frame - is given up whether or not it is still moving:
    // a swing begun now could only strike air.
    if (!target_in_reach) {
        return RT_CHASE_GIVE_UP;
    }

    // Moving at all, not only away (zsogg, 2026-09-24, after his playtest):
    // waiting only for steps that led away still let 7 swings in 37 begin
    // while he circled the guard a tile off and then stepped out of reach.
    return target_moving ? RT_CHASE_WAIT : RT_CHASE_SWING;
}

RtChaseVerdict rt_chase_check(AnimRunInfo* run_info)
{
    int64_t obj;
    int64_t target_obj;
    int64_t weapon_obj;
    RtChaseVerdict verdict;
    RtBudgetEntry* entry;

    if (!feature_enabled(FEATURE_REAL_TIME_CHASE)
        || combat_turn_based_is_active()
        || run_info->cur_stack_data == NULL
        || run_info->cur_stack_data->type != AG_ATTEMPT_ATTACK) {
        return RT_CHASE_SWING;
    }

    obj = run_info->cur_stack_data->params[AGDATA_SELF_OBJ].obj;
    target_obj = run_info->cur_stack_data->params[AGDATA_TARGET_OBJ].obj;
    if (obj == OBJ_HANDLE_NULL
        || target_obj == OBJ_HANDLE_NULL
        || obj_field_int32_get(obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return RT_CHASE_SWING;
    }

    // A melee weapon, as `anim_has_ranged_weapon` tells one; and a swing
    // still in its first frame - once the blade moves, it finishes.
    weapon_obj = combat_critter_weapon(obj);
    if (item_weapon_range(weapon_obj, obj) > 1
        || tig_art_id_frame_get(obj_field_int32_get(obj, OBJ_F_CURRENT_AID)) != 0) {
        return RT_CHASE_SWING;
    }

    verdict = rt_chase_decide(true,
        anim_target_in_weapon_range_of(obj, target_obj),
        rt_budget_is_moving(target_obj));

    // Giving up ends this run's attempt: anything the budget's hold changed
    // on the run goes back, as when the budget lets a held action go.
    if (verdict == RT_CHASE_GIVE_UP) {
        entry = rt_budget_find(obj);
        if (entry != NULL) {
            rt_budget_unhold(entry, run_info);
        }
    }

    return verdict;
}
