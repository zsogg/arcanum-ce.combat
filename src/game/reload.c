#include "game/reload.h"

#include <stdio.h>
#include <string.h>

#include <tig/tig.h>

#include "game/combat.h"
#include "game/feature.h"
#include "game/item.h"
#include "game/obj.h"
#include "game/player.h"
#include "game/rt_budget.h"
#include "game/tf.h"
#include "game/ui.h"
#include "game/weapon_rules.h"

// ---------------------------------------------------------------------------
// The work, pure.
// ---------------------------------------------------------------------------

ReloadWork reload_work(int cost, int progress, int available_ap)
{
    ReloadWork work;
    int owed;

    if (progress < 0) {
        progress = 0;
    }

    owed = cost - progress;
    if (owed < 0) {
        owed = 0;
    }

    work.spent = available_ap < owed ? available_ap : owed;
    if (work.spent < 0) {
        work.spent = 0;
    }
    work.progress = progress + work.spent;
    work.completed = work.progress >= cost;
    if (work.completed) {
        work.progress = 0;
    }
    return work;
}

// ---------------------------------------------------------------------------
// The engine side.
// ---------------------------------------------------------------------------

// The weapon's rule when it names a Capacity: reloading's own view of it.
static const WeaponRule* reload_rule_for(int64_t weapon_obj)
{
    const WeaponRule* rule;

    rule = weapon_rule_for(weapon_obj);
    return rule != NULL && rule->capacity != WEAPON_RULE_UNSET ? rule : NULL;
}

bool reload_applies(int64_t critter_obj, int64_t weapon_obj)
{
    return feature_enabled(FEATURE_FIREARM_RELOADING)
        && critter_obj != OBJ_HANDLE_NULL
        && player_is_local_pc_obj(critter_obj)
        && reload_rule_for(weapon_obj) != NULL;
}

int reload_loaded_get(int64_t weapon_obj)
{
    return obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_PAD_I_1);
}

int reload_progress_get(int64_t weapon_obj)
{
    return obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_PAD_I_2);
}

bool reload_wielded_is_empty(int64_t critter_obj)
{
    int64_t weapon_obj;

    if (critter_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    weapon_obj = item_wield_get(critter_obj, ITEM_INV_LOC_WEAPON);
    return reload_applies(critter_obj, weapon_obj)
        && reload_loaded_get(weapon_obj) <= 0;
}

static void reload_say(int64_t critter_obj, const char* text)
{
    tf_add(critter_obj, TF_TYPE_WHITE, text);
}

bool reload_fire(int64_t critter_obj, int64_t weapon_obj)
{
    int loaded;

    loaded = reload_loaded_get(weapon_obj);
    if (loaded <= 0) {
        return false;
    }

    obj_field_int32_set(weapon_obj, OBJ_F_WEAPON_PAD_I_1, loaded - 1);
    tig_debug_printf("reload: fired; %d loaded\n", loaded - 1);
    (void)critter_obj;
    return true;
}

// Whether the wielded gun can take another shot now: governed, not full, and
// the shot's bullets in the pack. Says why not over the player's head.
static bool reload_can_load(int64_t critter_obj, int64_t weapon_obj)
{
    const WeaponRule* rule;
    int loaded;
    int ammo_type;
    int need;
    char text[64];

    if (!reload_applies(critter_obj, weapon_obj)) {
        return false;
    }

    rule = reload_rule_for(weapon_obj);
    loaded = reload_loaded_get(weapon_obj);
    if (loaded >= rule->capacity) {
        snprintf(text, sizeof(text), "Loaded %d/%d", loaded, rule->capacity);
        reload_say(critter_obj, text);
        return false;
    }

    // The shot's bullets must all be there before any work is done, so a
    // reload never charges for a shot it cannot finish.
    ammo_type = item_weapon_ammo_type(weapon_obj);
    need = ammo_type != 10000
        ? obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_AMMO_CONSUMPTION)
        : 0;
    if (need > 0 && item_ammo_quantity_get(critter_obj, ammo_type) < need) {
        reload_say(critter_obj, "No ammunition");
        return false;
    }

    return true;
}

