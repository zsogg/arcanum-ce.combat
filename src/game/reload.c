#include "game/reload.h"

#include <stdio.h>
#include <string.h>

#include <tig/tig.h>

#include "game/ce_parse.h"
#include "game/combat.h"
#include "game/feature.h"
#include "game/item.h"
#include "game/mes.h"
#include "game/obj.h"
#include "game/player.h"
#include "game/proto.h"
#include "game/rt_budget.h"
#include "game/tf.h"
#include "game/ui.h"

// Weapons the rules file may list. The shipped game has 150 weapon prototypes;
// a rebalance lists a handful.
#define RELOAD_MAX_RULES 64

static ReloadRule reload_rules[RELOAD_MAX_RULES];
// The prototype object each rule names, found on first use: prototypes are
// not all loaded when the rules are read at init. 0 is not yet looked up.
static int64_t reload_rule_protos[RELOAD_MAX_RULES];
static int reload_rule_count;

// ---------------------------------------------------------------------------
// The rules, pure.
// ---------------------------------------------------------------------------

static int reload_clamp(int value, int lo, int hi)
{
    return value < lo ? lo : (value > hi ? hi : value);
}

bool reload_rule_parse(int prototype, const char* str, ReloadRule* rule)
{
    char buffer[256];
    char* part;
    char* save;
    char* colon;
    char* key;
    char* end;
    int value;
    int capacity = -1;
    int load_ap = -1;
    int load_ap_apprentice = -1;

    if (str == NULL || strlen(str) >= sizeof(buffer)) {
        return false;
    }
    strcpy(buffer, str);

    for (part = strtok_r(buffer, ",", &save); part != NULL; part = strtok_r(NULL, ",", &save)) {
        colon = strchr(part, ':');
        if (colon == NULL) {
            return false;
        }
        *colon = '\0';

        key = part;
        while (*key == ' ' || *key == '\t') {
            key++;
        }
        end = key + strlen(key);
        while (end > key && (end[-1] == ' ' || end[-1] == '\t')) {
            *--end = '\0';
        }

        if (!ce_parse_int(colon + 1, &value)) {
            return false;
        }

        if (strcmp(key, "Capacity") == 0) {
            capacity = reload_clamp(value, RELOAD_CAPACITY_MIN, RELOAD_CAPACITY_MAX);
        } else if (strcmp(key, "LoadAP") == 0) {
            load_ap = reload_clamp(value, RELOAD_LOAD_AP_MIN, RELOAD_LOAD_AP_MAX);
        } else if (strcmp(key, "LoadAPApprentice") == 0) {
            load_ap_apprentice = reload_clamp(value, RELOAD_LOAD_AP_MIN, RELOAD_LOAD_AP_MAX);
        } else {
            return false;
        }
    }

    if (capacity < 0 || load_ap < 0) {
        return false;
    }

    rule->prototype = prototype;
    rule->capacity = capacity;
    rule->load_ap = load_ap;
    rule->load_ap_apprentice = load_ap_apprentice >= 0 ? load_ap_apprentice : load_ap;
    return true;
}

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

void reload_init(void)
{
    reload_rules_load();
}

void reload_exit(void)
{
    reload_rule_count = 0;
}

void reload_rules_load(void)
{
    mes_file_handle_t mes_file;
    MesFileEntry entry;
    ReloadRule rule;

    reload_rule_count = 0;
    memset(reload_rule_protos, 0, sizeof(reload_rule_protos));

    if (!mes_load("rules\\firearm_reload.mes", &mes_file)) {
        return;
    }

    if (mes_find_first(mes_file, &entry)) {
        do {
            if (!reload_rule_parse(entry.num, entry.str, &rule)) {
                tig_debug_printf("reload: refusing entry {%d}{%s}\n", entry.num, entry.str);
                continue;
            }
            if (reload_rule_count >= RELOAD_MAX_RULES) {
                tig_debug_printf("reload: more than %d weapons listed; {%d} ignored\n",
                    RELOAD_MAX_RULES, entry.num);
                continue;
            }
            reload_rules[reload_rule_count++] = rule;
        } while (mes_find_next(mes_file, &entry));
    }

    mes_unload(mes_file);
    tig_debug_printf("reload: %d weapon(s) listed\n", reload_rule_count);
}

const ReloadRule* reload_rule_for(int64_t weapon_obj)
{
    int64_t proto_obj;
    int index;

    if (weapon_obj == OBJ_HANDLE_NULL
        || obj_field_int32_get(weapon_obj, OBJ_F_TYPE) != OBJ_TYPE_WEAPON) {
        return NULL;
    }

    proto_obj = obj_field_handle_get(weapon_obj, OBJ_F_PROTOTYPE_HANDLE);
    for (index = 0; index < reload_rule_count; index++) {
        if (reload_rule_protos[index] == 0) {
            // Prototype number = description + 20, the convention the replay
            // driver and the modding notes use.
            reload_rule_protos[index] = reload_rules[index].prototype > 20
                ? proto_get_by_description(reload_rules[index].prototype - 20)
                : OBJ_HANDLE_NULL;
            if (reload_rule_protos[index] == OBJ_HANDLE_NULL) {
                reload_rule_protos[index] = -1;
            }
        }
        if (reload_rule_protos[index] == proto_obj) {
            return &(reload_rules[index]);
        }
    }

    return NULL;
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

bool reload_command(int64_t critter_obj)
{
    int64_t weapon_obj;
    const ReloadRule* rule;
    int loaded;
    int ammo_type;
    int need;
    int cost;
    int progress;
    ReloadWork work;
    char text[64];

    weapon_obj = item_wield_get(critter_obj, ITEM_INV_LOC_WEAPON);
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
    } else if (!rt_budget_instant_action(critter_obj, RT_BUDGET_RELOAD, cost - progress)) {
        // Real time, still recovering from the last action.
        return false;
    }

    if (need > 0) {
        item_ammo_transfer(critter_obj, OBJ_HANDLE_NULL, need, ammo_type, OBJ_HANDLE_NULL);
    }
    obj_field_int32_set(weapon_obj, OBJ_F_WEAPON_PAD_I_1, loaded + 1);
    obj_field_int32_set(weapon_obj, OBJ_F_WEAPON_PAD_I_2, 0);
    snprintf(text, sizeof(text), "Loaded %d/%d", loaded + 1, rule->capacity);
    reload_say(critter_obj, text);
    tig_debug_printf("reload: loaded %d/%d, %d spare bullet(s) used\n",
        loaded + 1, rule->capacity, need);
    ui_refresh_counters();
    return true;
}
