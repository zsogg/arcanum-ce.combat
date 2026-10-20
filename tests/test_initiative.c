// Initiative and Awareness (card X10; zsogg, 2026-10-20): `turn order
// initiative`'s rules, against the real `combat_initiative_score` and
// `combat_initiative_compare`, which `sort_combat_list` orders the fight by.
// The sort itself, over a live fight's list, is measured on the observatory.
#include "game/combat.h"
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

void test_initiative_suite(void)
{
    RUN(a_score_is_dexterity_plus_the_roll);
    RUN(the_higher_score_acts_first);
    RUN(an_exact_tie_goes_to_the_party);
    RUN(then_to_the_higher_dexterity_then_the_list_order);
}
