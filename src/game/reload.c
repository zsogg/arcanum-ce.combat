#include "game/reload.h"

#include <stdio.h>
#include <string.h>

#include <tig/tig.h>

#include "game/ai.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/damage_type.h"
#include "game/feature.h"
#include "game/item.h"
#include "game/obj.h"
#include "game/object.h"
#include "game/object_node.h"
#include "game/player.h"
#include "game/rt_budget.h"
#include "game/skill.h"
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

// `OBJ_F_WEAPON_PAD_I_2` holds the loading work toward the next shot in its
// low bits, and in one bit above them whether reloading has ever governed this
// gun. A gun nobody has used under reloading reads 0 there - every gun in the
// shipped data and in an existing save.
#define RELOAD_PROGRESS_MASK 0xFFFF
#define RELOAD_SEEN 0x10000

static void reload_progress_set(int64_t weapon_obj, int progress)
{
    int flags;

    flags = obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_PAD_I_2) & ~RELOAD_PROGRESS_MASK;
    obj_field_int32_set(weapon_obj, OBJ_F_WEAPON_PAD_I_2,
        flags | (progress & RELOAD_PROGRESS_MASK));
}

// Session 4 (zsogg, 2026-09-27): a gun an NPC uses starts **loaded to
// capacity**, as an authored guard would carry it; the player's start empty
// (2026-09-25). Decided the first time reloading governs the gun: a gun the
// player has touched is theirs to load, wherever it goes afterwards, and one
// the player never touched arrives full in an NPC's hands. So a gun bought and
// handed straight to a follower comes loaded - a few shots, and accepted.
static void reload_first_use(int64_t critter_obj, int64_t weapon_obj, const WeaponRule* rule)
{
    int progress;

    progress = obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_PAD_I_2);
    if ((progress & RELOAD_SEEN) != 0) {
        return;
    }

    if (!player_is_local_pc_obj(critter_obj)) {
        obj_field_int32_set(weapon_obj, OBJ_F_WEAPON_PAD_I_1, rule->capacity);
    }
    obj_field_int32_set(weapon_obj, OBJ_F_WEAPON_PAD_I_2, progress | RELOAD_SEEN);
}

// Session 4: everyone who uses a listed gun - the player, followers and
// enemies alike. The first answer about a gun also settles how it starts
// (`reload_first_use`), so every governed path sees it initialised.
bool reload_applies(int64_t critter_obj, int64_t weapon_obj)
{
    const WeaponRule* rule;

    if (!feature_enabled(FEATURE_FIREARM_RELOADING)
        || critter_obj == OBJ_HANDLE_NULL
        || !obj_type_is_critter(obj_field_int32_get(critter_obj, OBJ_F_TYPE))) {
        return false;
    }

    rule = reload_rule_for(weapon_obj);
    if (rule == NULL) {
        return false;
    }

    reload_first_use(critter_obj, weapon_obj, rule);
    return true;
}

int reload_loaded_get(int64_t weapon_obj)
{
    return obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_PAD_I_1);
}

int reload_progress_get(int64_t weapon_obj)
{
    return obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_PAD_I_2) & RELOAD_PROGRESS_MASK;
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

// The player's own messages. An NPC says nothing about its gun but that it is
// reloading (`reload_bark`).
static void reload_say(int64_t critter_obj, const char* text)
{
    if (player_is_local_pc_obj(critter_obj)) {
        tf_add(critter_obj, TF_TYPE_WHITE, text);
    }
}

// Session 4: an NPC the player can see says "Reloading" when it spends time
// loading - agreed 2026-09-20. Never one that is prowling or invisible: the
// word must not give away where a hidden gunner is.
static void reload_bark(int64_t critter_obj)
{
    if (player_is_local_pc_obj(critter_obj)
        || critter_is_concealed(critter_obj)
        || (obj_field_int32_get(critter_obj, OBJ_F_FLAGS) & OF_INVISIBLE) != 0) {
        return;
    }

    tf_add(critter_obj, TF_TYPE_WHITE, "Reloading");
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
static bool reload_can_load_impl(int64_t critter_obj, int64_t weapon_obj, bool quiet);

static bool reload_can_load(int64_t critter_obj, int64_t weapon_obj)
{
    return reload_can_load_impl(critter_obj, weapon_obj, false);
}

bool reload_can_load_quiet(int64_t critter_obj, int64_t weapon_obj)
{
    return reload_can_load_impl(critter_obj, weapon_obj, true);
}

static bool reload_can_load_impl(int64_t critter_obj, int64_t weapon_obj, bool quiet)
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
        if (!quiet) {
            reload_say(critter_obj, text);
        }
        return false;
    }

    // The shot's bullets must all be there before any work is done, so a
    // reload never charges for a shot it cannot finish.
    ammo_type = item_weapon_ammo_type(weapon_obj);
    need = ammo_type != 10000
        ? obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_AMMO_CONSUMPTION)
        : 0;
    if (need > 0 && item_ammo_quantity_get(critter_obj, ammo_type) < need) {
        if (!quiet) {
            reload_say(critter_obj, "No ammunition");
        }
        return false;
    }

    return true;
}

