#include "game/weapon_rules.h"

#include <string.h>

#include <tig/tig.h>

#include "game/ce_parse.h"
#include "game/damage_type.h"
#include "game/feature.h"
#include "game/item.h"
#include "game/mes.h"
#include "game/obj.h"
#include "game/proto.h"
#include "game/skill.h"

// Weapons the file may list. The shipped game has 150 weapon prototypes; a
// rebalance lists a handful to a few dozen.
#define WEAPON_RULES_MAX 128

static WeaponRule weapon_rules[WEAPON_RULES_MAX];
// The prototype object each rule names, found on first use: not every
// prototype is loaded when the rules are read at init. 0 is not yet looked up;
// -1 is looked up and not found.
static int64_t weapon_rule_protos[WEAPON_RULES_MAX];
static int weapon_rule_count;

// ---------------------------------------------------------------------------
// The rules, pure.
// ---------------------------------------------------------------------------

static int weapon_rule_clamp(int value, int lo, int hi)
{
    return value < lo ? lo : (value > hi ? hi : value);
}

// `12-19`, or a single number for both ends.
static bool weapon_rule_parse_range(char* str, int* lo, int* hi)
{
    char* dash;
    int swap;

    // A leading minus is a negative number, not a separator.
    dash = strchr(str + strspn(str, " \t") + 1, '-');
    if (dash == NULL) {
        if (!ce_parse_int(str, lo)) {
            return false;
        }
        *hi = *lo;
        return true;
    }

    *dash = '\0';
    if (!ce_parse_int(str, lo) || !ce_parse_int(dash + 1, hi)) {
        return false;
    }
    if (*lo > *hi) {
        swap = *lo;
        *lo = *hi;
        *hi = swap;
    }
    return true;
}

