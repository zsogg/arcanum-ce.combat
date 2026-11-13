// The combat rebalance's weapon table, `rules/weapons.mes`: what an entry may
// say, pure. The engine side - the overrides at each stat, reloading - is
// exercised in scripted runs.

#include <string.h>

#include "game/item.h"
#include "game/skill.h"
#include "game/weapon_rules.h"

#include "test_harness.h"

TEST(an_entry_reads_every_key)
{
    WeaponRule rule;

    // The Flintlock as zsogg set it on 2026-09-26.
    CHECK(weapon_rule_parse(6101,
        "FireAP: 7, FireAPApprentice: 5, Damage: 12-19, CritEffect: 10, Worth: 100,"
        " Capacity: 1, LoadAP: 8, LoadAPApprentice: 6",
        &rule));
    CHECK_INT(rule.prototype, 6101);
    CHECK_INT(rule.fire_ap, 7);
    CHECK_INT(rule.fire_ap_apprentice, 5);
    CHECK_INT(rule.damage_min, 12);
    CHECK_INT(rule.damage_max, 19);
    CHECK_INT(rule.fatigue_min, WEAPON_RULE_UNSET);
    CHECK_INT(rule.crit_effect, 10);
    CHECK_INT(rule.worth, 100);
    CHECK_INT(rule.capacity, 1);
    CHECK_INT(rule.load_ap, 8);
    CHECK_INT(rule.load_ap_apprentice, 6);
    CHECK_INT(rule.range, WEAPON_RULE_UNSET);

    // The Repeater: keys in another order, spacing as a hand writes it.
    CHECK(weapon_rule_parse(6065,
        "Range: 18 ,Fatigue:1-4, Damage : 5-10, Capacity: 7, LoadAP: 3,"
        "LoadAPApprentice: 2, FireAP: 6, FireAPApprentice: 4",
        &rule));
    CHECK_INT(rule.range, 18);
    CHECK_INT(rule.fatigue_min, 1);
    CHECK_INT(rule.fatigue_max, 4);
    CHECK_INT(rule.damage_min, 5);
    CHECK_INT(rule.damage_max, 10);
    CHECK_INT(rule.capacity, 7);
    CHECK_INT(rule.worth, WEAPON_RULE_UNSET);
}

TEST(an_entry_may_change_only_stats_or_only_reloading)
{
    WeaponRule rule;

    CHECK(weapon_rule_parse(6053, "Damage: 8-12", &rule));
    CHECK_INT(rule.capacity, WEAPON_RULE_UNSET);
    CHECK_INT(rule.fire_ap, WEAPON_RULE_UNSET);

    CHECK(weapon_rule_parse(6053, "Capacity: 6, LoadAP: 2", &rule));
    CHECK_INT(rule.damage_min, WEAPON_RULE_UNSET);
}

TEST(an_apprentice_cost_defaults_to_the_untrained_one)
{
    WeaponRule rule;

    CHECK(weapon_rule_parse(6101, "Capacity: 1, LoadAP: 4, FireAP: 7", &rule));
    CHECK_INT(rule.load_ap_apprentice, 4);
    CHECK_INT(rule.fire_ap_apprentice, 7);
}

TEST(a_single_number_is_both_ends_and_a_reversed_range_is_ordered)
{
    WeaponRule rule;

    CHECK(weapon_rule_parse(6101, "Damage: 9", &rule));
    CHECK_INT(rule.damage_min, 9);
    CHECK_INT(rule.damage_max, 9);
    CHECK(weapon_rule_parse(6101, "Damage: 19-12", &rule));
    CHECK_INT(rule.damage_min, 12);
    CHECK_INT(rule.damage_max, 19);
}

TEST(a_value_outside_its_range_is_clamped)
{
    WeaponRule rule;

    CHECK(weapon_rule_parse(6101,
        "Capacity: 0, LoadAP: 99, FireAP: 0, Range: 500, CritEffect: -300,"
        " Worth: -5, Damage: 0-9999",
        &rule));
    CHECK_INT(rule.capacity, WEAPON_RULE_CAPACITY_MIN);
    CHECK_INT(rule.load_ap, WEAPON_RULE_AP_MAX);
    CHECK_INT(rule.fire_ap, WEAPON_RULE_AP_MIN);
    CHECK_INT(rule.range, WEAPON_RULE_RANGE_MAX);
    CHECK_INT(rule.crit_effect, WEAPON_RULE_CRIT_EFFECT_MIN);
    CHECK_INT(rule.worth, 0);
    CHECK_INT(rule.damage_max, WEAPON_RULE_DAMAGE_MAX);
}

