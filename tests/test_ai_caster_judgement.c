// Combat AI AI-8, `caster judgement` (arcanum-work tasks/AI8-caster-judgement.md):
// the score's pure arithmetic. Which critters count, the spells' damage and
// their chance to land are read from game objects, and are measured by the
// benches.

#include <stdlib.h>

#include "game/ai.h"
#include "game/combat.h"
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

TEST(an_area_spell_counts_the_allies_it_does_not_spare)
{
    // Handles stand in: the player, its follower (the caster), the follower's
    // summon, a town guard fighting the same bandits.
    const int64_t player = 101;
    const int64_t summon = 103;
    const int64_t guard = 104;

    CHECK(ai_judgement_area_spares(player, player, OBJ_HANDLE_NULL, false));
    CHECK(ai_judgement_area_spares(player, summon, player, false)); // a follower's follower
    CHECK(!ai_judgement_area_spares(player, guard, OBJ_HANDLE_NULL, false));
    CHECK(!ai_judgement_area_spares(player, summon, player, true)); // a blast takes the party
    CHECK(!ai_judgement_area_spares(OBJ_HANDLE_NULL, guard, OBJ_HANDLE_NULL, false)); // an enemy caster
    CHECK(!ai_judgement_area_spares(OBJ_HANDLE_NULL, OBJ_HANDLE_NULL, OBJ_HANDLE_NULL, false));

    CHECK_INT(ai_judgement_area_worth(AI_SIDE_TARGET, false, false, 30), 30);
    CHECK_INT(ai_judgement_area_worth(AI_SIDE_TARGET, true, false, 30), 0); // downed
    CHECK_INT(ai_judgement_area_worth(AI_SIDE_CASTER, false, false, 30), -60); // the guard
    CHECK_INT(ai_judgement_area_worth(AI_SIDE_CASTER, false, true, 30), 0); // the party
    CHECK_INT(ai_judgement_area_worth(AI_SIDE_NONE, false, false, 30), 0);
    // Two bandits and the guard in the blast: not worth it.
    CHECK(ai_judgement_area_worth(AI_SIDE_TARGET, false, false, 30) * 2
            + ai_judgement_area_worth(AI_SIDE_CASTER, false, false, 30)
        <= 0);
}

TEST(the_budget_keeps_back_the_best_attacks)
{
    // 60 left, a reserve of 45 (three Fireflashes): a 50-point summon is
    // refused whatever it scores (review 2026-11-08, casting-1).
    CHECK(ai_judgement_budget_refuses(60, 50, 45));
    CHECK(!ai_judgement_budget_refuses(200, 50, 45)); // plenty left
    CHECK(!ai_judgement_budget_refuses(60, 50, 0)); // no attack to keep for
    CHECK(!ai_judgement_budget_refuses(60, 0, 45)); // an item's cast is free
}

TEST(a_heal_keeps_to_the_reserve_unless_the_patient_is_dying)
{
    // Review 2026-11-08, healing-3.
    CHECK(ai_judgement_heal_refuses(60, 20, 45, 50));
    CHECK(!ai_judgement_heal_refuses(60, 20, 45, 30));
    CHECK(!ai_judgement_heal_refuses(60, 20, 45, 25));
    CHECK(!ai_judgement_heal_refuses(100, 20, 45, 50));
    CHECK(!ai_judgement_heal_refuses(60, 20, 0, 50));
    CHECK(!ai_judgement_heal_refuses(60, 0, 45, 50));
}

// Review 2026-11-08, casting-2 (zsogg 2026-11-09): a disable is worth the
// damage it denies.
TEST(a_disable_is_worth_the_damage_it_denies)
{
    // 10 a blow, two blows a turn, held two turns, at 85%.
    CHECK_INT(ai_judgement_disable_value(10, 10, 5, 2, 100, 60, 85), 34);
    CHECK_INT(ai_judgement_disable_value(10, 10, 5, 2, 50, 60, 85), 17); // a debuff
    CHECK_INT(ai_judgement_disable_value(10, 10, 5, 3, 100, 10, 100), 30); // never more than the kill
    CHECK_INT(ai_judgement_disable_value(10, 10, 0, 1, 100, 60, 100), 10); // an unknown blow: one
    CHECK_INT(ai_judgement_disable_value(10, 4, 5, 1, 100, 60, 100), 10); // short of a blow: one
    CHECK_INT(ai_judgement_disable_value(0, 10, 5, 2, 100, 60, 100), 0);
    CHECK_INT(ai_judgement_disable_value(10, 10, 5, 0, 100, 60, 100), 0);
    CHECK_INT(ai_judgement_disable_value(10, 10, 5, 2, 0, 60, 100), 0);
    CHECK_INT(ai_judgement_disable_value(10, 10, 5, 2, 100, 0, 100), 0);
}

