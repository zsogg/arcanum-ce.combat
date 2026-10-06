#include "game/crit_guard.h"

#include <stdio.h>
#include <string.h>

#include "game/combat.h"
#include "game/combat_observer.h"
#include "game/critter.h"
#include "game/feature.h"
#include "game/gamelib.h"
#include "game/obj.h"
#include "game/obj_pool.h"
#include "game/player.h"
#include "game/rt_budget.h"
#include "game/settings.h"
#include "game/tf.h"

// The critical effects this covers.
#define CRIT_GUARD_EFFECTS (CDF_STUN | CDF_KNOCKDOWN | CDF_KNOCKOUT)

// Characters a critical has incapacitated and who are not yet past their
// protection. A fight uses a handful; a full table evicts the record seen
// incapacitated longest ago.
#define CRIT_GUARD_MAX_ENTRIES 64

static CritGuardEntry crit_guard_entries[CRIT_GUARD_MAX_ENTRIES];
static unsigned int crit_guard_clock_ms;

// ---------------------------------------------------------------------------
// The rules, on one record. Pure: no engine state is read here.
// ---------------------------------------------------------------------------

void crit_guard_entry_incapacitate(CritGuardEntry* entry, unsigned int clock_ms)
{
    entry->free_turn_started = false;
    entry->seen_down = false;
    entry->incapacitated_ms = clock_ms;
}

void crit_guard_entry_observe(CritGuardEntry* entry, bool incapacitated, unsigned int clock_ms)
{
    if (incapacitated) {
        entry->seen_down = true;
        entry->incapacitated_ms = clock_ms;
    }
}

bool crit_guard_entry_protects(const CritGuardEntry* entry, bool incapacitated,
    bool turn_based, unsigned int clock_ms, int window_ms)
{
    // Protection starts on recovery: a character still down can be hit
    // again, and one not yet seen down has not recovered from anything - a
    // fumble rolls its hit on its wielder twice in one attack.
    if (incapacitated || !entry->seen_down) {
        return false;
    }

    // In turn-based the record lasts until its free turn ends.
    if (turn_based) {
        return true;
    }

    return clock_ms - entry->incapacitated_ms < (unsigned int)window_ms;
}

void crit_guard_entry_turn_started(CritGuardEntry* entry, bool incapacitated)
{
    // A turn that begins with them down does not count: they get up during
    // it, and the next turn is the full one.
    if (!incapacitated) {
        entry->free_turn_started = true;
    }
}

bool crit_guard_entry_turn_ended(const CritGuardEntry* entry)
{
    return entry->free_turn_started;
}

bool crit_guard_entry_lapsed(const CritGuardEntry* entry, bool incapacitated,
    bool turn_based, unsigned int clock_ms, int window_ms)
{
    return !turn_based
        && !incapacitated
        && clock_ms - entry->incapacitated_ms >= (unsigned int)window_ms;
}

// ---------------------------------------------------------------------------
// The engine side.
// ---------------------------------------------------------------------------

bool crit_guard_is_down(int64_t obj)
{
    return critter_is_unconscious(obj)
        || (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_STUNNED) != 0
        || critter_is_prone(obj);
}

static CritGuardEntry* crit_guard_find(int64_t obj)
{
    int index;

    for (index = 0; index < CRIT_GUARD_MAX_ENTRIES; index++) {
        if (crit_guard_entries[index].obj == obj) {
            return &(crit_guard_entries[index]);
        }
    }

    return NULL;
}

static CritGuardEntry* crit_guard_add(int64_t obj)
{
    CritGuardEntry* entry;
    int index;

    entry = crit_guard_find(OBJ_HANDLE_NULL);
    if (entry == NULL) {
        entry = &(crit_guard_entries[0]);
        for (index = 1; index < CRIT_GUARD_MAX_ENTRIES; index++) {
            if (crit_guard_clock_ms - crit_guard_entries[index].incapacitated_ms
                > crit_guard_clock_ms - entry->incapacitated_ms) {
                entry = &(crit_guard_entries[index]);
            }
        }
    }

    memset(entry, 0, sizeof(*entry));
    entry->obj = obj;
    return entry;
}

// One line per step in the engine log, so a playtest can be read back:
// "CritGuard: <object id> <what> at <clock> ms".
static void crit_guard_log(int64_t obj, const char* what)
{
    char id[COMBAT_OBSERVER_IDENTITY_SIZE];

    combat_observer_object_id(obj, id);
    tig_debug_printf("CritGuard: %s%s %s at %u ms (%s)\n",
        id,
        player_is_local_pc_obj(obj) ? " (player)" : "",
        what,
        crit_guard_clock_ms,
        combat_turn_based_is_active() ? "turn-based" : "real time");
}

// Observes `entry`'s character now, and logs a recovery.
static bool crit_guard_observe(CritGuardEntry* entry)
{
    bool incapacitated;

    incapacitated = crit_guard_is_down(entry->obj);
    if (entry->down && !incapacitated && entry->seen_down) {
        char what[48];

        // The animation they came up into: TIG_ART_ANIM_GET_UP (8) when
        // they got up properly.
        snprintf(what, sizeof(what), "recovered (animation %d)",
            tig_art_id_anim_get(obj_field_int32_get(entry->obj, OBJ_F_CURRENT_AID)));
        crit_guard_log(entry->obj, what);
    }
    crit_guard_entry_observe(entry, incapacitated, crit_guard_clock_ms);
    entry->down = incapacitated;
    return incapacitated;
}