// The shot is loaded: its bullets move from the pack into the gun.
static void reload_load_one(int64_t critter_obj, int64_t weapon_obj, bool quiet)
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
    reload_progress_set(weapon_obj, 0);
    snprintf(text, sizeof(text), "Loaded %d/%d", loaded + 1, rule->capacity);
    if (!quiet) {
        reload_say(critter_obj, text);
    }
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
        reload_bark(critter_obj);
        if (!work.completed) {
            reload_progress_set(weapon_obj, work.progress);
            snprintf(text, sizeof(text), "Reloading %d/%d", work.progress, cost);
            reload_say(critter_obj, text);
            tig_debug_printf("reload: %d of %d AP done toward the next shot\n",
                work.progress, cost);
            return true;
        }
        reload_load_one(critter_obj, weapon_obj, false);
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
        reload_bark(critter_obj);
        tig_debug_printf("reload: loading, %d AP, standing\n", cost - progress);
        return true;
    }

    // Real time without the budget: stock real time gives no action any time,
    // and a reload is no different.
    reload_load_one(critter_obj, weapon_obj, false);
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

    reload_load_one(critter_obj, weapon_obj, false);
}

// ---------------------------------------------------------------------------
// Session 4: followers and enemies, and loading outside combat.
// ---------------------------------------------------------------------------

// The NPC's best melee weapon in its pack, by its upper normal damage, or NULL.
static int64_t reload_melee_backup(int64_t critter_obj)
{
    int64_t best_obj = OBJ_HANDLE_NULL;
    int best_damage = -1;
    int count;
    int index;
    int64_t item_obj;
    int min_damage;
    int max_damage;

    count = obj_field_int32_get(critter_obj, OBJ_F_CRITTER_INVENTORY_NUM);
    for (index = 0; index < count; index++) {
        item_obj = obj_arrayfield_handle_get(critter_obj, OBJ_F_CRITTER_INVENTORY_LIST_IDX, index);
        if (obj_field_int32_get(item_obj, OBJ_F_TYPE) != OBJ_TYPE_WEAPON
            || item_weapon_range(item_obj, critter_obj) > 1
            || item_check_wield(item_obj, ITEM_INV_LOC_WEAPON, critter_obj) != ITEM_CANNOT_OK) {
            continue;
        }
        item_weapon_damage(item_obj, critter_obj, DAMAGE_TYPE_NORMAL, SKILL_MELEE, false, &min_damage, &max_damage);
        if (max_damage > best_damage) {
            best_damage = max_damage;
            best_obj = item_obj;
        }
    }

    return best_obj;
}

bool reload_ai_act(int64_t critter_obj, int64_t enemy_obj)
{
    int64_t weapon_obj;
    int64_t melee_obj;

    weapon_obj = item_wield_get(critter_obj, ITEM_INV_LOC_WEAPON);
    if (!reload_applies(critter_obj, weapon_obj)) {
        return false;
    }

    // Real time: a standing reload under way is waited for, not cancelled by
    // the next order.
    if (rt_budget_standing_action_pending(critter_obj)) {
        return true;
    }

    // A shot loaded: fire it whenever the target is in range (zsogg,
    // 2026-09-27, after his session 4 playtest). With no target in range, the
    // gun is topped up a shot at a time before closing in; a full gun closes
    // in as stock does.
    if (reload_loaded_get(weapon_obj) > 0) {
        if (enemy_obj != OBJ_HANDLE_NULL
            && object_dist(critter_obj, enemy_obj) <= item_weapon_range(weapon_obj, critter_obj)) {
            return false;
        }
        if (!reload_can_load_quiet(critter_obj, weapon_obj)) {
            return false;
        }
        tig_debug_printf("reload: an NPC tops up, no target in range\n");
        if (!reload_command(critter_obj)
            && combat_turn_based_is_active()
            && combat_turn_based_whos_turn_get() == critter_obj) {
            combat_turn_based_end_critter_turn(critter_obj);
        }
        return true;
    }

    // Empty, with an enemy right beside it: a melee backup if it carries one
    // (zsogg, 2026-09-27, the agreed "prefer a melee backup under pressure").
    // The swap is free for an NPC: the swap cost is the player's for now.
    if (enemy_obj != OBJ_HANDLE_NULL
        && object_dist(critter_obj, enemy_obj) <= 1) {
        melee_obj = reload_melee_backup(critter_obj);
        if (melee_obj != OBJ_HANDLE_NULL && item_wield_set(melee_obj, ITEM_INV_LOC_WEAPON)) {
            tig_debug_printf("reload: an NPC takes a melee backup\n");
            return false; // attack with it now
        }
    }

    if (reload_can_load(critter_obj, weapon_obj)) {
        if (!reload_command(critter_obj)
            && combat_turn_based_is_active()
            && combat_turn_based_whos_turn_get() == critter_obj) {
            // No AP left for the work: the turn is over.
            combat_turn_based_end_critter_turn(critter_obj);
        }
        return true;
    }

    // Nothing to load it with: another weapon. The empty gun no longer counts
    // as usable (`item_wield_best`), so it is not picked again.
    tig_debug_printf("reload: an NPC is out of ammunition and switches weapon\n");
    ai_switch_weapon(critter_obj);
    return true;
}

