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
// damage, fatigue damage, range, critical-effect bonus, worth, accuracy,
// minimum strength, weight, armor damage, elemental damage, ammunition per
// shot and whether a hit can be dodged - to anyone
// who uses the weapon. `firearm reloading` (`reload.c`) uses Capacity and the
// loading costs, for the player in its first build. A key an entry does not
// name leaves that stat as the weapon's own.
//
// **The `.mes` reader has no comments** (quirk 53): braces anywhere, even after
// `//`, are an entry, so a rules file's notes must not use them.

// "Not set": the weapon's own value stands.
#define WEAPON_RULE_UNSET (-1)

// A firearm's critical style (`firearm criticals`, criticals CR-3; zsogg,
// 2026-10-10): what its critical hits do besides the stock rolls.
typedef enum WeaponCritStyle {
    WEAPON_CRIT_STYLE_NONE,
    // A critical always finds a gap in armor (`armor penetration`).
    WEAPON_CRIT_STYLE_PRECISE,
    // Winded takes the hit's whole damage as fatigue, and knockdown is +10.
    WEAPON_CRIT_STYLE_IMPACT,
} WeaponCritStyle;

typedef struct WeaponRule {
    int prototype;
    // Reloading (`firearm reloading`). Capacity and LoadAP come as a pair.
    int capacity;
    int load_ap;
    int load_ap_apprentice;
    // 1: a reload fills the whole magazine, and LoadAP is what that costs -
    // as many shots as the capacity and the pack allow, when the work is
    // done. Unset or 0: one shot a reload, LoadAP each (2026-10-03, zsogg: the
    // Tesla Rod, the Tesla Gun, the Charged Accelerator Gun and the Flame
    // Thrower). Needs a Capacity.
    int whole_magazine;
    // Criticals CR-3 (`firearm criticals`; zsogg, 2026-10-10): points added
    // to the critical chance of a hit, as written, and the critical style -
    // `CritStyle: Precise`, `Impact` or `None`.
    int crit_chance;
    int crit_style;
    // AP to clear a jam (`firearm fumbles`, criticals CR-4.1; zsogg,
    // 2026-10-06): the class's in the generated table - muzzle-loader 7,
    // pistol 4, rifle and repeater 5, heavy 6. Unset: the gun's LoadAP, one
    // more reload. Needs a Capacity.
    int clear_ap;
    // The stats (`weapon rules`). Each Apprentice value defaults to its own
    // untrained one.
    int fire_ap;
    int fire_ap_apprentice;
    int damage_min;
    int damage_max;
    int fatigue_min;
    int fatigue_max;
    // Elemental damage (2026-09-28, zsogg: the Charged Accelerator Gun's
    // electrical 10-18), as `low-high` like the other two.
    int poison_min;
    int poison_max;
    int electrical_min;
    int electrical_max;
    int fire_min;
    int fire_max;
    int range;
    int crit_effect;
    int worth;
    // The weapon's chance to hit, as a percentage of what it would be (2026-09-28,
    // zsogg: the Old Flintlock Pistol at 50).
    int accuracy;
    // The Strength a wielder needs to avoid the to-hit penalty (2026-09-28,
    // zsogg: the Folding Rifle asks less than the other rifles).
    int min_strength;
    // Weight, in the game's weight units (2026-09-28, zsogg: the Mechanized Gun
    // at 240, double its stock 120).
    int weight;
    // Hit points a hit takes from the target's armor when it damages armor -
    // every hit for an acid weapon, fire damage, some criticals. The engine's
    // own figure is 10 (2026-09-28, zsogg: the Acid Gun at 40).
    int armor_damage;
    // Ammunition a shot spends (2026-09-28, zsogg: the Charged Accelerator Gun
    // at 2, from its stock 3).
    int ammo_per_shot;
    // 0: the target cannot dodge a hit with this weapon; 1, or unset, as
    // stock, where every hit gets a Dodge roll (2026-10-02, zsogg: "the
    // flamethrower is also dodged quite easily by the level 30 guard, this
    // should not be possible").
    int dodgeable;
    // Against armor's protection, under `armor penetration` (armor milestone
    // 2): the weapon's own; unset takes its kind's default (`armor_rules.h`).
    int penetration;
    // Under `armor penetration` (zsogg, 2026-10-13): the percentage of the
    // normal damage armor stops that still lands as fatigue - a blunt
    // weapon's. Unset: none, as stock.
    int stopped_fatigue;
    // Under `armor penetration` (zsogg, 2026-10-13): full penetration out to
    // PenetrationRange tiles, then PenetrationFalloff percent of it lost per
    // tile beyond - a pistol's. They come as a pair.
    int penetration_range;
    int penetration_falloff;
    // Under `weapon rules` (zsogg, 2026-10-13): the long-distance to-hit
    // penalty. AccurateTiles are added to half the shooter's Perception before
    // it starts; RangePenalty is its points per tile beyond. Unset: 0 and 5,
    // stock.
    int accurate_tiles;
    int range_penalty;
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
#define WEAPON_RULE_WEIGHT_MAX 10000
#define WEAPON_RULE_ARMOR_DAMAGE_MAX 1000
#define WEAPON_RULE_AMMO_PER_SHOT_MIN 1
#define WEAPON_RULE_AMMO_PER_SHOT_MAX 100
#define WEAPON_RULE_DODGEABLE_MIN 0
#define WEAPON_RULE_DODGEABLE_MAX 1
#define WEAPON_RULE_PENETRATION_MAX 1000
#define WEAPON_RULE_CRIT_CHANCE_MIN (-100)
#define WEAPON_RULE_CRIT_CHANCE_MAX 100
#define WEAPON_RULE_PERCENT_MAX 100
#define WEAPON_RULE_TILES_MAX 100
#define WEAPON_RULE_RANGE_PENALTY_MAX 50

// Pure: reads one entry's text into `rule`. False, with `rule` untouched, for
// anything malformed - an unknown key, a value that is not a number or a
// `low-high` range, Capacity without LoadAP or the reverse, or no key at all -
// so a bad entry changes nothing rather than a guessed weapon. Values outside
// their range are clamped; a range given high-low is put in order.
bool weapon_rule_parse(int prototype, const char* str, WeaponRule* rule);
// Pure: `penetration` at `distance` tiles under the rule's PenetrationRange
// and PenetrationFalloff, rounded down and never below 0; unchanged within the
// range, or for a NULL rule or one without them.
int weapon_rule_penetration_at(const WeaponRule* rule, int distance, int penetration);

void weapon_rules_init(void);
void weapon_rules_exit(void);
// Reads `rules/weapons.mes` again. Called at init.
void weapon_rules_load(void);

// The rule for this weapon, or NULL when it is not listed.
const WeaponRule* weapon_rule_for(int64_t weapon_obj);
// Apprentice training or better in the weapon's skill - the test that gives an
// attack its +5 speed in `item_weapon_magic_speed`.
bool weapon_rules_trained(int64_t critter_obj, int64_t weapon_obj);
// Criticals CR-3, under `firearm criticals`: the weapon's critical-chance
// points (0 unlisted or off) and its critical style (none unlisted or off).
int weapon_rules_crit_chance(int64_t weapon_obj);
WeaponCritStyle weapon_rules_crit_style(int64_t weapon_obj);

// The stat overrides, under `weapon rules`: each returns `stock` when the
// switch is off, the weapon is not listed, or its entry does not name the stat.
int weapon_rules_damage(int64_t weapon_obj, int damage_type, bool upper, int stock);
int weapon_rules_range(int64_t weapon_obj, int stock);
// The critical-effect bonus `owner_obj` gets from `weapon_obj`. A table
// CritEffect applies as written: it is the weapon's design, like its Damage,
// not an enchantment (zsogg, 2026-10-05). Otherwise the stock magick bonus,
// scaled by the wielder's aptitude in `item_adjust_magic` as stock scales it.
int weapon_rules_crit_effect_for(int64_t weapon_obj, int64_t owner_obj);
int weapon_rules_worth(int64_t weapon_obj, int stock);
// Accuracy as a percentage of the ordinary chance to hit; 100 when the entry
// does not name it.
int weapon_rules_accuracy(int64_t weapon_obj);
int weapon_rules_min_strength(int64_t weapon_obj, int stock);
int weapon_rules_weight(int64_t weapon_obj, int stock);
int weapon_rules_armor_damage(int64_t weapon_obj, int stock);
// The ammunition a shot spends: the entry's AmmoPerShot, or the weapon's own
// OBJ_F_WEAPON_AMMO_CONSUMPTION. Every reader of that field goes through here.
int weapon_rules_ammo_per_shot(int64_t weapon_obj);
// False when the entry says `Dodgeable: 0`: combat then skips the target's
// Dodge roll for a hit with this weapon. True otherwise, as stock.
bool weapon_rules_dodgeable(int64_t weapon_obj);
// True, with `*penetration` set, when the entry names a Penetration. Read
// under `armor penetration`, not `weapon rules`: it means nothing without the
// armor rules.
bool weapon_rules_penetration(int64_t weapon_obj, int* penetration);
// Under `armor penetration`: a penetration at `distance` tiles, after the
// entry's PenetrationRange and PenetrationFalloff; `penetration` unchanged
// when the entry names none.
int weapon_rules_penetration_at(int64_t weapon_obj, int distance, int penetration);
// Under `armor penetration`: the entry's StoppedToFatigue, or
// WEAPON_RULE_UNSET.
int weapon_rules_stopped_to_fatigue(int64_t weapon_obj);
// Under `weapon rules`: the entry's AccurateTiles and RangePenalty, or the
// stock values given.
int weapon_rules_accurate_tiles(int64_t weapon_obj, int stock);
int weapon_rules_range_penalty(int64_t weapon_obj, int stock);
// True, with `*action_points` set, when the entry names a firing cost.
bool weapon_rules_fire_ap(int64_t critter_obj, int64_t weapon_obj, int* action_points);

#endif /* ARCANUM_GAME_WEAPON_RULES_H_ */
