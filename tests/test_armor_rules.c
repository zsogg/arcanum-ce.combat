// Armor milestone 2: the armor table's entries, pure. The engine side - which
// worn piece a hit meets, and the switch - is exercised in scripted runs.

#include "game/armor_rules.h"

#include "test_harness.h"

TEST(a_piece_takes_a_protection)
{
    ArmorRule rule;

    CHECK(armor_rule_parse(8094, "Protection: 20", &rule));
    CHECK_INT(rule.id, 8094);
    CHECK_INT(rule.protection, 20);
    CHECK_INT(rule.block_chance, ARMOR_RULE_UNSET);
    CHECK_INT(rule.penetration, ARMOR_RULE_UNSET);

    // Spaces around keys and values, as the generator writes them.
    CHECK(armor_rule_parse(8094, " Protection : 7 ", &rule));
    CHECK_INT(rule.protection, 7);
}

TEST(a_shield_takes_a_block_chance_too)
{
    ArmorRule rule;

    CHECK(armor_rule_parse(8201, "Protection: 25, BlockChance: 18", &rule));
    CHECK_INT(rule.protection, 25);
    CHECK_INT(rule.block_chance, 18);
}

TEST(a_kind_takes_a_penetration_and_nothing_else)
{
    ArmorRule rule;

    CHECK(armor_rule_parse(ARMOR_RULE_KIND_MELEE, "Penetration: 30", &rule));
    CHECK_INT(rule.penetration, 30);
    CHECK(armor_rule_parse(ARMOR_RULE_KIND_FIREARM, "Penetration: 40", &rule));
    CHECK_INT(rule.id, ARMOR_RULE_KIND_FIREARM);

    CHECK(!armor_rule_parse(ARMOR_RULE_KIND_BOW, "Protection: 30", &rule));
    CHECK(!armor_rule_parse(ARMOR_RULE_KIND_BOW, "Penetration: 30, BlockChance: 5", &rule));
    CHECK(!armor_rule_parse(ARMOR_RULE_KIND_BOW, "", &rule));
}

TEST(malformed_entries_are_refused_and_change_nothing)
{
    ArmorRule rule;

    rule.id = 1234;
    rule.protection = 99;
    // A piece with a penetration, or without a protection.
    CHECK(!armor_rule_parse(8094, "Penetration: 30", &rule));
    CHECK(!armor_rule_parse(8094, "BlockChance: 5", &rule));
    // An unknown key, a missing colon, a value that is not a number.
    CHECK(!armor_rule_parse(8094, "Protection: 20, Thickness: 3", &rule));
    CHECK(!armor_rule_parse(8094, "Protection 20", &rule));
    CHECK(!armor_rule_parse(8094, "Protection: lots", &rule));
    // An id that is neither a kind nor a prototype.
    CHECK(!armor_rule_parse(0, "Penetration: 30", &rule));
    CHECK(!armor_rule_parse(6, "Penetration: 30", &rule));
    CHECK(!armor_rule_parse(20, "Protection: 30", &rule));
    CHECK(!armor_rule_parse(8094, NULL, &rule));

    CHECK_INT(rule.id, 1234);
    CHECK_INT(rule.protection, 99);
}

TEST(values_are_clamped)
{
    ArmorRule rule;

    CHECK(armor_rule_parse(8201, "Protection: 5000, BlockChance: 250", &rule));
    CHECK_INT(rule.protection, ARMOR_RULE_VALUE_MAX);
    CHECK_INT(rule.block_chance, ARMOR_RULE_CHANCE_MAX);
    CHECK(armor_rule_parse(8201, "Protection: -3, BlockChance: -1", &rule));
    CHECK_INT(rule.protection, 0);
    CHECK_INT(rule.block_chance, 0);
    CHECK(armor_rule_parse(ARMOR_RULE_KIND_THROWN, "Penetration: -9", &rule));
    CHECK_INT(rule.penetration, 0);
}

void test_armor_rules_suite(void)
{
    RUN(a_piece_takes_a_protection);
    RUN(a_shield_takes_a_block_chance_too);
    RUN(a_kind_takes_a_penetration_and_nothing_else);
    RUN(malformed_entries_are_refused_and_change_nothing);
    RUN(values_are_clamped);
}
