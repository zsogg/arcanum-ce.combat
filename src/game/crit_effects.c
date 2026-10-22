#include "game/crit_effects.h"

#include <stdio.h>
#include <string.h>

#include "game/combat.h"
#include "game/critter.h"
#include "game/feature.h"
#include "game/gamelib.h"
#include "game/obj.h"
#include "game/obj_pool.h"
#include "game/player.h"
#include "game/rt_budget.h"
#include "game/tf.h"

// Characters with a lasting critical effect. A fight uses a handful; a full
// table replaces the record applied longest ago.
#define CRIT_EFFECTS_MAX_ENTRIES 64

static CritEffectEntry crit_effects_entries[CRIT_EFFECTS_MAX_ENTRIES];
static unsigned int crit_effects_clock_ms;

static const struct {
    unsigned int effect;
    const char* name;
    const char* floating;
} crit_effects_table[] = {
    { CRIT_EFFECT_WINDED, "winded", "Winded" },
    { CRIT_EFFECT_NUMBED_ARM, "numbed_arm", "Numbed arm" },
    { CRIT_EFFECT_HOBBLED, "hobbled", "Hobbled" },
    { CRIT_EFFECT_STAGGERED, "staggered", "Staggered" },
    { CRIT_EFFECT_DAZZLED, "dazzled", "Dazzled" },
    { CRIT_EFFECT_EXPOSED, "exposed", "Exposed" },
    { CRIT_EFFECT_UNFOCUSED, "unfocused", "Unfocused" },
    { CRIT_EFFECT_MALFUNCTION, "malfunction", "Malfunction" },
    { CRIT_EFFECT_DISRUPTED, "disrupted", "Disrupted" },
    { CRIT_EFFECT_JAMMED_ARM, "jammed_arm", "Jammed arm" },
};

// MG-2: what attacks against an Exposed critter are made easier by.
#define CRIT_EFFECT_EXPOSED_BONUS 20

// MG-2: the critter whose next stun is long, if any.
static int64_t crit_effects_long_stun_obj;

// ---------------------------------------------------------------------------
// The rules, on one record. Pure: no engine state is read here.
// ---------------------------------------------------------------------------

unsigned int crit_effects_for_location(int hit_loc)
{
    switch (hit_loc) {
    case HIT_LOC_HEAD:
        return CRIT_EFFECT_STAGGERED;
    case HIT_LOC_ARM:
        return CRIT_EFFECT_NUMBED_ARM;
    case HIT_LOC_LEG:
        return CRIT_EFFECT_HOBBLED;
    default:
        return CRIT_EFFECT_WINDED;
    }
}

void crit_effects_entry_apply(CritEffectEntry* entry, unsigned int effects, unsigned int clock_ms)
{
    crit_effects_entry_apply_for(entry, effects, 1, clock_ms);
}

void crit_effects_entry_apply_for(CritEffectEntry* entry, unsigned int effects, int turns,
    unsigned int clock_ms)
{
    // Winded is spent when it lands; the rest last. A new effect refreshes
    // the record, so everything it holds runs to the end of the next turn -
    // or, for a debuff's critical (MG-2), the one after; a refresh never
    // shortens what is left.
    if (entry->effects == 0 || entry->turn_started || entry->turns < turns) {
        entry->turns = turns;
    }
    entry->effects |= effects & ~CRIT_EFFECT_WINDED;
    entry->turn_started = false;
    entry->applied_ms = clock_ms;
}

int crit_effects_entry_to_be_hit_bonus(const CritEffectEntry* entry)
{
    return (entry->effects & CRIT_EFFECT_EXPOSED) != 0 ? CRIT_EFFECT_EXPOSED_BONUS : 0;
}

bool crit_effects_entry_unfocused(const CritEffectEntry* entry)
{
    return (entry->effects & CRIT_EFFECT_UNFOCUSED) != 0;
}

int crit_effects_entry_to_hit_penalty(const CritEffectEntry* entry)
{
    return (entry->effects & (CRIT_EFFECT_NUMBED_ARM | CRIT_EFFECT_DAZZLED | CRIT_EFFECT_JAMMED_ARM)) != 0
        ? CRIT_EFFECT_TO_HIT_PENALTY
        : 0;
}