// Automatic loading outside combat: time toward the next shot, per critter.
#define RELOAD_PREP_MAX 16

static struct {
    int64_t obj;
    int elapsed_ms;
    // Visited in this tick; an entry a tick passes over - a follower who left
    // or died - is freed at its end.
    bool visited;
} reload_prep[RELOAD_PREP_MAX];

static void reload_prep_one(int64_t critter_obj, int delta_ms)
{
    int64_t weapon_obj;
    int index;
    int free_index = -1;
    int duration_ms;

    for (index = 0; index < RELOAD_PREP_MAX; index++) {
        if (reload_prep[index].obj == critter_obj) {
            break;
        }
        if (free_index < 0 && reload_prep[index].obj == OBJ_HANDLE_NULL) {
            free_index = index;
        }
    }

    weapon_obj = item_wield_get(critter_obj, ITEM_INV_LOC_WEAPON);
    if (combat_critter_is_combat_mode_active(critter_obj)
        || !reload_applies(critter_obj, weapon_obj)
        || reload_loaded_get(weapon_obj) >= reload_rule_for(weapon_obj)->capacity) {
        // Nothing to do, or not safe: start again from zero next time.
        if (index < RELOAD_PREP_MAX) {
            reload_prep[index].obj = OBJ_HANDLE_NULL;
        }
        return;
    }

    if (index == RELOAD_PREP_MAX) {
        if (free_index < 0) {
            return;
        }
        index = free_index;
        reload_prep[index].obj = critter_obj;
        reload_prep[index].elapsed_ms = 0;
    }
    reload_prep[index].visited = true;

    reload_prep[index].elapsed_ms += delta_ms;
    duration_ms = rt_budget_duration_ms(rt_budget_pace_ms_now(),
        combat_reload_cost(critter_obj, weapon_obj) - reload_progress_get(weapon_obj),
        combat_action_points_per_turn(critter_obj));
    if (reload_prep[index].elapsed_ms < duration_ms) {
        return;
    }

    reload_prep[index].elapsed_ms = 0;
    if (reload_can_load_quiet(critter_obj, weapon_obj)) {
        reload_load_one(critter_obj, weapon_obj, true);
        tig_debug_printf("reload: loaded outside combat\n");
    }
}

void reload_prep_tick(int delta_ms)
{
    int64_t pc_obj;
    ObjectList objects;
    ObjectNode* node;
    int index;

    if (!feature_enabled(FEATURE_FIREARM_RELOADING)
        || combat_turn_based_is_active()
        || delta_ms <= 0) {
        return;
    }

    pc_obj = player_get_local_pc_obj();
    if (pc_obj == OBJ_HANDLE_NULL) {
        return;
    }

    for (index = 0; index < RELOAD_PREP_MAX; index++) {
        reload_prep[index].visited = false;
    }

    // While the player is in combat mode the party is fighting: nobody's gun
    // loads itself, and the time so far is dropped.
    if (!combat_critter_is_combat_mode_active(pc_obj)) {
        reload_prep_one(pc_obj, delta_ms);
        object_list_all_followers(pc_obj, &objects);
        for (node = objects.head; node != NULL; node = node->next) {
            reload_prep_one(node->obj, delta_ms);
        }
        object_list_destroy(&objects);
    }

    for (index = 0; index < RELOAD_PREP_MAX; index++) {
        if (!reload_prep[index].visited) {
            reload_prep[index].obj = OBJ_HANDLE_NULL;
        }
    }
}