bool weapon_rule_parse(int prototype, const char* str, WeaponRule* rule)
{
    WeaponRule parsed;
    char buffer[512];
    char* part;
    char* save;
    char* colon;
    char* key;
    char* end;
    int value;
    int lo;
    int hi;
    int keys = 0;

    if (str == NULL || strlen(str) >= sizeof(buffer)) {
        return false;
    }
    strcpy(buffer, str);

    parsed.prototype = prototype;
    parsed.capacity = WEAPON_RULE_UNSET;
    parsed.load_ap = WEAPON_RULE_UNSET;
    parsed.load_ap_apprentice = WEAPON_RULE_UNSET;
    parsed.fire_ap = WEAPON_RULE_UNSET;
    parsed.fire_ap_apprentice = WEAPON_RULE_UNSET;
    parsed.damage_min = WEAPON_RULE_UNSET;
    parsed.damage_max = WEAPON_RULE_UNSET;
    parsed.fatigue_min = WEAPON_RULE_UNSET;
    parsed.fatigue_max = WEAPON_RULE_UNSET;
    parsed.range = WEAPON_RULE_UNSET;
    parsed.crit_effect = WEAPON_RULE_UNSET;
    parsed.worth = WEAPON_RULE_UNSET;
    parsed.accuracy = WEAPON_RULE_UNSET;
    parsed.min_strength = WEAPON_RULE_UNSET;
    parsed.weight = WEAPON_RULE_UNSET;

    for (part = strtok_r(buffer, ",", &save); part != NULL; part = strtok_r(NULL, ",", &save)) {
        colon = strchr(part, ':');
        if (colon == NULL) {
            return false;
        }
        *colon = '\0';

        key = part + strspn(part, " \t");
        end = key + strlen(key);
        while (end > key && (end[-1] == ' ' || end[-1] == '\t')) {
            *--end = '\0';
        }

        if (strcmp(key, "Damage") == 0 || strcmp(key, "Fatigue") == 0) {
            if (!weapon_rule_parse_range(colon + 1, &lo, &hi)) {
                return false;
            }
            lo = weapon_rule_clamp(lo, 0, WEAPON_RULE_DAMAGE_MAX);
            hi = weapon_rule_clamp(hi, 0, WEAPON_RULE_DAMAGE_MAX);
            if (key[0] == 'D') {
                parsed.damage_min = lo;
                parsed.damage_max = hi;
            } else {
                parsed.fatigue_min = lo;
                parsed.fatigue_max = hi;
            }
            keys++;
            continue;
        }

        if (!ce_parse_int(colon + 1, &value)) {
            return false;
        }

        if (strcmp(key, "Capacity") == 0) {
            parsed.capacity = weapon_rule_clamp(value, WEAPON_RULE_CAPACITY_MIN, WEAPON_RULE_CAPACITY_MAX);
        } else if (strcmp(key, "LoadAP") == 0) {
            parsed.load_ap = weapon_rule_clamp(value, WEAPON_RULE_AP_MIN, WEAPON_RULE_AP_MAX);
        } else if (strcmp(key, "LoadAPApprentice") == 0) {
            parsed.load_ap_apprentice = weapon_rule_clamp(value, WEAPON_RULE_AP_MIN, WEAPON_RULE_AP_MAX);
        } else if (strcmp(key, "FireAP") == 0) {
            parsed.fire_ap = weapon_rule_clamp(value, WEAPON_RULE_AP_MIN, WEAPON_RULE_AP_MAX);
        } else if (strcmp(key, "FireAPApprentice") == 0) {
            parsed.fire_ap_apprentice = weapon_rule_clamp(value, WEAPON_RULE_AP_MIN, WEAPON_RULE_AP_MAX);
        } else if (strcmp(key, "Range") == 0) {
            parsed.range = weapon_rule_clamp(value, WEAPON_RULE_RANGE_MIN, WEAPON_RULE_RANGE_MAX);
        } else if (strcmp(key, "CritEffect") == 0) {
            parsed.crit_effect = weapon_rule_clamp(value, WEAPON_RULE_CRIT_EFFECT_MIN, WEAPON_RULE_CRIT_EFFECT_MAX);
        } else if (strcmp(key, "Worth") == 0) {
            parsed.worth = weapon_rule_clamp(value, 0, WEAPON_RULE_WORTH_MAX);
        } else if (strcmp(key, "Accuracy") == 0) {
            parsed.accuracy = weapon_rule_clamp(value, WEAPON_RULE_ACCURACY_MIN, WEAPON_RULE_ACCURACY_MAX);
        } else if (strcmp(key, "MinStrength") == 0) {
            parsed.min_strength = weapon_rule_clamp(value, WEAPON_RULE_MIN_STRENGTH_MIN, WEAPON_RULE_MIN_STRENGTH_MAX);
        } else if (strcmp(key, "Weight") == 0) {
            parsed.weight = weapon_rule_clamp(value, 0, WEAPON_RULE_WEIGHT_MAX);
        } else {
            return false;
        }
        keys++;
    }

    // Reloading needs both halves; a cost for a gun with no capacity, or the
    // reverse, is a half-written rule.
    if (keys == 0
        || (parsed.capacity == WEAPON_RULE_UNSET) != (parsed.load_ap == WEAPON_RULE_UNSET)
        || (parsed.load_ap_apprentice != WEAPON_RULE_UNSET && parsed.load_ap == WEAPON_RULE_UNSET)
        || (parsed.fire_ap_apprentice != WEAPON_RULE_UNSET && parsed.fire_ap == WEAPON_RULE_UNSET)) {
        return false;
    }

    if (parsed.load_ap != WEAPON_RULE_UNSET && parsed.load_ap_apprentice == WEAPON_RULE_UNSET) {
        parsed.load_ap_apprentice = parsed.load_ap;
    }
    if (parsed.fire_ap != WEAPON_RULE_UNSET && parsed.fire_ap_apprentice == WEAPON_RULE_UNSET) {
        parsed.fire_ap_apprentice = parsed.fire_ap;
    }

    *rule = parsed;
    return true;
}

// ---------------------------------------------------------------------------
// The engine side.
// ---------------------------------------------------------------------------

void weapon_rules_init(void)
{
    weapon_rules_load();
}

void weapon_rules_exit(void)
{
    weapon_rule_count = 0;
}

void weapon_rules_load(void)
{
    mes_file_handle_t mes_file;
    MesFileEntry entry;
    WeaponRule rule;

    weapon_rule_count = 0;
    memset(weapon_rule_protos, 0, sizeof(weapon_rule_protos));

    if (!mes_load("rules\\weapons.mes", &mes_file)) {
        return;
    }

    if (mes_find_first(mes_file, &entry)) {
        do {
            if (!weapon_rule_parse(entry.num, entry.str, &rule)) {
                tig_debug_printf("weapon rules: refusing entry {%d}{%s}\n", entry.num, entry.str);
                continue;
            }
            if (weapon_rule_count >= WEAPON_RULES_MAX) {
                tig_debug_printf("weapon rules: more than %d weapons listed; {%d} ignored\n",
                    WEAPON_RULES_MAX, entry.num);
                continue;
            }
            weapon_rules[weapon_rule_count++] = rule;
        } while (mes_find_next(mes_file, &entry));
    }

    mes_unload(mes_file);
    tig_debug_printf("weapon rules: %d weapon(s) listed\n", weapon_rule_count);
}