TEST(accuracy_is_a_percentage)
{
    WeaponRule rule;

    // The Old Flintlock Pistol, zsogg 2026-09-28: half the Flintlock's chance.
    CHECK(weapon_rule_parse(6102, "Accuracy: 50", &rule));
    CHECK_INT(rule.accuracy, 50);
    CHECK(weapon_rule_parse(6102, "Damage: 12-19", &rule));
    CHECK_INT(rule.accuracy, WEAPON_RULE_UNSET);
    CHECK(weapon_rule_parse(6102, "Accuracy: 900", &rule));
    CHECK_INT(rule.accuracy, WEAPON_RULE_ACCURACY_MAX);
    CHECK(weapon_rule_parse(6102, "Accuracy: -5", &rule));
    CHECK_INT(rule.accuracy, WEAPON_RULE_ACCURACY_MIN);
}

TEST(blunt_fatigue_and_range_keys_are_read_and_clamped)
{
    WeaponRule rule;

    // zsogg, 2026-10-13: a mace's stopped blow tires at 40%; a rifle is
    // accurate 4 tiles further, at 3 a tile beyond; a pistol's penetration
    // falls off past 6 tiles.
    CHECK(weapon_rule_parse(6052, "Penetration: 46, StoppedToFatigue: 40", &rule));
    CHECK_INT(rule.stopped_fatigue, 40);
    CHECK_INT(rule.accurate_tiles, WEAPON_RULE_UNSET);
    CHECK(weapon_rule_parse(6060, "AccurateTiles: 4, RangePenalty: 3", &rule));
    CHECK_INT(rule.accurate_tiles, 4);
    CHECK_INT(rule.range_penalty, 3);
    CHECK_INT(rule.stopped_fatigue, WEAPON_RULE_UNSET);
    CHECK(weapon_rule_parse(6053, "RangePenalty: 6, PenetrationRange: 6, PenetrationFalloff: 15", &rule));
    CHECK_INT(rule.penetration_range, 6);
    CHECK_INT(rule.penetration_falloff, 15);
    CHECK(weapon_rule_parse(6052, "StoppedToFatigue: 250, RangePenalty: 90", &rule));
    CHECK_INT(rule.stopped_fatigue, WEAPON_RULE_PERCENT_MAX);
    CHECK_INT(rule.range_penalty, WEAPON_RULE_RANGE_PENALTY_MAX);

    // The penetration pair comes together.
    CHECK(!weapon_rule_parse(6053, "PenetrationRange: 6", &rule));
    CHECK(!weapon_rule_parse(6053, "PenetrationFalloff: 15", &rule));
}

TEST(penetration_falls_off_beyond_its_range)
{
    WeaponRule rule;

    CHECK(weapon_rule_parse(6053, "Penetration: 24, PenetrationRange: 6, PenetrationFalloff: 15", &rule));
    CHECK_INT(weapon_rule_penetration_at(&rule, 1, 24), 24);
    CHECK_INT(weapon_rule_penetration_at(&rule, 6, 24), 24);
    CHECK_INT(weapon_rule_penetration_at(&rule, 7, 24), 20); // 85%, rounded down
    CHECK_INT(weapon_rule_penetration_at(&rule, 10, 24), 9); // 40%
    CHECK_INT(weapon_rule_penetration_at(&rule, 12, 24), 2); // 10%
    CHECK_INT(weapon_rule_penetration_at(&rule, 13, 24), 0);
    CHECK_INT(weapon_rule_penetration_at(&rule, 40, 24), 0);

    CHECK(weapon_rule_parse(6060, "Penetration: 39", &rule));
    CHECK_INT(weapon_rule_penetration_at(&rule, 20, 39), 39);
    CHECK_INT(weapon_rule_penetration_at(NULL, 20, 39), 39);
}

TEST(minimum_strength_is_read_and_clamped)
{
    WeaponRule rule;

    // The Folding Rifle, zsogg 2026-09-28: less Strength than the other rifles.
    CHECK(weapon_rule_parse(6121, "MinStrength: 6", &rule));
    CHECK_INT(rule.min_strength, 6);
    CHECK(weapon_rule_parse(6121, "Damage: 10-18", &rule));
    CHECK_INT(rule.min_strength, WEAPON_RULE_UNSET);
    CHECK(weapon_rule_parse(6121, "MinStrength: 0", &rule));
    CHECK_INT(rule.min_strength, WEAPON_RULE_MIN_STRENGTH_MIN);
}