int crit_effects_entry_move_multiplier(const CritEffectEntry* entry, bool turn_based)
{
    return turn_based && (entry->effects & CRIT_EFFECT_HOBBLED) != 0 ? 2 : 1;
}

int crit_effects_entry_turn_action_points(const CritEffectEntry* entry, int action_points)
{
    // Only the turn that begins after it was dealt is halved.
    if (!entry->turn_started
        && (entry->effects & (CRIT_EFFECT_STAGGERED | CRIT_EFFECT_MALFUNCTION | CRIT_EFFECT_DISRUPTED)) != 0) {
        return action_points / 2;
    }

    return action_points;
}

void crit_effects_entry_turn_started(CritEffectEntry* entry)
{
    entry->turn_started = true;
}

bool crit_effects_entry_turn_ended(CritEffectEntry* entry)
{
    if (!entry->turn_started) {
        return false;
    }

    // MG-2: a two-turn record outlives the first turn that ends.
    if (entry->turns > 1) {
        entry->turns--;
        entry->turn_started = false;
        return false;
    }

    return true;
}

bool crit_effects_entry_lapsed(const CritEffectEntry* entry, bool turn_based,
    unsigned int clock_ms, int window_ms)
{
    int turns = entry->turns > 1 ? entry->turns : 1;

    return !turn_based && clock_ms - entry->applied_ms >= (unsigned int)(window_ms * turns);
}

// ---------------------------------------------------------------------------
// The engine side.
// ---------------------------------------------------------------------------

static CritEffectEntry* crit_effects_find(int64_t obj)
{
    int index;

    for (index = 0; index < CRIT_EFFECTS_MAX_ENTRIES; index++) {
        if (crit_effects_entries[index].obj == obj) {
            return &(crit_effects_entries[index]);
        }
    }

    return NULL;
}

static CritEffectEntry* crit_effects_add(int64_t obj)
{
    CritEffectEntry* entry;
    int index;

    entry = crit_effects_find(OBJ_HANDLE_NULL);
    if (entry == NULL) {
        entry = &(crit_effects_entries[0]);
        for (index = 1; index < CRIT_EFFECTS_MAX_ENTRIES; index++) {
            if (crit_effects_clock_ms - crit_effects_entries[index].applied_ms
                > crit_effects_clock_ms - entry->applied_ms) {
                entry = &(crit_effects_entries[index]);
            }
        }
    }

    memset(entry, 0, sizeof(*entry));
    entry->obj = obj;
    return entry;
}

int crit_effects_names(unsigned int effects, char* buffer, int size)
{
    int used = 0;
    size_t index;

    if (size <= 0) {
        return 0;
    }

    buffer[0] = '\0';
    for (index = 0; index < sizeof(crit_effects_table) / sizeof(crit_effects_table[0]); index++) {
        if ((effects & crit_effects_table[index].effect) != 0) {
            int written = snprintf(buffer + used, (size_t)(size - used), "%s\"%s\"",
                used > 0 ? "," : "", crit_effects_table[index].name);
            if (written < 0 || written >= size - used) {
                break;
            }
            used += written;
        }
    }

    return used;
}

bool crit_effects_enabled(void)
{
    return feature_enabled(FEATURE_CRITICAL_EFFECTS) || feature_enabled(FEATURE_SPELL_CRITICALS);
}

void crit_effects_reset(void)
{
    memset(crit_effects_entries, 0, sizeof(crit_effects_entries));
    crit_effects_clock_ms = 0;
    crit_effects_long_stun_obj = OBJ_HANDLE_NULL;
}

// Floats each effect over its victim and keeps the lasting ones.
void crit_effects_apply(int64_t obj, unsigned int effects)
{
    crit_effects_apply_for(obj, effects, 1);
}