const WeaponRule* weapon_rule_for(int64_t weapon_obj)
{
    int64_t proto_obj;
    int index;

    if (weapon_obj == OBJ_HANDLE_NULL
        || obj_field_int32_get(weapon_obj, OBJ_F_TYPE) != OBJ_TYPE_WEAPON) {
        return NULL;
    }

    proto_obj = obj_field_handle_get(weapon_obj, OBJ_F_PROTOTYPE_HANDLE);
    for (index = 0; index < weapon_rule_count; index++) {
        if (weapon_rule_protos[index] == 0) {
            // Prototype number = description + 20, the convention the replay
            // driver and the modding notes use.
            weapon_rule_protos[index] = weapon_rules[index].prototype > 20
                ? proto_get_by_description(weapon_rules[index].prototype - 20)
                : OBJ_HANDLE_NULL;
            if (weapon_rule_protos[index] == OBJ_HANDLE_NULL) {
                weapon_rule_protos[index] = -1;
            }
        }
        if (weapon_rule_protos[index] == proto_obj) {
            return &(weapon_rules[index]);
        }
    }

    return NULL;
}

bool weapon_rules_trained(int64_t critter_obj, int64_t weapon_obj)
{
    int skill;
    int training;

    skill = item_weapon_skill(weapon_obj);
    training = IS_TECH_SKILL(skill)
        ? tech_skill_training_get(critter_obj, GET_TECH_SKILL(skill))
        : basic_skill_training_get(critter_obj, GET_BASIC_SKILL(skill));
    return training >= TRAINING_APPRENTICE;
}

static const WeaponRule* weapon_rules_active_for(int64_t weapon_obj)
{
    return feature_enabled(FEATURE_WEAPON_RULES) ? weapon_rule_for(weapon_obj) : NULL;
}

int weapon_rules_damage(int64_t weapon_obj, int damage_type, bool upper, int stock)
{
    const WeaponRule* rule;

    rule = weapon_rules_active_for(weapon_obj);
    if (rule == NULL) {
        return stock;
    }

    if (damage_type == DAMAGE_TYPE_NORMAL && rule->damage_min != WEAPON_RULE_UNSET) {
        return upper ? rule->damage_max : rule->damage_min;
    }
    if (damage_type == DAMAGE_TYPE_FATIGUE && rule->fatigue_min != WEAPON_RULE_UNSET) {
        return upper ? rule->fatigue_max : rule->fatigue_min;
    }
    return stock;
}

int weapon_rules_range(int64_t weapon_obj, int stock)
{
    const WeaponRule* rule;

    rule = weapon_rules_active_for(weapon_obj);
    return rule != NULL && rule->range != WEAPON_RULE_UNSET ? rule->range : stock;
}

int weapon_rules_crit_effect(int64_t weapon_obj, int stock)
{
    const WeaponRule* rule;

    rule = weapon_rules_active_for(weapon_obj);
    return rule != NULL && rule->crit_effect != WEAPON_RULE_UNSET ? rule->crit_effect : stock;
}

int weapon_rules_worth(int64_t weapon_obj, int stock)
{
    const WeaponRule* rule;

    rule = weapon_rules_active_for(weapon_obj);
    return rule != NULL && rule->worth != WEAPON_RULE_UNSET ? rule->worth : stock;
}

int weapon_rules_accuracy(int64_t weapon_obj)
{
    const WeaponRule* rule;

    rule = weapon_rules_active_for(weapon_obj);
    return rule != NULL && rule->accuracy != WEAPON_RULE_UNSET ? rule->accuracy : 100;
}

int weapon_rules_min_strength(int64_t weapon_obj, int stock)
{
    const WeaponRule* rule;

    rule = weapon_rules_active_for(weapon_obj);
    return rule != NULL && rule->min_strength != WEAPON_RULE_UNSET ? rule->min_strength : stock;
}

int weapon_rules_weight(int64_t weapon_obj, int stock)
{
    const WeaponRule* rule;

    rule = weapon_rules_active_for(weapon_obj);
    return rule != NULL && rule->weight != WEAPON_RULE_UNSET ? rule->weight : stock;
}

bool weapon_rules_fire_ap(int64_t critter_obj, int64_t weapon_obj, int* action_points)
{
    const WeaponRule* rule;

    rule = weapon_rules_active_for(weapon_obj);
    if (rule == NULL || rule->fire_ap == WEAPON_RULE_UNSET) {
        return false;
    }

    *action_points = weapon_rules_trained(critter_obj, weapon_obj)
        ? rule->fire_ap_apprentice
        : rule->fire_ap;
    return true;
}