TEST(weight_is_read_and_clamped)
{
    WeaponRule rule;

    // The Mechanized Gun, zsogg 2026-09-28: double its stock weight.
    CHECK(weapon_rule_parse(6124, "Weight: 240", &rule));
    CHECK_INT(rule.weight, 240);
    CHECK(weapon_rule_parse(6124, "Damage: 5-30", &rule));
    CHECK_INT(rule.weight, WEAPON_RULE_UNSET);
    CHECK(weapon_rule_parse(6124, "Weight: -1", &rule));
    CHECK_INT(rule.weight, 0);
}

TEST(penetration_is_read_and_clamped)
{
    WeaponRule rule;

    CHECK(weapon_rule_parse(6059, "Penetration: 60", &rule));
    CHECK_INT(rule.penetration, 60);
    CHECK(weapon_rule_parse(6059, "Damage: 20-50", &rule));
    CHECK_INT(rule.penetration, WEAPON_RULE_UNSET);
    CHECK(weapon_rule_parse(6059, "Penetration: 5000", &rule));
    CHECK_INT(rule.penetration, WEAPON_RULE_PENETRATION_MAX);
    CHECK(weapon_rule_parse(6059, "Penetration: -4", &rule));
    CHECK_INT(rule.penetration, 0);
}

TEST(whole_magazine_is_read_and_needs_a_capacity)
{
    WeaponRule rule;

    // The Tesla Gun, zsogg 2026-10-03: 5 shots, 6 AP for the magazine.
    CHECK(weapon_rule_parse(6174, "Capacity: 5, LoadAP: 6, WholeMagazine: 1", &rule));
    CHECK_INT(rule.whole_magazine, 1);
    CHECK_INT(rule.capacity, 5);
    CHECK(weapon_rule_parse(6174, "Capacity: 5, LoadAP: 6", &rule));
    CHECK_INT(rule.whole_magazine, WEAPON_RULE_UNSET);
    CHECK(weapon_rule_parse(6174, "Capacity: 5, LoadAP: 6, WholeMagazine: 9", &rule));
    CHECK_INT(rule.whole_magazine, 1);
    // A whole-magazine reload of no magazine is a half-written rule.
    CHECK(!weapon_rule_parse(6174, "WholeMagazine: 1", &rule));
    CHECK(!weapon_rule_parse(6174, "Damage: 5-15, WholeMagazine: 1", &rule));
}

TEST(clear_ap_is_read_clamped_and_needs_a_capacity)
{
    WeaponRule rule;

    // Criticals CR-4.1, zsogg 2026-10-06: a pistol clears a jam for 4 AP.
    CHECK(weapon_rule_parse(6100, "Capacity: 6, LoadAP: 5, ClearAP: 4", &rule));
    CHECK_INT(rule.clear_ap, 4);
    CHECK(weapon_rule_parse(6100, "Capacity: 6, LoadAP: 5", &rule));
    CHECK_INT(rule.clear_ap, WEAPON_RULE_UNSET);
    CHECK(weapon_rule_parse(6100, "Capacity: 6, LoadAP: 5, ClearAP: 99", &rule));
    CHECK_INT(rule.clear_ap, WEAPON_RULE_AP_MAX);
    CHECK(weapon_rule_parse(6100, "Capacity: 6, LoadAP: 5, ClearAP: 0", &rule));
    CHECK_INT(rule.clear_ap, WEAPON_RULE_AP_MIN);
    // Clearing a gun that has no magazine is a half-written rule.
    CHECK(!weapon_rule_parse(6100, "ClearAP: 4", &rule));
    CHECK(!weapon_rule_parse(6100, "Damage: 3-10, ClearAP: 4", &rule));
}

