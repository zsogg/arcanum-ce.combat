// Armor milestone 1: penetration against protection, pure. Every case is a
// worked example from arcanum-work COMBAT-REBALANCE-DESIGN-DISCUSSION.md,
// "Armor penetration - current design direction", so a change to the rules
// shows up as a failure here before it shows up in a fight.

#include "game/combat.h"
#include "game/penetration.h"

#include "test_harness.h"

static PenetrationHit torso_hit(int body, int penetration, int damage)
{
    PenetrationHit hit = { 0 };

    hit.hit_loc = HIT_LOC_TORSO;
    hit.worn.body = body;
    hit.penetration = penetration;
    hit.variation_percent = 100;
    hit.damage = damage;
    return hit;
}

TEST(the_curve_against_protection_40)
{
    // The design's table: a 12-damage hit against protection 40.
    PenetrationHit hit = torso_hit(40, 20, 12);
    PenetrationResult result;

    result = penetration_resolve(&hit);
    CHECK_INT(result.damage, 0); // 20 or less: nothing

    hit.penetration = 30;
    result = penetration_resolve(&hit);
    CHECK_INT(result.passed_num * 6, result.passed_den); // 1/6
    CHECK_INT(result.damage, 2);

    hit.penetration = 40;
    result = penetration_resolve(&hit);
    CHECK_INT(result.passed_num * 3, result.passed_den); // 1/3 at equality
    CHECK_INT(result.damage, 4);

    hit.penetration = 50;
    result = penetration_resolve(&hit);
    CHECK_INT(result.passed_num * 3, result.passed_den * 2); // 2/3
    CHECK_INT(result.damage, 8);

    hit.penetration = 60;
    result = penetration_resolve(&hit);
    CHECK_INT(result.damage, 12); // 60 or more: everything

    hit.penetration = 200;
    result = penetration_resolve(&hit);
    CHECK_INT(result.damage, 12); // never more than the hit's damage
}

TEST(no_protection_passes_everything)
{
    PenetrationHit hit = torso_hit(0, 0, 9);

    CHECK_INT(penetration_resolve(&hit).damage, 9);
}

TEST(limbs_weigh_body_armor_and_the_limb_piece)
{
    // The design's illustration: body 50, gloves 30, boots 20.
    PenetrationWorn worn = { 0 };

    worn.body = 50;
    worn.gloves = 30;
    worn.boots = 20;
    worn.helmet = 15;
    CHECK_INT(penetration_location_protection_centi(HIT_LOC_ARM, &worn, 0), 3800);
    CHECK_INT(penetration_location_protection_centi(HIT_LOC_LEG, &worn, 0), 3200);
    CHECK_INT(penetration_location_protection_centi(HIT_LOC_TORSO, &worn, 0), 5000);
    CHECK_INT(penetration_location_protection_centi(HIT_LOC_HEAD, &worn, 0), 1500);

    // An empty slot gives nothing, and its weight does not move.
    worn.gloves = 0;
    CHECK_INT(penetration_location_protection_centi(HIT_LOC_ARM, &worn, 0), 2000);

    // Gloves or boots alone give 60% of their own rating.
    worn.body = 0;
    worn.boots = 20;
    CHECK_INT(penetration_location_protection_centi(HIT_LOC_LEG, &worn, 0), 1200);

    // Natural protection adds at every location.
    CHECK_INT(penetration_location_protection_centi(HIT_LOC_LEG, &worn, 10), 2200);
}

TEST(the_order_of_resolution_example)
{
    // Body armor 40, magical 10, a sword with penetration 40.
    PenetrationHit hit = torso_hit(40, 40, 10);
    PenetrationResult result;

    // An ordinary torso hit meets 50: ratio 0.8, 20% passed.
    hit.magic = 10;
    result = penetration_resolve(&hit);
    CHECK_INT(result.protection_centi, 5000);
    CHECK_INT(result.passed_num * 5, result.passed_den);
    CHECK_INT(result.damage, 2);

    // A critical finding a gap meets 40 x 0.35 + 10 = 24: ratio 1.67, all of
    // it, the critical's bonus included in the damage.
    hit.gap = true;
    hit.damage = 20;
    result = penetration_resolve(&hit);
    CHECK_INT(result.protection_centi, 2400);
    CHECK_INT(result.damage, 20);
}

TEST(a_gap_cuts_natural_protection_but_never_magic)
{
    PenetrationHit hit = torso_hit(20, 0, 10);

    hit.natural = 20;
    hit.magic = 30;
    hit.gap = true;
    // (20 + 20) x 0.35 + 30.
    CHECK_INT(penetration_resolve(&hit).protection_centi, 4400);
}

