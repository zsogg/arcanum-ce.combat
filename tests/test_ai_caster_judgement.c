// Combat AI AI-8, `caster judgement` (arcanum-work tasks/AI8-caster-judgement.md):
// the score's pure arithmetic. Which critters count, the spells' damage and
// their chance to land are read from game objects, and are measured by the
// benches.

#include <stdlib.h>

#include "game/ai.h"
#include "game/magictech.h"

#include "test_harness.h"

TEST(damage_is_worth_what_the_target_has_left_and_a_kill_more)
{
    CHECK_INT(ai_judgement_damage_value(30, 100, 100), 30);
    CHECK_INT(ai_judgement_damage_value(30, 100, 50), 15);
    // Overkill counts only what is left, plus the kill.
    CHECK_INT(ai_judgement_damage_value(30000, 40, 100), 60);
    CHECK_INT(ai_judgement_damage_value(40, 40, 100), 60);
    CHECK_INT(ai_judgement_damage_value(0, 40, 100), 0);
    CHECK_INT(ai_judgement_damage_value(30, 0, 100), 0); // already down
}

TEST(a_disabling_spell_is_worth_half_its_target_by_its_ai_value)
{
    CHECK_INT(ai_judgement_control_value(100, 20, 100), 50);
    CHECK_INT(ai_judgement_control_value(100, 30, 100), 50); // capped at 20
    CHECK_INT(ai_judgement_control_value(100, 10, 100), 25);
    CHECK_INT(ai_judgement_control_value(100, 10, 0), 0);
    CHECK_INT(ai_judgement_control_value(100, -1, 100), 0);
}

TEST(a_summon_is_worth_more_as_the_enemy_closes)
{
    CHECK_INT(ai_judgement_summon_value(8, 3), 96);
    CHECK_INT(ai_judgement_summon_value(8, 8), 96);
    CHECK_INT(ai_judgement_summon_value(8, 20), 19);
    CHECK_INT(ai_judgement_summon_value(8, 40), 19);
    CHECK(ai_judgement_summon_value(8, 14) < ai_judgement_summon_value(8, 10));
    CHECK(ai_judgement_summon_value(8, 14) > ai_judgement_summon_value(8, 18));
    CHECK_INT(ai_judgement_summon_value(-1, 3), 0);
}

TEST(a_buff_is_worth_what_threatens)
{
    CHECK_INT(ai_judgement_buff_value(0, 2), 0); // nobody threatens: not worth casting
    CHECK_INT(ai_judgement_buff_value(1, 0), 10);
    CHECK_INT(ai_judgement_buff_value(5, 0), 50);
    CHECK_INT(ai_judgement_buff_value(9, 0), 50); // at most 5 threats
    CHECK_INT(ai_judgement_buff_value(1, 15), 40);
}

TEST(the_score_divides_by_the_cost_and_two_upkeep_periods)
{
    CHECK_INT(ai_judgement_score(150, 15, 0), 1000);
    CHECK_INT(ai_judgement_score(150, 25, 10), 333); // Body of Fire's 25 + 2 x 10
    CHECK_INT(ai_judgement_score(10, 0, 0), 1000); // a free cast divides by 1
}

TEST(fireflash_into_a_crowd_beats_an_elemental_far_off)
{
    // The demonstration battle: Fireflash (15 fatigue) into five riflemen at
    // 30 each against Call Fire Elemental (50 + 5 upkeep) with the enemy 20
    // tiles off, and against the same summon with the enemy at hand.
    int fireflash = ai_judgement_score(5 * ai_judgement_damage_value(30, 60, 90), 15, 0);
    int far_summon = ai_judgement_score(ai_judgement_summon_value(8, 20), 50, 5);
    int near_summon = ai_judgement_score(ai_judgement_summon_value(8, 4), 50, 5);
    int lone_fireflash = ai_judgement_score(ai_judgement_damage_value(30, 60, 90), 15, 0);

    CHECK(fireflash > far_summon);
    CHECK(fireflash > near_summon);
    CHECK(lone_fireflash > far_summon);
    CHECK(near_summon > far_summon);
}

TEST(the_budget_keeps_back_the_best_attacks)
{
    // 60 left, a reserve of 45 (three Fireflashes): a 50-point summon scoring
    // less than Fireflash is refused; Fireflash itself never is.
    CHECK(ai_judgement_budget_refuses(60, 50, 45, 100, 1000));
    CHECK(!ai_judgement_budget_refuses(60, 15, 45, 1000, 1000));
    CHECK(!ai_judgement_budget_refuses(200, 50, 45, 100, 1000)); // plenty left
    CHECK(!ai_judgement_budget_refuses(60, 50, 45, 1200, 1000)); // scores better
    CHECK(!ai_judgement_budget_refuses(60, 50, 0, 100, 1000)); // no attack to keep for
}

TEST(a_turn_is_not_spent_on_a_buff_that_strands_the_attack)
{
    // 7 AP left: a 5-AP buff leaves 2, too few for a 5-AP attack worth more.
    CHECK(ai_judgement_turn_refuses(7, 5, 5, 40, 150));
    CHECK(!ai_judgement_turn_refuses(10, 5, 5, 40, 150)); // both fit
    CHECK(!ai_judgement_turn_refuses(7, 5, 5, 200, 150)); // the buff is worth more
    CHECK(!ai_judgement_turn_refuses(7, 5, 0, 40, 150)); // no attack this turn
}

// Quench Life's 50-100 is in its [Callback], not its begin (review
// 2026-11-08, casting-4): it was weighed as a disable.
#define CALLBACK_DAMAGE_SPELL 22

TEST(a_spell_s_callback_damage_is_counted)
{
    MagicTechComponentInfo damage = { 0 };

    magictech_spells = (MagicTechInfo*)calloc(MT_SPELL_COUNT, sizeof(MagicTechInfo));
    damage.type = MTC_DAMAGE;
    damage.data.damage.damage_type = DAMAGE_TYPE_NORMAL;
    damage.data.damage.damage_min = 50;
    damage.data.damage.damage_max = 100;
    magictech_spells[CALLBACK_DAMAGE_SPELL].components[MAGICTECH_ACTION_CALLBACK].cnt = 1;
    magictech_spells[CALLBACK_DAMAGE_SPELL].components[MAGICTECH_ACTION_CALLBACK].entries = &damage;

    CHECK_INT(magictech_ai_expected_damage(OBJ_HANDLE_NULL, OBJ_HANDLE_NULL, CALLBACK_DAMAGE_SPELL), 75);

    free(magictech_spells);
    magictech_spells = NULL;
}

void test_ai_caster_judgement_suite(void)
{
    RUN(damage_is_worth_what_the_target_has_left_and_a_kill_more);
    RUN(a_disabling_spell_is_worth_half_its_target_by_its_ai_value);
    RUN(a_summon_is_worth_more_as_the_enemy_closes);
    RUN(a_buff_is_worth_what_threatens);
    RUN(the_score_divides_by_the_cost_and_two_upkeep_periods);
    RUN(fireflash_into_a_crowd_beats_an_elemental_far_off);
    RUN(the_budget_keeps_back_the_best_attacks);
    RUN(a_turn_is_not_spent_on_a_buff_that_strands_the_attack);
    RUN(a_spell_s_callback_damage_is_counted);
}