TEST(critical_chance_and_style_are_read)
{
    WeaponRule rule;

    // Criticals CR-3, zsogg 2026-10-10: the pistols +11; the rifles Precise,
    // the muzzle-loaders and heavy guns Impact.
    CHECK(weapon_rule_parse(6053, "CritChance: 11, CritEffect: 4", &rule));
    CHECK_INT(rule.crit_chance, 11);
    CHECK_INT(rule.crit_style, WEAPON_RULE_UNSET);
    CHECK(weapon_rule_parse(6128, "CritEffect: 25, CritStyle: Precise", &rule));
    CHECK_INT(rule.crit_style, WEAPON_CRIT_STYLE_PRECISE);
    CHECK_INT(rule.crit_chance, WEAPON_RULE_UNSET);
    CHECK(weapon_rule_parse(6101, "CritStyle:  Impact ", &rule));
    CHECK_INT(rule.crit_style, WEAPON_CRIT_STYLE_IMPACT);
    CHECK(weapon_rule_parse(6060, "CritStyle: None", &rule));
    CHECK_INT(rule.crit_style, WEAPON_CRIT_STYLE_NONE);
    CHECK(weapon_rule_parse(6053, "CritChance: 500", &rule));
    CHECK_INT(rule.crit_chance, WEAPON_RULE_CRIT_CHANCE_MAX);
    CHECK(weapon_rule_parse(6053, "CritChance: -500", &rule));
    CHECK_INT(rule.crit_chance, WEAPON_RULE_CRIT_CHANCE_MIN);
    // A style the engine does not know, or two words, is refused whole.
    CHECK(!weapon_rule_parse(6053, "CritStyle: precise", &rule));
    CHECK(!weapon_rule_parse(6053, "CritStyle: Impact Precise", &rule));
    CHECK(!weapon_rule_parse(6053, "CritStyle: 1", &rule));
    CHECK(!weapon_rule_parse(6053, "CritChance: many", &rule));
}

TEST(expertise_keys_are_read_and_need_their_base_cost)
{
    WeaponRule rule;

    // Firearm expertise, zsogg 2026-11-12: the Rifle - Apprentice 1 off the
    // load, Expert and Master 1 more off both, criticals 5, 13, 23, and 70% of
    // the Firearms level as damage.
    CHECK(weapon_rule_parse(6128,
        "FireAP: 6, FireAPApprentice: 6, FireAPExpert: 5, FireAPMaster: 4,"
        " Capacity: 1, LoadAP: 6, LoadAPApprentice: 5, LoadAPExpert: 4, LoadAPMaster: 3,"
        " CritChanceApprentice: 5, CritChanceExpert: 13, CritChanceMaster: 23, SkillDamage: 70",
        &rule));
    CHECK_INT(rule.fire_ap_expert, 5);
    CHECK_INT(rule.fire_ap_master, 4);
    CHECK_INT(rule.load_ap_expert, 4);
    CHECK_INT(rule.load_ap_master, 3);
    CHECK_INT(rule.crit_chance_apprentice, 5);
    CHECK_INT(rule.crit_chance_expert, 13);
    CHECK_INT(rule.crit_chance_master, 23);
    CHECK_INT(rule.skill_damage, 70);
    CHECK_INT(rule.free_load, WEAPON_RULE_UNSET);

    // The pistols' 15% free shot; clamped like the other percentages.
    CHECK(weapon_rule_parse(6053, "Capacity: 6, LoadAP: 5, FreeLoad: 15", &rule));
    CHECK_INT(rule.free_load, 15);
    CHECK(weapon_rule_parse(6053, "Capacity: 6, LoadAP: 5, FreeLoad: 250", &rule));
    CHECK_INT(rule.free_load, WEAPON_RULE_PERCENT_MAX);
    CHECK(weapon_rule_parse(6053, "FireAP: 4, FireAPMaster: 0", &rule));
    CHECK_INT(rule.fire_ap_master, WEAPON_RULE_AP_MIN);

    // A tier's cost with no base cost, or a free shot with no loading, is
    // half a rule.
    CHECK(!weapon_rule_parse(6128, "FireAPExpert: 5", &rule));
    CHECK(!weapon_rule_parse(6128, "Capacity: 1, LoadAP: 6, FireAPMaster: 4", &rule));
    CHECK(!weapon_rule_parse(6128, "LoadAPMaster: 3", &rule));
    CHECK(!weapon_rule_parse(6053, "FreeLoad: 15", &rule));
}

TEST(a_tier_falls_back_to_the_one_below)
{
    // The Rifle's firing cost: 6 untrained and at Apprentice, 5, 4.
    CHECK_INT(weapon_rule_tiered(6, 6, 5, 4, TRAINING_NONE), 6);
    CHECK_INT(weapon_rule_tiered(6, 6, 5, 4, TRAINING_APPRENTICE), 6);
    CHECK_INT(weapon_rule_tiered(6, 6, 5, 4, TRAINING_EXPERT), 5);
    CHECK_INT(weapon_rule_tiered(6, 6, 5, 4, TRAINING_MASTER), 4);
    // Unset tiers take the value below them, down to the untrained one.
    CHECK_INT(weapon_rule_tiered(6, WEAPON_RULE_UNSET, WEAPON_RULE_UNSET, 4, TRAINING_EXPERT), 6);
    CHECK_INT(weapon_rule_tiered(6, 5, WEAPON_RULE_UNSET, WEAPON_RULE_UNSET, TRAINING_MASTER), 5);
    CHECK_INT(weapon_rule_tiered(0, WEAPON_RULE_UNSET, 13, WEAPON_RULE_UNSET, TRAINING_MASTER), 13);
}

