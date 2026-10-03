// The combat rebalance's weapon table, `rules/weapons.mes`: what an entry may
// say, pure. The engine side - the overrides at each stat, reloading - is
// exercised in scripted runs.

#include <string.h>

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

void test_weapon_rules_suite(void)
{
    RUN(an_entry_reads_every_key);
    RUN(an_entry_may_change_only_stats_or_only_reloading);
    RUN(an_apprentice_cost_defaults_to_the_untrained_one);
    RUN(a_single_number_is_both_ends_and_a_reversed_range_is_ordered);
    RUN(a_value_outside_its_range_is_clamped);
    RUN(accuracy_is_a_percentage);
    RUN(minimum_strength_is_read_and_clamped);
    RUN(weight_is_read_and_clamped);
    RUN(dodgeable_is_read_and_clamped);
    RUN(armor_damage_is_read_and_clamped);
    RUN(elemental_damage_and_ammunition_per_shot_are_read);
    RUN(a_malformed_entry_changes_nothing);
}