// The shot is loaded: its bullets move from the pack into the gun.
static void reload_load_one(int64_t critter_obj, int64_t weapon_obj)
{
    const WeaponRule* rule;
    int loaded;
    int ammo_type;
    int need;
    char text[64];

    rule = reload_rule_for(weapon_obj);
    loaded = reload_loaded_get(weapon_obj);
    ammo_type = item_weapon_ammo_type(weapon_obj);
    need = ammo_type != 10000
        ? obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_AMMO_CONSUMPTION)
        : 0;

    if (need > 0) {
        item_ammo_transfer(critter_obj, OBJ_HANDLE_NULL, need, ammo_type, OBJ_HANDLE_NULL);
    }
    obj_field_int32_set(weapon_obj, OBJ_F_WEAPON_PAD_I_1, loaded + 1);
    obj_field_int32_set(weapon_obj, OBJ_F_WEAPON_PAD_I_2, 0);
    snprintf(text, sizeof(text), "Loaded %d/%d", loaded + 1, rule->capacity);
    reload_say(critter_obj, text);
    tig_debug_printf("reload: loaded %d/%d, %d spare bullet(s) used, %d left\n",
        loaded + 1, rule->capacity, need,
        ammo_type != 10000 ? item_ammo_quantity_get(critter_obj, ammo_type) : 0);
    ui_refresh_counters();
}

bool reload_command(int64_t critter_obj)
{
    int64_t weapon_obj;
    int cost;
    int progress;
    ReloadWork work;
    char text[64];

    weapon_obj = item_wield_get(critter_obj, ITEM_INV_LOC_WEAPON);
    if (!reload_can_load(critter_obj, weapon_obj)) {
        return false;
    }

    cost = combat_reload_cost(critter_obj, weapon_obj);
    progress = reload_progress_get(weapon_obj);

    if (combat_turn_based_is_active()) {
        if (combat_turn_based_whos_turn_get() != critter_obj) {
            return false;
        }

        work = reload_work(cost, progress, combat_action_points_get());
        if (work.spent == 0 && !work.completed) {
            reload_say(critter_obj, "No action points");
            return false;
        }
        if (work.spent > 0 && !combat_consume_action_points(critter_obj, work.spent)) {
            return false;
        }
        if (!work.completed) {
            obj_field_int32_set(weapon_obj, OBJ_F_WEAPON_PAD_I_2, work.progress);
            snprintf(text, sizeof(text), "Reloading %d/%d", work.progress, cost);
            reload_say(critter_obj, text);
            tig_debug_printf("reload: %d of %d AP done toward the next shot\n",
                work.progress, cost);
            return true;
        }
        reload_load_one(critter_obj, weapon_obj);
        return true;
    }

    // Real time: only standing still, and the shot is loaded when its time has
    // passed with the player still standing (`reload_standing_done`).
    if (rt_budget_is_moving_now(critter_obj)) {
        reload_say(critter_obj, "Stand still to reload");
        tig_debug_printf("reload: refused while moving\n");
        return false;
    }

    if (rt_budget_active()) {
        if (!rt_budget_begin_standing_action(critter_obj, RT_BUDGET_RELOAD, cost - progress)) {
            return false;
        }
        reload_say(critter_obj, "Reloading");
        tig_debug_printf("reload: loading, %d AP, standing\n", cost - progress);
        return true;
    }

    // Real time without the budget: stock real time gives no action any time,
    // and a reload is no different.
    reload_load_one(critter_obj, weapon_obj);
    return true;
}

void reload_standing_done(int64_t critter_obj, bool completed)
{
    int64_t weapon_obj;

    if (!completed) {
        reload_say(critter_obj, "Reload interrupted");
        tig_debug_printf("reload: interrupted; nothing loaded\n");
        return;
    }

    // Checked again: the pack or the hand may have changed while it stood.
    weapon_obj = item_wield_get(critter_obj, ITEM_INV_LOC_WEAPON);
    if (!reload_can_load(critter_obj, weapon_obj)) {
        return;
    }

    reload_load_one(critter_obj, weapon_obj);
}