TEST(skill_damage_is_a_percentage_of_the_level_rounded_up)
{
    // 70% of levels 4, 8, 12, 16, 20 - one to five points: 3, 6, 9, 12, 14.
    CHECK_INT(weapon_rule_skill_damage_bonus(70, 4), 3);
    CHECK_INT(weapon_rule_skill_damage_bonus(70, 8), 6);
    CHECK_INT(weapon_rule_skill_damage_bonus(70, 12), 9);
    CHECK_INT(weapon_rule_skill_damage_bonus(70, 16), 12);
    CHECK_INT(weapon_rule_skill_damage_bonus(70, 20), 14);
    // The specials' 45%: 2, 4, 6, 8, 9.
    CHECK_INT(weapon_rule_skill_damage_bonus(45, 4), 2);
    CHECK_INT(weapon_rule_skill_damage_bonus(45, 20), 9);
    CHECK_INT(weapon_rule_skill_damage_bonus(45, 1), 1);
    CHECK_INT(weapon_rule_skill_damage_bonus(70, 0), 0);
    CHECK_INT(weapon_rule_skill_damage_bonus(WEAPON_RULE_UNSET, 20), 0);
    CHECK_INT(weapon_rule_skill_damage_bonus(0, 20), 0);
}

TEST(dodgeable_is_read_and_clamped)
{
    WeaponRule rule;

    // The Flame Thrower, zsogg 2026-10-02: a hit cannot be dodged.
    CHECK(weapon_rule_parse(6061, "Dodgeable: 0", &rule));
    CHECK_INT(rule.dodgeable, 0);
    CHECK(weapon_rule_parse(6061, "Damage: 5-10", &rule));
    CHECK_INT(rule.dodgeable, WEAPON_RULE_UNSET);
    CHECK(weapon_rule_parse(6061, "Dodgeable: 7", &rule));
    CHECK_INT(rule.dodgeable, 1);
    CHECK(weapon_rule_parse(6061, "Dodgeable: -1", &rule));
    CHECK_INT(rule.dodgeable, 0);
}

TEST(dodge_share_is_read_and_clamped)
{
    WeaponRule rule;

    // Card DG1, zsogg 2026-11-05: a firearm is dodged at 30%.
    CHECK(weapon_rule_parse(6060, "DodgeShare: 30", &rule));
    CHECK_INT(rule.dodge_share, 30);
    CHECK_INT(rule.dodgeable, WEAPON_RULE_UNSET);
    CHECK(weapon_rule_parse(6060, "Damage: 5-10", &rule));
    CHECK_INT(rule.dodge_share, WEAPON_RULE_UNSET);
    CHECK(weapon_rule_parse(6060, "DodgeShare: 250", &rule));
    CHECK_INT(rule.dodge_share, 100);
    CHECK(weapon_rule_parse(6060, "DodgeShare: -5", &rule));
    CHECK_INT(rule.dodge_share, 0);
}

TEST(armor_damage_is_read_and_clamped)
{
    WeaponRule rule;

    // The Acid Gun, zsogg 2026-09-28: 40 a hit, where the engine takes 10.
    CHECK(weapon_rule_parse(6173, "ArmorDamage: 40", &rule));
    CHECK_INT(rule.armor_damage, 40);
    CHECK(weapon_rule_parse(6173, "Damage: 15-38", &rule));
    CHECK_INT(rule.armor_damage, WEAPON_RULE_UNSET);
    CHECK(weapon_rule_parse(6173, "ArmorDamage: -3", &rule));
    CHECK_INT(rule.armor_damage, 0);
}