TEST(a_hold_lasts_its_turns)
{
    CHECK_INT(ai_judgement_hold_turns(true, 6, false), 2);
    CHECK_INT(ai_judgement_hold_turns(true, 9, false), 3);
    CHECK_INT(ai_judgement_hold_turns(true, 12, false), AI_JUDGEMENT_HOLD_TURNS);
    CHECK_INT(ai_judgement_hold_turns(true, 0, true), 1); // Constitution 19 and up: the animation's turn
    CHECK_INT(ai_judgement_hold_turns(true, 2, false), 1);
    CHECK_INT(ai_judgement_hold_turns(false, 0, true), AI_JUDGEMENT_HOLD_TURNS);
    CHECK_INT(ai_judgement_hold_turns(false, 0, false), 1);
}

TEST(the_mental_mage_stuns_the_warrior)
{
    // The review's scenario: Stun at a 60-hit-point warrior swinging 10 a
    // blow twice a turn, a mage with fists. The 2026-11-08 worth was 8, under
    // the floor; the damage it denies is 34.
    int stun = ai_judgement_disable_value(10, 10, 5, ai_judgement_hold_turns(true, 6, false), 100, 60, 85);

    CHECK_INT(ai_judgement_control_value(60, 7, 85), 8);
    CHECK_INT(stun, 34);
    CHECK(stun >= ai_judgement_floor(3, 25, 5, 4, 60));
}

// Review 2026-11-08, cross-7: a spell is weighed against the caster's blow.
TEST(the_floor_is_the_weapon_for_the_spell_s_ap)
{
    CHECK_INT(ai_judgement_floor(25, 80, 5, 4, 100), 16);
    CHECK_INT(ai_judgement_floor(25, 80, 4, 6, 100), 29);
    CHECK_INT(ai_judgement_floor(3, 25, 5, 4, 60), 10); // a mage's fists
    CHECK_INT(ai_judgement_floor(0, 80, 5, 4, 100), 10); // out of reach
    CHECK_INT(ai_judgement_floor(25, 80, 0, 4, 100), 20);
    CHECK_INT(ai_judgement_floor(15, 80, 4, 4, 8), 22); // the sword's kill, on a spell's scale
}

static AiJudgedSpell judged(int value, int score, int cost, int ap_cost, int floor, bool attack)
{
    AiJudgedSpell c = { 0 };

    c.value = value;
    c.score = score;
    c.cost = cost;
    c.ap_cost = ap_cost;
    c.floor = floor;
    c.attack = attack;
    return c;
}

// Review 2026-11-08, casting-1: the reserve never changed a choice.
TEST(rule_2_changes_the_pick)
{
    AiJudgedSpell cands[2];
    int reserve;

    cands[0] = judged(150, 1000, 15, 4, 10, true); // Fireflash
    cands[1] = judged(960, 1600, 50, 6, 10, false); // Call Fire Elemental
    CHECK_INT(ai_judgement_pick(cands, 2, 80, -1, &reserve), 0);
    CHECK_INT(reserve, 45);
    CHECK_INT(cands[1].code, 21);

    cands[0] = judged(150, 1000, 15, 4, 10, true);
    cands[1] = judged(960, 1600, 50, 6, 10, false);
    CHECK_INT(ai_judgement_pick(cands, 2, 200, -1, &reserve), 1);
    CHECK_INT(cands[0].code, 20);
}

TEST(an_attack_under_its_floor_keeps_no_reserve)
{
    AiJudgedSpell cands[2];
    int reserve;

    cands[0] = judged(9, 300, 3, 4, 10, true); // Harm, worth too little
    cands[1] = judged(20, 200, 10, 5, 10, false); // a buff
    CHECK_INT(ai_judgement_pick(cands, 2, 15, -1, &reserve), 1);
    CHECK_INT(cands[0].code, 23);
    CHECK_INT(reserve, 0);
}

