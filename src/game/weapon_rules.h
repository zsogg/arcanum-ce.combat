#ifndef ARCANUM_GAME_WEAPON_RULES_H_
#define ARCANUM_GAME_WEAPON_RULES_H_

#include <stdbool.h>
#include <stdint.h>

// CE (turn-based rebalance): per-weapon rules, read from `rules/weapons.mes`.
//
// One entry per weapon, keyed by its prototype number, overriding the stats
// the rebalance changes - without touching game data. zsogg chose this over
// editing prototypes (2026-09-26): a save made with edited prototypes carries
// them, and loading it writes them back into the loose layer even after the
// mod is gone (MODDING-NOTES quirk 35), where overrides read at runtime leave
// both the data and every save mod-neutral. It is also the one place to edit a
// weapon, which zsogg asked to be the way weapon editing is streamlined.
//
//     {6101}{FireAP: 7, FireAPApprentice: 5, Damage: 12-19, CritEffect: 10,
//            Worth: 100, Capacity: 1, LoadAP: 10, LoadAPApprentice: 8}
//
// Two switches read it. `weapon rules` applies the stat overrides - firing AP,
// damage, fatigue damage, range, critical-effect bonus, worth, accuracy and
// minimum strength - to anyone
// who uses the weapon. `firearm reloading` (`reload.c`) uses Capacity and the
// loading costs, for the player in its first build. A key an entry does not
// name leaves that stat as the weapon's own.
//
// **The `.mes` reader has no comments** (quirk 53): braces anywhere, even after
// `//`, are an entry, so a rules file's notes must not use them.

// "Not set": the weapon's own value stands.
#define WEAPON_RULE_UNSET (-1)

typedef struct WeaponRule {
    int prototype;
    // Reloading (`firearm reloading`). Capacity and LoadAP come as a pair.
    int capacity;
    int load_ap;
    int load_ap_apprentice;
    // The stats (`weapon rules`). Each Apprentice value defaults to its own
    // untrained one.
    int fire_ap;
    int fire_ap_apprentice;
    int damage_min;
    int damage_max;
    int fatigue_min;
    int fatigue_max;
    int range;
    int crit_effect;
    int worth;
    // The weapon's chance to hit, as a percentage of what it would be (2026-09-28,
    // zsogg: the Old Flintlock Pistol at 50).
    int accuracy;
    // The Strength a wielder needs to avoid the to-hit penalty (2026-09-28,
    // zsogg: the Folding Rifle asks less than the other rifles).
    int min_strength;
} WeaponRule;

#define WEAPON_RULE_CAPACITY_MIN 1
#define WEAPON_RULE_CAPACITY_MAX 100
#define WEAPON_RULE_AP_MIN 1
#define WEAPON_RULE_AP_MAX 20
#define WEAPON_RULE_DAMAGE_MAX 500
#define WEAPON_RULE_RANGE_MIN 1
#define WEAPON_RULE_RANGE_MAX 100
#define WEAPON_RULE_CRIT_EFFECT_MIN (-100)
#define WEAPON_RULE_CRIT_EFFECT_MAX 100
#define WEAPON_RULE_WORTH_MAX 1000000
#define WEAPON_RULE_ACCURACY_MIN 0
#define WEAPON_RULE_ACCURACY_MAX 200
#define WEAPON_RULE_MIN_STRENGTH_MIN 1
#define WEAPON_RULE_MIN_STRENGTH_MAX 30

// Pure: reads one entry's text into `rule`. False, with `rule` untouched, for
// anything malformed - an unknown key, a value that is not a number or a
// `low-high` range, Capacity without LoadAP or the reverse, or no key at all -
// so a bad entry changes nothing rather than a guessed weapon. Values outside
// their range are clamped; a range given high-low is put in order.
bool weapon_rule_parse(int prototype, const char* str, WeaponRule* rule);

void weapon_rules_init(void);
void weapon_rules_exit(void);
// Reads `rules/weapons.mes` again. Called at init.
void weapon_rules_load(void);

// The rule for this weapon, or NULL when it is not listed.
const WeaponRule* weapon_rule_for(int64_t weapon_obj);
// Apprentice training or better in the weapon's skill - the test that gives an
// attack its +5 speed in `item_weapon_magic_speed`.
bool weapon_rules_trained(int64_t critter_obj, int64_t weapon_obj);

// The stat overrides, under `weapon rules`: each returns `stock` when the
// switch is off, the weapon is not listed, or its entry does not name the stat.
int weapon_rules_damage(int64_t weapon_obj, int damage_type, bool upper, int stock);
int weapon_rules_range(int64_t weapon_obj, int stock);
int weapon_rules_crit_effect(int64_t weapon_obj, int stock);
int weapon_rules_worth(int64_t weapon_obj, int stock);
// Accuracy as a percentage of the ordinary chance to hit; 100 when the entry
// does not name it.
int weapon_rules_accuracy(int64_t weapon_obj);
int weapon_rules_min_strength(int64_t weapon_obj, int stock);
// True, with `*action_points` set, when the entry names a firing cost.
bool weapon_rules_fire_ap(int64_t critter_obj, int64_t weapon_obj, int* action_points);

#endif /* ARCANUM_GAME_WEAPON_RULES_H_ */