void crit_effects_apply_for(int64_t obj, unsigned int effects, int turns)
{
    CritEffectEntry* entry;
    int tf_type;
    size_t index;

    if (!crit_effects_enabled() || effects == 0) {
        return;
    }

    // The colours `combat_dmg` gives its own lines about this target.
    if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_PC) {
        tf_type = TF_TYPE_RED;
    } else if (critter_pc_leader_get(obj) == player_get_local_pc_obj()) {
        tf_type = TF_TYPE_YELLOW;
    } else {
        tf_type = TF_TYPE_WHITE;
    }
    for (index = 0; index < sizeof(crit_effects_table) / sizeof(crit_effects_table[0]); index++) {
        if ((effects & crit_effects_table[index].effect) != 0) {
            tf_add(obj, tf_type, crit_effects_table[index].floating);
        }
    }

    if ((effects & ~CRIT_EFFECT_WINDED) == 0) {
        return;
    }

    entry = crit_effects_find(obj);
    if (entry == NULL) {
        entry = crit_effects_add(obj);
    }
    crit_effects_entry_apply_for(entry, effects, turns, crit_effects_clock_ms);
}

int crit_effects_to_be_hit_bonus(int64_t obj)
{
    CritEffectEntry* entry;

    if (!crit_effects_enabled()) {
        return 0;
    }

    entry = crit_effects_find(obj);
    return entry != NULL ? crit_effects_entry_to_be_hit_bonus(entry) : 0;
}

bool crit_effects_unfocused(int64_t obj)
{
    CritEffectEntry* entry;

    if (!crit_effects_enabled()) {
        return false;
    }

    entry = crit_effects_find(obj);
    return entry != NULL && crit_effects_entry_unfocused(entry);
}

void crit_effects_mark_long_stun(int64_t obj)
{
    crit_effects_long_stun_obj = obj;
}

bool crit_effects_take_long_stun(int64_t obj)
{
    if (obj == OBJ_HANDLE_NULL || crit_effects_long_stun_obj != obj) {
        return false;
    }

    crit_effects_long_stun_obj = OBJ_HANDLE_NULL;
    return true;
}

int crit_effects_to_hit_penalty(int64_t obj)
{
    CritEffectEntry* entry;

    if (!crit_effects_enabled()) {
        return 0;
    }

    entry = crit_effects_find(obj);
    return entry != NULL ? crit_effects_entry_to_hit_penalty(entry) : 0;
}

int crit_effects_move_multiplier(int64_t obj)
{
    CritEffectEntry* entry;

    if (!crit_effects_enabled()) {
        return 1;
    }

    entry = crit_effects_find(obj);
    return entry != NULL
        ? crit_effects_entry_move_multiplier(entry, combat_turn_based_is_active())
        : 1;
}

int crit_effects_turn_action_points(int64_t obj, int action_points)
{
    CritEffectEntry* entry;

    if (!crit_effects_enabled()) {
        return action_points;
    }

    entry = crit_effects_find(obj);
    return entry != NULL
        ? crit_effects_entry_turn_action_points(entry, action_points)
        : action_points;
}

void crit_effects_turn_started(int64_t obj)
{
    CritEffectEntry* entry;

    if (!crit_effects_enabled()) {
        return;
    }

    entry = crit_effects_find(obj);
    if (entry != NULL) {
        crit_effects_entry_turn_started(entry);
    }
}

void crit_effects_turn_ended(int64_t obj)
{
    CritEffectEntry* entry;

    if (!crit_effects_enabled()) {
        return;
    }

    entry = crit_effects_find(obj);
    if (entry != NULL && crit_effects_entry_turn_ended(entry)) {
        memset(entry, 0, sizeof(*entry));
    }
}

void crit_effects_tick(int delta_ms)
{
    CritEffectEntry* entry;
    bool turn_based;
    int window_ms;
    int index;

    if (!crit_effects_enabled()) {
        return;
    }

    if (delta_ms > 0) {
        crit_effects_clock_ms += (unsigned int)delta_ms;
    }

    turn_based = combat_turn_based_is_active();
    // One turn length, the real-time budget's `R`.
    window_ms = rt_budget_pace_ms_now();

    for (index = 0; index < CRIT_EFFECTS_MAX_ENTRIES; index++) {
        entry = &(crit_effects_entries[index]);
        if (entry->obj == OBJ_HANDLE_NULL) {
            continue;
        }

        if (!obj_handle_is_valid(entry->obj)
            || critter_is_dead(entry->obj)
            || crit_effects_entry_lapsed(entry, turn_based, crit_effects_clock_ms, window_ms)) {
            memset(entry, 0, sizeof(*entry));
        }
    }
}