TEST(a_free_scroll_is_not_kept_back)
{
    AiJudgedSpell cands[2];
    int reserve;

    cands[0] = judged(150, 1000, 15, 4, 10, true); // Fireflash
    cands[1] = judged(20, 133, 0, 4, 10, false); // a scroll
    CHECK_INT(ai_judgement_pick(cands, 2, 30, -1, &reserve), 0);
    CHECK_INT(reserve, 45);
    CHECK_INT(cands[1].code, 20);
}

TEST(a_spell_below_the_weapon_is_not_cast)
{
    AiJudgedSpell cands[1];
    int reserve;

    cands[0] = judged(12, 800, 15, 4, 16, true);
    CHECK_INT(ai_judgement_pick(cands, 1, 100, -1, &reserve), -1);
    CHECK_INT(cands[0].code, 23);
    CHECK_INT(reserve, 0);
}

TEST(the_turn_rule_protects_only_an_attack_worth_casting)
{
    AiJudgedSpell cands[2];
    int reserve;

    cands[0] = judged(14, 466, 3, 4, 16, true); // Harm, under its sword
    cands[1] = judged(12, 120, 10, 5, 10, false); // a buff
    CHECK_INT(ai_judgement_pick(cands, 2, 100, 7, &reserve), 1);
    CHECK_INT(cands[0].code, 23);

    cands[0] = judged(60, 400, 15, 5, 10, true); // Fireflash
    cands[1] = judged(40, 500, 5, 5, 10, false); // a buff that strands it
    CHECK_INT(ai_judgement_pick(cands, 2, 200, 7, &reserve), 0);
    CHECK_INT(cands[1].code, 22);

    // Another attack is never held back for the best one: only a buff or
    // summon waits (the mutation check of 2026-11-09).
    cands[0] = judged(60, 400, 15, 5, 10, true); // Fireflash
    cands[1] = judged(40, 300, 10, 5, 10, true); // a lesser attack
    CHECK_INT(ai_judgement_pick(cands, 2, 200, 7, &reserve), 0);
    CHECK_INT(cands[1].code, 20);
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

// Stun's stun is in its [End] damage (casting-2).
#define STUN_SPELL 23

TEST(a_spell_s_stun_is_found)
{
    MagicTechComponentInfo stun = { 0 };
    MagicTechComponentInfo burn = { 0 };

    magictech_spells = (MagicTechInfo*)calloc(MT_SPELL_COUNT, sizeof(MagicTechInfo));
    stun.type = MTC_DAMAGE;
    stun.data.damage.damage_flags = CDF_STUN;
    burn.type = MTC_DAMAGE;
    burn.data.damage.damage_min = 5;
    burn.data.damage.damage_max = 9;
    magictech_spells[STUN_SPELL].components[MAGICTECH_ACTION_END].cnt = 1;
    magictech_spells[STUN_SPELL].components[MAGICTECH_ACTION_END].entries = &stun;
    magictech_spells[CALLBACK_DAMAGE_SPELL].components[MAGICTECH_ACTION_BEGIN].cnt = 1;
    magictech_spells[CALLBACK_DAMAGE_SPELL].components[MAGICTECH_ACTION_BEGIN].entries = &burn;

    CHECK((magictech_ai_damage_flags(STUN_SPELL) & CDF_STUN) != 0);
    CHECK((magictech_ai_damage_flags(CALLBACK_DAMAGE_SPELL) & CDF_STUN) == 0);
    stun.data.damage.damage_flags = CDF_DROP_WEAPON;
    CHECK(magictech_ai_damage_flags(STUN_SPELL) == CDF_DROP_WEAPON);

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
    RUN(an_area_spell_counts_the_allies_it_does_not_spare);
    RUN(the_budget_keeps_back_the_best_attacks);
    RUN(a_heal_keeps_to_the_reserve_unless_the_patient_is_dying);
    RUN(a_disable_is_worth_the_damage_it_denies);
    RUN(a_hold_lasts_its_turns);
    RUN(the_mental_mage_stuns_the_warrior);
    RUN(the_floor_is_the_weapon_for_the_spell_s_ap);
    RUN(rule_2_changes_the_pick);
    RUN(an_attack_under_its_floor_keeps_no_reserve);
    RUN(a_free_scroll_is_not_kept_back);
    RUN(a_spell_below_the_weapon_is_not_cast);
    RUN(the_turn_rule_protects_only_an_attack_worth_casting);
    RUN(a_spell_s_stun_is_found);
    RUN(a_turn_is_not_spent_on_a_buff_that_strands_the_attack);
    RUN(a_spell_s_callback_damage_is_counted);
}