TEST(a_block_takes_the_higher_rating_and_allows_no_gap)
{
    PenetrationHit hit = torso_hit(20, 40, 12);
    PenetrationResult result;

    hit.blocked = true;
    hit.shield = 50;
    hit.gap = true; // a block allows no gap
    result = penetration_resolve(&hit);
    CHECK_INT(result.protection_centi, 5000);

    // A weaker shield than the armor there leaves the armor's rating.
    hit.shield = 10;
    result = penetration_resolve(&hit);
    CHECK_INT(result.protection_centi, 2000);

    // Magic still adds after a block.
    hit.magic = 5;
    result = penetration_resolve(&hit);
    CHECK_INT(result.protection_centi, 2500);
}

TEST(variation_moves_penetration_not_protection)
{
    // Nominal 40 against 40, varied to 32 and to 48.
    PenetrationHit hit = torso_hit(40, 40, 30);
    PenetrationResult result;

    hit.variation_percent = 80;
    result = penetration_resolve(&hit);
    CHECK_INT(result.penetration_centi, 3200);
    CHECK_INT(result.protection_centi, 4000);
    CHECK_INT(result.passed_num * 5, result.passed_den); // 20%
    CHECK_INT(result.damage, 6);

    hit.variation_percent = 120;
    result = penetration_resolve(&hit);
    CHECK_INT(result.passed_num * 5, result.passed_den * 3); // 60%
    CHECK_INT(result.damage, 18);

    // Out-of-range variation is held to 80-120.
    hit.variation_percent = 500;
    CHECK_INT(penetration_resolve(&hit).penetration_centi, 4800);
    hit.variation_percent = 0;
    CHECK_INT(penetration_resolve(&hit).penetration_centi, 3200);
}

TEST(damage_rounds_to_the_nearest_point)
{
    // 1/6 of 9 is 1.5: halves round up. 1/6 of 8 is 1.33: down.
    PenetrationHit hit = torso_hit(40, 30, 9);

    CHECK_INT(penetration_resolve(&hit).damage, 2);
    hit.damage = 8;
    CHECK_INT(penetration_resolve(&hit).damage, 1);
}

TEST(the_gap_chance)
{
    CHECK_INT(penetration_gap_chance(0, HIT_LOC_TORSO, false), 50);
    CHECK_INT(penetration_gap_chance(10, HIT_LOC_TORSO, false), 60);
    CHECK_INT(penetration_gap_chance(0, HIT_LOC_ARM, true), 60);
    CHECK_INT(penetration_gap_chance(0, HIT_LOC_LEG, true), 60);
    CHECK_INT(penetration_gap_chance(0, HIT_LOC_HEAD, true), 70);
    // An unaimed head critical takes no aiming bonus.
    CHECK_INT(penetration_gap_chance(0, HIT_LOC_HEAD, false), 50);
    CHECK_INT(penetration_gap_chance(90, HIT_LOC_HEAD, true), 100);
    CHECK_INT(penetration_gap_chance(-80, HIT_LOC_TORSO, false), 0);
}

// Combat AI milestone AI-1: the expected share passed, against a Python
// replica of the same integer arithmetic (2026-10-04).
TEST(the_expected_share_averages_locations_rolls_and_blocks)
{
    PenetrationHit hit = { 0 };

    hit.penetration = 24;
    CHECK_INT(penetration_expected_permille(&hit, 0), 1000); // unarmored

    hit.worn.body = 36; // plate body only: the bare head passes everything
    CHECK_INT(penetration_expected_permille(&hit, 0), 370);

    hit.worn.helmet = 36; // a matched plate suit
    hit.worn.gloves = 24;
    hit.worn.boots = 30;
    CHECK_INT(penetration_expected_permille(&hit, 0), 129);
    hit.penetration = 46; // a mace against it
    CHECK_INT(penetration_expected_permille(&hit, 0), 746);

    hit.penetration = 24; // and a Quality Iron Shield, 45 on 30%
    hit.shield = 45;
    CHECK_INT(penetration_expected_permille(&hit, 30), 99);
    CHECK_INT(penetration_expected_permille(&hit, 250), penetration_expected_permille(&hit, 100));

    // Nothing gets through natural protection 40 at penetration 10.
    hit = (PenetrationHit) { 0 };
    hit.natural = 40;
    hit.penetration = 10;
    CHECK_INT(penetration_expected_permille(&hit, 0), 0);
}

void test_penetration_suite(void)
{
    RUN(the_expected_share_averages_locations_rolls_and_blocks);
    RUN(the_curve_against_protection_40);
    RUN(no_protection_passes_everything);
    RUN(limbs_weigh_body_armor_and_the_limb_piece);
    RUN(the_order_of_resolution_example);
    RUN(a_gap_cuts_natural_protection_but_never_magic);
    RUN(a_block_takes_the_higher_rating_and_allows_no_gap);
    RUN(variation_moves_penetration_not_protection);
    RUN(damage_rounds_to_the_nearest_point);
    RUN(the_gap_chance);
}
