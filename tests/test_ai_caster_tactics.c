// Combat AI AI-7, `caster tactics` (arcanum-work tasks/AI7-caster-tactics.md):
// the rules' pure decisions. Which critters count and when a target is
// disabled are read from game objects, and are measured by the benches.

#include "game/ai.h"

#include "test_harness.h"

TEST(an_area_spell_spares_the_allies_unless_two_enemies_go_with_them)
{
    CHECK(!ai_tactics_area_refuses(0, 0)); // nobody but the target's side, or nobody
    CHECK(!ai_tactics_area_refuses(0, 1));
    CHECK(ai_tactics_area_refuses(1, 0));
    CHECK(ai_tactics_area_refuses(1, 1));
    CHECK(!ai_tactics_area_refuses(1, 2)); // zsogg, 2026-11-02: two or more enemies
    CHECK(!ai_tactics_area_refuses(3, 2));
}

TEST(a_target_has_just_recovered_within_one_turn)
{
    // Turn-based: rounds count up.
    CHECK(ai_tactics_just_recovered(4, 4));
    CHECK(ai_tactics_just_recovered(4, 5));
    CHECK(!ai_tactics_just_recovered(4, 6));
    // Real time: `ai_desperation_key` counts down.
    CHECK(ai_tactics_just_recovered(-10, -11));
    CHECK(!ai_tactics_just_recovered(-10, -12));
}

// Review 2026-11-08, cross-1: kin first; then a fighting critter is on the
// side opposite its focus, so a faction-0 wolf biting the player is a
// follower's enemy, and a faction-0 bandit fighting beside a bandit mage is
// its ally.
TEST(a_fighting_critter_is_on_the_side_opposite_its_focus)
{
    CHECK_INT(ai_side_rule(true, false, true, true, false), AI_SIDE_CASTER);
    CHECK_INT(ai_side_rule(true, true, false, false, false), AI_SIDE_CASTER);
    CHECK_INT(ai_side_rule(false, true, true, true, false), AI_SIDE_TARGET);
    CHECK_INT(ai_side_rule(false, false, true, true, false), AI_SIDE_TARGET);
    CHECK_INT(ai_side_rule(false, false, true, false, true), AI_SIDE_CASTER);
    CHECK_INT(ai_side_rule(false, false, false, true, false), AI_SIDE_NONE);
    CHECK_INT(ai_side_rule(false, false, false, false, true), AI_SIDE_NONE);
    CHECK_INT(ai_side_rule(false, false, true, false, false), AI_SIDE_NONE);
    CHECK_INT(ai_side_rule(false, false, true, true, true), AI_SIDE_TARGET);
}

void test_ai_caster_tactics_suite(void)
{
    RUN(a_fighting_critter_is_on_the_side_opposite_its_focus);
    RUN(an_area_spell_spares_the_allies_unless_two_enemies_go_with_them);
    RUN(a_target_has_just_recovered_within_one_turn);
}