static int crit_guard_window_ms(void)
{
    // One turn length, the real-time budget's `R`.
    return rt_budget_pace_ms_now();
}

bool crit_guard_debug_forces(int64_t attacker, int64_t target)
{
    int mode;

    if (!feature_enabled(FEATURE_DEBUG_CRITICALS) || attacker == target) {
        return false;
    }

    mode = settings_get_value(&settings, feature_key(FEATURE_DEBUG_CRITICALS));
    return ((mode & 1) != 0 && player_is_local_pc_obj(attacker))
        || ((mode & 2) != 0 && player_is_local_pc_obj(target));
}

bool crit_guard_debug_fumbles(int64_t attacker, int64_t target)
{
    int mode;

    if (!feature_enabled(FEATURE_DEBUG_FUMBLES) || attacker == target) {
        return false;
    }

    mode = settings_get_value(&settings, feature_key(FEATURE_DEBUG_FUMBLES));
    return ((mode & 1) != 0 && player_is_local_pc_obj(attacker))
        || ((mode & 2) != 0 && player_is_local_pc_obj(target));
}

void crit_guard_reset(void)
{
    memset(crit_guard_entries, 0, sizeof(crit_guard_entries));
    crit_guard_clock_ms = 0;
}

bool crit_guard_is_protected(int64_t obj)
{
    CritGuardEntry* entry;

    if (!feature_enabled(FEATURE_CRITICAL_PROTECTION)) {
        return false;
    }

    entry = crit_guard_find(obj);
    return entry != NULL
        && crit_guard_entry_protects(entry,
            crit_guard_is_down(obj),
            combat_turn_based_is_active(),
            crit_guard_clock_ms,
            crit_guard_window_ms());
}

// The incapacitations a critical against `obj` must skip, given the effects
// it rolled. When none are skipped and it incapacitates, `obj`'s record
// starts again.
unsigned int crit_guard_filter(int64_t obj, unsigned int dam_flags)
{
    unsigned int effects;
    CritGuardEntry* entry;
    int tf_type;
    const char* what;

    if (!feature_enabled(FEATURE_CRITICAL_PROTECTION)) {
        return 0;
    }

    effects = dam_flags & CRIT_GUARD_EFFECTS;
    if (effects == 0) {
        return 0;
    }

    if (crit_guard_is_protected(obj)) {
        if ((effects & CDF_KNOCKOUT) != 0) {
            what = "Resisted knockout";
        } else if ((effects & CDF_KNOCKDOWN) != 0) {
            what = "Resisted knockdown";
        } else {
            what = "Resisted stun";
        }

        // The colours `combat_dmg` gives its own lines about this target.
        if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_PC) {
            tf_type = TF_TYPE_RED;
        } else if (critter_pc_leader_get(obj) == player_get_local_pc_obj()) {
            tf_type = TF_TYPE_YELLOW;
        } else {
            tf_type = TF_TYPE_WHITE;
        }
        tf_add(obj, tf_type, what);
        crit_guard_log(obj, what);

        return effects;
    }

    entry = crit_guard_find(obj);
    if (entry == NULL) {
        entry = crit_guard_add(obj);
    }
    crit_guard_entry_incapacitate(entry, crit_guard_clock_ms);
    crit_guard_log(obj, (effects & CDF_KNOCKOUT) != 0 ? "knocked out" : (effects & CDF_KNOCKDOWN) != 0 ? "knocked down"
                                                                                                       : "stunned");

    return 0;
}

void crit_guard_turn_started(int64_t obj)
{
    CritGuardEntry* entry;
    bool incapacitated;

    if (!feature_enabled(FEATURE_CRITICAL_PROTECTION)) {
        return;
    }

    entry = crit_guard_find(obj);
    if (entry != NULL) {
        incapacitated = crit_guard_observe(entry);
        crit_guard_entry_turn_started(entry, incapacitated);
        crit_guard_log(obj, incapacitated ? "begins a turn down" : "begins a turn up");
    }
}

void crit_guard_turn_ended(int64_t obj)
{
    CritGuardEntry* entry;

    if (!feature_enabled(FEATURE_CRITICAL_PROTECTION)) {
        return;
    }

    entry = crit_guard_find(obj);
    if (entry != NULL && crit_guard_entry_turn_ended(entry)) {
        crit_guard_log(obj, "is no longer protected: a full turn ended");
        memset(entry, 0, sizeof(*entry));
    }
}

void crit_guard_tick(int delta_ms)
{
    CritGuardEntry* entry;
    bool turn_based;
    bool incapacitated;
    int window_ms;
    int index;

    if (!feature_enabled(FEATURE_CRITICAL_PROTECTION)) {
        return;
    }

    if (delta_ms > 0) {
        crit_guard_clock_ms += (unsigned int)delta_ms;
    }

    turn_based = combat_turn_based_is_active();
    window_ms = crit_guard_window_ms();

    for (index = 0; index < CRIT_GUARD_MAX_ENTRIES; index++) {
        entry = &(crit_guard_entries[index]);
        if (entry->obj == OBJ_HANDLE_NULL) {
            continue;
        }

        if (!obj_handle_is_valid(entry->obj)
            || critter_is_dead(entry->obj)) {
            memset(entry, 0, sizeof(*entry));
            continue;
        }

        incapacitated = crit_guard_observe(entry);
        if (crit_guard_entry_lapsed(entry, incapacitated, turn_based,
                crit_guard_clock_ms, window_ms)) {
            crit_guard_log(entry->obj, "is no longer protected: its time is up");
            memset(entry, 0, sizeof(*entry));
        }
    }
}
