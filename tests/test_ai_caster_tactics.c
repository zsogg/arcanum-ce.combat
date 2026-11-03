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

void test_ai_caster_tactics_suite(void)
{
    RUN(an_area_spell_spares_the_allies_unless_two_enemies_go_with_them);
    RUN(a_target_has_just_recovered_within_one_turn);
}