TEST(elemental_damage_and_ammunition_per_shot_are_read)
{
    WeaponRule rule;

    // The Charged Accelerator Gun, zsogg 2026-09-28.
    CHECK(weapon_rule_parse(6176,
        "Damage: 14-25, Electrical: 10-18, Fatigue: 5-10, AmmoPerShot: 2", &rule));
    CHECK_INT(rule.electrical_min, 10);
    CHECK_INT(rule.electrical_max, 18);
    CHECK_INT(rule.fire_min, WEAPON_RULE_UNSET);
    CHECK_INT(rule.poison_min, WEAPON_RULE_UNSET);
    CHECK_INT(rule.ammo_per_shot, 2);
    CHECK(weapon_rule_parse(6061, "Fire: 30-15, Poison: 4", &rule));
    CHECK_INT(rule.fire_min, 15);
    CHECK_INT(rule.fire_max, 30);
    CHECK_INT(rule.poison_min, 4);
    CHECK_INT(rule.poison_max, 4);
    CHECK_INT(rule.ammo_per_shot, WEAPON_RULE_UNSET);
    CHECK(weapon_rule_parse(6176, "AmmoPerShot: 0", &rule));
    CHECK_INT(rule.ammo_per_shot, WEAPON_RULE_AMMO_PER_SHOT_MIN);
}

TEST(a_malformed_entry_changes_nothing)
{
    WeaponRule rule;

    memset(&rule, 0x5a, sizeof(rule));
    CHECK(!weapon_rule_parse(6101, "Capacity: 1", &rule)); // no LoadAP
    CHECK(!weapon_rule_parse(6101, "LoadAP: 4", &rule)); // no Capacity
    CHECK(!weapon_rule_parse(6101, "LoadAPApprentice: 3, Damage: 5", &rule));
    CHECK(!weapon_rule_parse(6101, "FireAPApprentice: 3", &rule)); // no FireAP
    CHECK(!weapon_rule_parse(6101, "Capacity: one, LoadAP: 4", &rule));
    CHECK(!weapon_rule_parse(6101, "Damage: 12 to 19", &rule));
    CHECK(!weapon_rule_parse(6101, "Damage: 12-", &rule));
    CHECK(!weapon_rule_parse(6101, "Jams: 2", &rule)); // unknown key
    CHECK(!weapon_rule_parse(6101, "Range 18", &rule)); // no colon
    CHECK(!weapon_rule_parse(6101, "", &rule)); // nothing said
    CHECK_INT(rule.capacity, 0x5a5a5a5a);
}

// Card AP1 (zsogg, 2026-11-12): a weapon at a creature of the opposite lean
// loses 15% of its damage at strength 50 and 35% at 100.
TEST(the_aptitude_cut_is_15_at_half_strength_and_35_at_full)
{
    CHECK_INT(item_aptitude_cut(0), 0);
    CHECK_INT(item_aptitude_cut(-20), 0);
    CHECK_INT(item_aptitude_cut(31), 9); // the golem against Kal
    CHECK_INT(item_aptitude_cut(40), 12); // the golem against a neutral shooter
    CHECK_INT(item_aptitude_cut(50), 15);
    CHECK_INT(item_aptitude_cut(75), 25);
    CHECK_INT(item_aptitude_cut(100), 35);
    CHECK_INT(item_aptitude_cut(150), 35);
}

void test_weapon_rules_suite(void)
{
    RUN(an_entry_reads_every_key);
    RUN(an_entry_may_change_only_stats_or_only_reloading);
    RUN(an_apprentice_cost_defaults_to_the_untrained_one);
    RUN(a_single_number_is_both_ends_and_a_reversed_range_is_ordered);
    RUN(a_value_outside_its_range_is_clamped);
    RUN(accuracy_is_a_percentage);
    RUN(blunt_fatigue_and_range_keys_are_read_and_clamped);
    RUN(penetration_falls_off_beyond_its_range);
    RUN(minimum_strength_is_read_and_clamped);
    RUN(weight_is_read_and_clamped);
    RUN(penetration_is_read_and_clamped);
    RUN(whole_magazine_is_read_and_needs_a_capacity);
    RUN(clear_ap_is_read_clamped_and_needs_a_capacity);
    RUN(critical_chance_and_style_are_read);
    RUN(expertise_keys_are_read_and_need_their_base_cost);
    RUN(a_tier_falls_back_to_the_one_below);
    RUN(skill_damage_is_a_percentage_of_the_level_rounded_up);
    RUN(dodgeable_is_read_and_clamped);
    RUN(dodge_share_is_read_and_clamped);
    RUN(armor_damage_is_read_and_clamped);
    RUN(elemental_damage_and_ammunition_per_shot_are_read);
    RUN(a_malformed_entry_changes_nothing);
    RUN(the_aptitude_cut_is_15_at_half_strength_and_35_at_full);
}
