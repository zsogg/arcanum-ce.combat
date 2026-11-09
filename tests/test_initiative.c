// Initiative and Awareness (card X10; zsogg, 2026-10-20): `turn order
// initiative`'s rules, against the real `combat_initiative_score` and
// `combat_initiative_compare`, which `sort_combat_list` orders the fight by.
// The sort itself, over a live fight's list, is measured on the observatory.
#include <string.h>

#include "game/combat.h"
#include "game/random.h"
#include "test_harness.h"

TEST(a_score_is_dexterity_plus_the_roll)
{
    CHECK_INT(combat_initiative_score(12, 1), 13);
    CHECK_INT(combat_initiative_score(12, 10), 22);
}

TEST(the_higher_score_acts_first)
{
    CHECK(combat_initiative_compare(20, false, 12, 15, true, 14) < 0);
    CHECK(combat_initiative_compare(15, true, 14, 20, false, 12) > 0);
}

TEST(an_exact_tie_goes_to_the_party)
{
    CHECK(combat_initiative_compare(18, true, 10, 18, false, 15) < 0);
    CHECK(combat_initiative_compare(18, false, 15, 18, true, 10) > 0);
}

TEST(then_to_the_higher_dexterity_then_the_list_order)
{
    CHECK(combat_initiative_compare(18, false, 15, 18, false, 10) < 0);
    CHECK(combat_initiative_compare(18, true, 10, 18, true, 15) > 0);
    // Equal in everything: neither moves, so the list's own order stands.
    CHECK_INT(combat_initiative_compare(18, false, 12, 18, false, 12), 0);
}

// Review 2026-11-08, turns-6: a roll comes from the fight's seed and the
// critter's ID, so a load mid-fight rebuilds the order it saved.
static ObjectID initiative_p_id(int temp_id)
{
    ObjectID oid;

    memset(&oid, 0, sizeof(oid));
    oid.type = OID_TYPE_P;
    oid.d.p.location = 0x0000123400005678LL;
    oid.d.p.temp_id = temp_id;
    oid.d.p.map = 4;
    return oid;
}

static ObjectID initiative_guid_id(void)
{
    ObjectID oid;
    int index;

    memset(&oid, 0, sizeof(oid));
    oid.type = OID_TYPE_GUID;
    for (index = 0; index < (int)sizeof(oid.d.g); index++) {
        ((unsigned char*)&oid.d.g)[index] = (unsigned char)(index * 37 + 11);
    }
    return oid;
}

TEST(a_roll_does_not_draw_from_the_game_rng)
{
    ObjectID oid = initiative_guid_id();
    int a;
    int b;

    random_seed(1);
    a = combat_initiative_roll(77, oid);
    CHECK(random_state_get() == 1);
    random_seed(2);
    b = combat_initiative_roll(77, oid);
    CHECK_INT(a, b);
}

TEST(a_roll_is_one_to_ten)
{
    ObjectID guid = initiative_guid_id();
    ObjectID p = initiative_p_id(9);
    ObjectID a;
    unsigned int seed;
    int roll;

    memset(&a, 0, sizeof(a));
    a.type = OID_TYPE_A;
    a.d.a = 2031;
    for (seed = 0; seed <= 0x7FFF; seed++) {
        roll = combat_initiative_roll(seed, guid);
        CHECK(roll >= 1 && roll <= 10);
        roll = combat_initiative_roll(seed, p);
        CHECK(roll >= 1 && roll <= 10);
        roll = combat_initiative_roll(seed, a);
        CHECK(roll >= 1 && roll <= 10);
    }
}

TEST(a_roll_ignores_the_ids_padding)
{
    ObjectID a;
    ObjectID b;

    memset(&a, 0, sizeof(a));
    memset(&b, 0xAB, sizeof(b));
    a.type = b.type = OID_TYPE_A;
    a.d.a = b.d.a = 2031;
    CHECK_INT(combat_initiative_roll(5, a), combat_initiative_roll(5, b));
    a = initiative_p_id(3);
    b = a;
    b.padding_2 = 7;
    b.padding_4 = 99;
    CHECK_INT(combat_initiative_roll(5, a), combat_initiative_roll(5, b));
}

TEST(rolls_spread_evenly)
{
    ObjectID guid = initiative_guid_id();
    int counts[11] = { 0 };
    unsigned int seed;
    int temp_id;
    int value;

    for (seed = 0; seed <= 0x7FFF; seed++) {
        counts[combat_initiative_roll(seed, guid)]++;
    }
    for (value = 1; value <= 10; value++) {
        CHECK(counts[value] >= 3000 && counts[value] <= 3560);
    }

    memset(counts, 0, sizeof(counts));
    for (temp_id = 0; temp_id < 10000; temp_id++) {
        counts[combat_initiative_roll(1234, initiative_p_id(temp_id))]++;
    }
    for (value = 1; value <= 10; value++) {
        CHECK(counts[value] >= 900 && counts[value] <= 1100);
    }
}

TEST(fights_and_critters_roll_apart)
{
    ObjectID guid = initiative_guid_id();
    bool seen[11] = { false };
    unsigned int seed;
    int temp_id;
    int distinct = 0;
    int value;

    for (seed = 0; seed < 100; seed++) {
        seen[combat_initiative_roll(seed, guid)] = true;
    }
    for (value = 1; value <= 10; value++) {
        distinct += seen[value] ? 1 : 0;
    }
    CHECK(distinct >= 5);

    memset(seen, 0, sizeof(seen));
    distinct = 0;
    for (temp_id = 0; temp_id < 100; temp_id++) {
        seen[combat_initiative_roll(42, initiative_p_id(temp_id))] = true;
    }
    for (value = 1; value <= 10; value++) {
        distinct += seen[value] ? 1 : 0;
    }
    CHECK(distinct >= 5);
}

TEST(neighbouring_critters_can_tie)
{
    int ties = 0;
    int temp_id;

    for (temp_id = 0; temp_id < 10000; temp_id++) {
        if (combat_initiative_roll(42, initiative_p_id(temp_id))
            == combat_initiative_roll(42, initiative_p_id(temp_id + 1))) {
            ties++;
        }
    }
    CHECK(ties >= 800 && ties <= 1200);
}

void test_initiative_suite(void)
{
    RUN(a_roll_does_not_draw_from_the_game_rng);
    RUN(a_roll_is_one_to_ten);
    RUN(a_roll_ignores_the_ids_padding);
    RUN(rolls_spread_evenly);
    RUN(fights_and_critters_roll_apart);
    RUN(neighbouring_critters_can_tie);
    RUN(a_score_is_dexterity_plus_the_roll);
    RUN(the_higher_score_acts_first);
    RUN(an_exact_tie_goes_to_the_party);
    RUN(then_to_the_higher_dexterity_then_the_list_order);
}
